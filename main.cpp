
#include "jack.hpp"


#include <gtkmm.h>
#include <gtkmm/accelmap.h>
#include <glibmm/optioncontext.h>
#include <cassert>
#include <stdio.h>
#include <iostream>
#include <string>

#include "model.hpp"
#include "patternview.hpp"
#include "songview.hpp"
#include "measure.hpp"
#include "trackview.hpp"
#include "player.hpp"

#include "jsong.hpp"
#include "ring_buffer.hpp"

#include "jacker_config.hpp"

namespace Jacker {

const char AccelPathPlay[] = "<Jacker>/Transport/Play";
const char AccelPathStop[] = "<Jacker>/Transport/Stop";
const char AccelPathPatternView[] = "<Jacker>/View/Pattern";
const char AccelPathTrackView[] = "<Jacker>/View/Song";
const char AccelPathSave[] = "<Jacker>/File/Save";
const char AccelPathOpen[] = "<Jacker>/File/Open";

class JackPlayer : public Jack::Client,
                   public Player {
public:
    enum ThreadMessageType {
        MsgPlay = 0,
        MsgStop = 1,
        MsgSeek = 2,
    };
        
    struct ThreadMessage {
        ThreadMessageType type;
        int position;
    };
    
    RingBuffer<ThreadMessage> thread_messages;
   
    Jack::MIDIPort *midi_inp;
    Jack::MIDIPort *midi_omni_out;
    typedef std::vector<Jack::MIDIPort *> MIDIPortArray;

    MIDIPortArray midi_ports;
    bool defunct;

    bool enable_sync;
    bool waiting_for_sync;

    JackPlayer() : Jack::Client("jacker") {
        thread_messages.resize(100);
        
        enable_sync = false;
        waiting_for_sync = false;
        defunct = false;
        midi_inp = new Jack::MIDIPort(
            *this, "control", Jack::MIDIPort::IsInput);
        midi_omni_out = new Jack::MIDIPort(
            *this, "omni", Jack::MIDIPort::IsOutput);
        for (int i = 0; i < MaxPorts; ++i) {
            char name[32];
            sprintf(name, "port-%i", i);
            midi_ports.push_back(new Jack::MIDIPort(
                *this, name, Jack::MIDIPort::IsOutput));
        }
    }
    
    ~JackPlayer() {
        for (size_t i = 0; i < midi_ports.size(); ++i) {
            delete midi_ports[i];
        }
        midi_ports.clear();
        delete midi_omni_out;
        delete midi_inp;
    }
    
    void play() {
        if (enable_sync) {
            transport_start();
            return;
        }
        Player::play();
    }
    
    void seek(int frame) {
        if (enable_sync) {
            double samples = 
                (double)frame * (sample_rate * 60.0) / (double)(model->beats_per_minute * model->frames_per_beat);
            transport_locate((int)(samples+0.5));
            return;
        }
        Player::seek(frame);
    }
    
    void stop() {
        if (enable_sync) {
            transport_stop();
            return;
        }
        Player::stop();
    }
    
    void handle_thread_message(ThreadMessage &msg) {
        switch(msg.type) {
            case MsgPlay : {
                printf("SYNC: play\n");
                Player::play();
            } break;
            case MsgStop : {
                printf("SYNC: stop\n");
                Player::stop();
            } break;
            case MsgSeek : {
                printf("SYNC: seek to %i\n", msg.position);
                Player::seek(msg.position);
            } break;
            default: break;
        }
    }
    
    void mix() {
        ThreadMessage msg;
        while (!thread_messages.empty()) {
            msg = thread_messages.peek();
            handle_thread_message(msg);
            thread_messages.pop();
        }

        Player::mix();
    }
    
    virtual void on_sample_rate(Jack::NFrames nframes) {
        set_sample_rate((int)nframes);
        reset();
    }
    
    virtual void on_message(const Message &msg) {
        //printf("msg: CH%i 0x%x %i %i\n", msg.channel+1, msg.command, msg.data1, msg.data2);
        int offset = (int)(msg.timestamp>>32L);
        midi_omni_out->write_event(offset, msg);
        midi_ports[msg.port]->write_event(offset, msg);
    }
    
    int get_frame_from_position(const Jack::Position &pos) {
        /*
        if (pos.valid & JackPositionBBT) {
            int result = model->beats_per_bar * model->frames_per_beat * (pos.bar-1);
            result += model->frames_per_beat * (pos.beat-1);
            result += pos.tick * model->frames_per_beat / pos.ticks_per_beat;
            return result;
        }
        */
        return (int)(((double)model->beats_per_minute * pos.frame / (sample_rate * 60.0))+0.5) * 
            model->frames_per_beat;
        //return -1;
    }
    
    virtual bool on_sync(Jack::TransportState state, const Jack::Position &pos) {
        if (!enable_sync) {
            waiting_for_sync = false;
            return true;
        }
        
        bool r_playing = false;
        
        switch(state) {
            case JackTransportStarting:
            {
                if (waiting_for_sync) {
                    if (thread_messages.empty()) {
                        printf("finished waiting for sync\n");
                        waiting_for_sync = false;
                        return true;
                    } else {
                        //printf("waiting for sync\n");
                        return false;
                    }
                }
                waiting_for_sync = true;
                r_playing = true;
            } break;
            case JackTransportStopped:
            {
                waiting_for_sync = false;
                r_playing = false;
            } break;
            default: break;
        }
        
        int r_position = get_frame_from_position(pos);
        if (r_position != get_position()) {
            ThreadMessage msg;
            msg.type = MsgSeek;
            msg.position = r_position;
            thread_messages.push(msg);
        }
        
        if (r_playing != is_playing()) {
            ThreadMessage msg;
            msg.type = (r_playing?MsgPlay:MsgStop);
            thread_messages.push(msg);            
        }
        return false;
    }
    
    virtual void on_process(Jack::NFrames size) {
        if (enable_sync) {
            Jack::Position tpos;
            memset(&tpos, 0, sizeof(tpos));
            Jack::TransportState tstate = transport_query(&tpos);
            switch(tstate) {
                case JackTransportStopped: {
                    if (is_playing() && thread_messages.empty()) {
                        ThreadMessage msg;
                        msg.type = MsgStop;
                        thread_messages.push(msg);
                    }
                } break;
                default:
                    break;
            }
        }
        
        midi_omni_out->clear_buffer();
        for (MIDIPortArray::iterator iter = midi_ports.begin();
            iter != midi_ports.end(); ++iter) {
            (*iter)->clear_buffer();
        }
        
        for (Jack::NFrames i = 0; i < midi_inp->get_event_count(); ++i) {
            MIDI::Message ctrl_msg;
            if (midi_inp->get_event(ctrl_msg, NULL, i)) {
                ctrl_msg.channel = model->midi_control_channel;
                midi_omni_out->write_event(0, ctrl_msg);
                midi_ports[model->midi_control_port]->write_event(0, ctrl_msg);
            }
        }

        process_messages((int)size);
    }
    
    virtual void on_shutdown() {
        defunct = true;
    }
};

class App {
public:
    Gtk::Main kit;
    Model model;

    Glib::RefPtr<Gtk::Builder> builder;

    Glib::RefPtr<Gtk::Adjustment> bpm_range;
    Glib::RefPtr<Gtk::Adjustment> bpb_range;
    Glib::RefPtr<Gtk::Adjustment> fpb_range;

    Glib::RefPtr<Gtk::AccelGroup> accel_group;
    Glib::RefPtr<Gtk::ToggleAction> sync_action;

    Gtk::Window* window;
    PatternView *pattern_view;
    MeasureView *pattern_measure;
    Gtk::Entry *play_frames;

    SongView *song_view;
    MeasureView *song_measure;
    Gtk::Menu *song_view_menu;
    TrackView *track_view;

    Gtk::Statusbar *statusbar;
    Gtk::HBox *pattern_navigation;
    Gtk::Label *pattern_frame;
    Gtk::Label *pattern_column;
    Gtk::Label *pattern_value;

    Gtk::Notebook *view_notebook;
    Glib::OptionContext options;

    sigc::connection mix_timer;

    std::string filepath;
    JackPlayer *player;
    
    enum NotebookPages {
        PageSongView = 0,
        PagePatternView,
    };

    App(int argc, char **argv)
        : kit(argc,argv) {
        player = NULL;
        pattern_view = NULL;
        song_view = NULL;
        song_measure = NULL;
        pattern_measure = NULL;
        play_frames = NULL;
        song_view_menu = NULL;
        view_notebook = NULL;
        track_view = NULL;
        statusbar = NULL;
        pattern_navigation = NULL;
        pattern_frame = NULL;
        pattern_column = NULL;
        pattern_value = NULL;
    }
    
    bool parse_options(int argc, char **argv) {
        options.set_help_enabled(true);
        
        bool result = false;
        try {
            result = options.parse(argc, argv);
        } catch (Glib::OptionError) {
            printf("Unrecognized option.\n");
        }
        
        if (result) {
            // find filename
            for (int i = 0; i < argc; ++i) {
                if (!i)
                    continue;
                if (argv[i][0] == '-') // skip option
                    continue;
                // schedule a file to be loaded
                sigc::slot<bool> load_file_slot = sigc::bind(
                    sigc::mem_fun(*this, &App::load_startup_file), argv[i]);
                Glib::signal_idle().connect(load_file_slot);
                return true;
            }
        }
        
        return result;
    }
    
    ~App() {
        assert(!player);
    }
    
    bool load_startup_file(const std::string &path) {
        if (!load_song(path.c_str())) {
            printf("Error loading %s.\n", path.c_str());
            return false;
        }
        model_changed();
        return false;
    }
    
    std::string get_filepath() {
        return this->filepath;
    }
    
    void set_filepath(const std::string &filepath) {
        if (this->filepath == filepath)
            return;
        this->filepath = filepath;
        update_title();
    }
    
    void update_title() {
        assert(window);
        char title[256];
        if (filepath.empty()) {
            sprintf(title, "(Untitled) - Jacker");
        } else {
            std::string name = 
            #if defined(WIN32)
                filepath.substr(filepath.find_last_of("\\") + 1);
            #else // unix
                filepath.substr(filepath.find_last_of("/") + 1);
            #endif
            sprintf(title, "%s - Jacker", name.c_str());
        }
        window->set_title(title);
    }
    
    void on_play_action() {
        init_player();
        if (!player)
            return;
        if (!player->is_playing()) {
            player->play();
        } else if (model.enable_loop) {
            player->seek(model.loop.get_begin());
        }
    }
    
    void on_stop_action() {
        if (!player)
            return;
        if (player->is_playing()) {
            player->stop();
        } else if (model.enable_loop && 
                (player->get_position() > model.loop.get_begin())) {
            player->seek(model.loop.get_begin());
        } else {
            player->seek(0);
        }
    }
    
    void add_dialog_filters(Gtk::FileChooserDialog &dialog) {
        Gtk::FileFilter filter_song;
        filter_song.set_name("Jacker songs");
        filter_song.add_pattern("*.jsong");
        dialog.add_filter(filter_song);

        Gtk::FileFilter filter_any;
        filter_any.set_name("Any files");
        filter_any.add_pattern("*");
        dialog.add_filter(filter_any);
    }
    
    void model_changed() {
        pattern_view->reset();
        song_view->reset();
        pattern_view->set_song_event(model.song.end());
        song_view->set_loop(model.loop);
        update_measures();
        track_view->update_tracks();
        all_views_changed();
    }
    
    void on_new_action() {
        if (player) {
            player->stop();
            player->seek(0);
        }
        set_filepath("");
        model.reset();
        model_changed();
    }
    
    void on_quit_action() {
        kit.quit();
    }
    
    void on_save_as_action() {
        Gtk::FileChooserDialog dialog("Save Song",
            Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.set_transient_for(*window);
        dialog.set_do_overwrite_confirmation();

        dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
        dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

        add_dialog_filters(dialog);

        dialog.set_filename(get_filepath());
        
        int result = dialog.run();
        if (result != Gtk::RESPONSE_OK)
            return;
        
        std::string filename = dialog.get_filename();
        if (filename.substr(filename.find_last_of(".") + 1) != "jsong") {
            filename += ".jsong";
        }
        
        save_song(filename);
    }
    
    void on_save_action() {
        if (get_filepath().empty()) {
            on_save_as_action();
            return;
        }
        
        save_song(get_filepath());
    }
    
    void on_open_action() {
        Gtk::FileChooserDialog dialog("Open Song",
            Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.set_transient_for(*window);

        dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
        dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

        add_dialog_filters(dialog);
        
        dialog.set_filename(get_filepath());

        int result = dialog.run();
        if (result != Gtk::RESPONSE_OK)
            return;
        
        load_song(dialog.get_filename());
        model_changed();
    }
    
    void save_song(const std::string &filename) {
        set_filepath(filename);
        write_jsong(model, filename);
    }
    
    bool load_song(const std::string &filename) {
        if (player) {
            player->stop();
            player->seek(0);
        }
        try {
            if (!read_jsong(model, filename))
                return false;
            set_filepath(filename);
        } catch(...) {
            return false;
        }
        return true;
    }
    
    void all_views_changed() {
        pattern_view->invalidate();
        song_view->invalidate();
        song_measure->invalidate();
        pattern_measure->invalidate();
    }
    
    void on_about_action() {
        Gtk::AboutDialog *dialog;
        builder->get_widget("aboutdialog", dialog);
        assert(dialog);
        dialog->run();
        dialog->hide();
    }
    
    Glib::RefPtr<Gtk::Adjustment> get_adjustment(const std::string &name) {
        Glib::RefPtr<Gtk::Adjustment> adjustment =
            Glib::RefPtr<Gtk::Adjustment>::cast_static(
                builder->get_object(name));
        assert(adjustment);
        return adjustment;
    }

    template<typename T>
    Glib::RefPtr<Gtk::ToggleAction> connect_toggle_action(const std::string &name, 
                        const T &signal,
                        const Glib::ustring& accel_path="") {
        Glib::RefPtr<Gtk::ToggleAction> action =
            Glib::RefPtr<Gtk::ToggleAction>::cast_static(
                builder->get_object(name));
        assert(action);
        if (!accel_path.empty()) {
            action->set_accel_path(accel_path);
            action->set_accel_group(accel_group);       
            action->connect_accelerator();
        }
        action->signal_toggled().connect(signal);
        return action;
    }
    
    template<typename T>
    void connect_action(const std::string &name, 
                        const T &signal,
                        const Glib::ustring& accel_path="") {
        Glib::RefPtr<Gtk::Action> action =
            Glib::RefPtr<Gtk::Action>::cast_static(
                builder->get_object(name));
        assert(action);
        if (!accel_path.empty()) {
            action->set_accel_path(accel_path);
            action->set_accel_group(accel_group);       
            action->connect_accelerator();
        }
        action->signal_activate().connect(signal);
    }
    
    void on_show_pattern_action() {
        show_pattern_view();
    }
    
    void on_show_song_action() {
        show_song_view();
    }
    
    void on_sync_action() {
        player->enable_sync = sync_action->get_active();
    }
    
    void init_menu() {
        connect_action("new_action", sigc::mem_fun(*this, &App::on_new_action));
        connect_action("open_action", sigc::mem_fun(*this, &App::on_open_action),
            AccelPathOpen);
        connect_action("save_action", sigc::mem_fun(*this, &App::on_save_action),
            AccelPathSave);
        connect_action("save_as_action", sigc::mem_fun(*this, &App::on_save_as_action));
        connect_action("quit_action", sigc::mem_fun(*this, &App::on_quit_action));
        connect_action("about_action", sigc::mem_fun(*this, &App::on_about_action));
    
        connect_action("show_pattern_action", 
            sigc::mem_fun(*this, &App::on_show_pattern_action),
            AccelPathPatternView);
        connect_action("show_song_action", 
            sigc::mem_fun(*this, &App::on_show_song_action),
            AccelPathTrackView);
    
        sync_action = connect_toggle_action("sync_action", sigc::mem_fun(*this, &App::on_sync_action));
    }

    void on_bpm_changed() {
        model.beats_per_minute = int(bpm_range->get_value()+0.5);
        if (player)
            player->flush();
    }
    
    void on_bpb_changed() {
        model.beats_per_bar = int(bpb_range->get_value()+0.5);
        all_views_changed();
    }
    
    void on_fpb_changed() {
        model.frames_per_beat = int(fpb_range->get_value()+0.5);
        all_views_changed();
    }
    
    void update_measures() {
        bpm_range->set_value(model.beats_per_minute);
        bpb_range->set_value(model.beats_per_bar);
        fpb_range->set_value(model.frames_per_beat);
    }
    
    void init_transport() {
        connect_action("play_action", 
            sigc::mem_fun(*this, &App::on_play_action),
            AccelPathPlay);
        connect_action("stop_action", 
            sigc::mem_fun(*this, &App::on_stop_action),
            AccelPathStop);
        
        builder->get_widget("play_frames", play_frames);
        
        bpm_range = get_adjustment("bpm_range");
        bpm_range->signal_value_changed().connect(sigc::mem_fun(*this,
            &App::on_bpm_changed));
        bpb_range = get_adjustment("bpb_range");
        bpb_range->signal_value_changed().connect(sigc::mem_fun(*this,
            &App::on_bpb_changed));
        fpb_range = get_adjustment("fpb_range");
        fpb_range->signal_value_changed().connect(sigc::mem_fun(*this,
            &App::on_fpb_changed));
    }
    
    void init_model() {
    }
    
    void on_play_request(int frame) {
        if (!player)
            return;
        player->seek(frame);
        player->play();
    }
    
    void on_pattern_view_play_event(int track, const Pattern::Event &event) {
        if (!player)
            return;
        player->play_event(track, event);
    }
    
    void on_pattern_seek_request(int frame) {
    }
    
    void on_pattern_return_request() {
        show_song_view();
    }
    
    void on_pattern_view_navi_status_request(const std::string &pos,
        const std::string &column, const std::string &value) {
        pattern_frame->set_text(pos);
        pattern_column->set_text(column);
        pattern_value->set_text(value);
    }
    
    void init_pattern_view() {
        builder->get_widget_derived("pattern_view", pattern_view);
        assert(pattern_view);
        
        Gtk::HScrollbar *pattern_hscroll;
        builder->get_widget("pattern_hscroll", pattern_hscroll);
        Gtk::VScrollbar *pattern_vscroll;
        builder->get_widget("pattern_vscroll", pattern_vscroll);
        pattern_view->set_scroll_adjustments(
            pattern_hscroll->get_adjustment(), 
            pattern_vscroll->get_adjustment());
        
        pattern_view->set_model(model);
        
        pattern_view->signal_play_event_request().connect(
            sigc::mem_fun(*this, &App::on_pattern_view_play_event));
        pattern_view->signal_return_request().connect(
            sigc::mem_fun(*this, &App::on_pattern_return_request));
        pattern_view->signal_play_request().connect(
            sigc::mem_fun(*this, &App::on_play_request));
        pattern_view->signal_navigation_status_request().connect(
            sigc::mem_fun(*this, &App::on_pattern_view_navi_status_request));
        
        builder->get_widget_derived("pattern_measure", pattern_measure);
        assert(pattern_measure);
        pattern_measure->set_model(model);
        pattern_measure->set_adjustment(pattern_vscroll->get_adjustment());
        pattern_measure->signal_seek_request().connect(
            sigc::mem_fun(*this, &App::on_pattern_seek_request));
        pattern_measure->set_orientation(MeasureView::OrientationVertical);
        
        
        builder->get_widget("pattern_navigation", pattern_navigation);
        assert(pattern_navigation);
        builder->get_widget("pattern_frame", pattern_frame);
        assert(pattern_frame);
        builder->get_widget("pattern_column", pattern_column);
        assert(pattern_column);
        builder->get_widget("pattern_value", pattern_value);
        assert(pattern_value);
    }
    
    void show_pattern_view() {
        pattern_navigation->show_all();
        view_notebook->set_current_page(PagePatternView);
        pattern_view->grab_focus();
    }
    
    void show_song_view() {
        pattern_navigation->hide_all();
        view_notebook->set_current_page(PageSongView);
        song_view->grab_focus();
    }
    
    void on_song_view_edit_pattern(Song::iterator event) {
        pattern_view->set_song_event(event);
        show_pattern_view();
    }
    
    void on_song_view_context_menu(SongView *view, GdkEventButton* event) {
        song_view_menu->popup(event->button, event->time);
    }
    
    void on_seek_request(int frame) {
        if (!player)
            return;
        player->seek(frame);
    }
    
    void on_measure_loop_changed() {
        song_view->set_loop(model.loop);
    }
    
    void on_song_view_loop_changed() {
        song_measure->invalidate();
    }
    
    void on_song_view_pattern_erased(Song::iterator event) {
        if (pattern_view->get_song_event() == event) {
            pattern_view->set_song_event(model.song.end());
        }
    }
    
    void on_song_view_tracks_changed() {
        track_view->update_tracks();
    }
    
    void init_song_view() {
        builder->get_widget_derived("song_view", song_view);
        assert(song_view);
        
        Gtk::HScrollbar *song_hscroll;
        builder->get_widget("song_hscroll", song_hscroll);
        Gtk::VScrollbar *song_vscroll;
        builder->get_widget("song_vscroll", song_vscroll);
        song_view->set_scroll_adjustments(
            song_hscroll->get_adjustment(), 
            song_vscroll->get_adjustment());
        
        song_view->set_model(model);
        song_view->signal_pattern_edit_request().connect(
            sigc::mem_fun(*this, &App::on_song_view_edit_pattern));
        song_view->signal_context_menu().connect(
            sigc::mem_fun(*this, &App::on_song_view_context_menu));
        song_view->signal_loop_changed().connect(
            sigc::mem_fun(*this, &App::on_song_view_loop_changed));
        song_view->signal_play_request().connect(
            sigc::mem_fun(*this, &App::on_play_request));
        song_view->signal_pattern_erased().connect(
            sigc::mem_fun(*this, &App::on_song_view_pattern_erased));
        song_view->signal_tracks_changed().connect(
            sigc::mem_fun(*this, &App::on_song_view_tracks_changed));
        
        builder->get_widget("song_view_menu", song_view_menu);
        assert(song_view_menu);
        
        connect_action("add_track_action", 
            sigc::mem_fun(*song_view, &SongView::add_track));
            
        builder->get_widget_derived("song_measure", song_measure);
        assert(song_measure);
        song_measure->set_model(model);
        song_measure->set_adjustment(song_hscroll->get_adjustment());
        song_measure->signal_seek_request().connect(
            sigc::mem_fun(*this, &App::on_seek_request));
        song_measure->signal_loop_changed().connect(
            sigc::mem_fun(*this, &App::on_measure_loop_changed));
            
        builder->get_widget_derived("track_view", track_view);
        assert(track_view);
        track_view->set_model(model);        
        track_view->signal_mute_toggled().connect(
            sigc::mem_fun(*this, &App::on_track_view_mute_toggled));

    }
    
    void on_track_view_mute_toggled(int index, bool mute) {
        if (!player)
            return;
        if (mute) {
            player->stop_events(index);
        }
    }
    
    void init_timer() {
        sigc::slot<bool> mix_timer_slot = sigc::bind(
            sigc::mem_fun(*this, &App::mix), 0);
        mix_timer = Glib::signal_timeout().connect(mix_timer_slot,
            100);
    }
    
    void shutdown_player() {
        if (!player)
            return;
        if (player->is_created()) {
            player->deactivate();
            player->shutdown();
        }
        delete player;
        player = NULL;
    }
    
    void init_player() {
        if (player)
            return;
        player = new JackPlayer();
        player->set_model(model);
        if (!player->init()) {
            shutdown_player();
        }
    }
    
    void fix_menuitem_accelerator(const std::string &name) {
        Gtk::MenuItem *item;
        builder->get_widget(name, item);
        assert(item);
        Glib::RefPtr<Gtk::Action> action = item->get_action();
        if (!action)
            return;
        if (action->get_accel_path().empty())
            return;
        item->set_accel_path(action->get_accel_path());
    }
    
    void fix_accelerators() {
        fix_menuitem_accelerator("menuitem_new");
        fix_menuitem_accelerator("menuitem_open");
        fix_menuitem_accelerator("menuitem_save");
        fix_menuitem_accelerator("menuitem_save_as");
        fix_menuitem_accelerator("menuitem_quit");
        
        /*
        fix_menuitem_accelerator("menuitem_cut");
        fix_menuitem_accelerator("menuitem_copy");
        fix_menuitem_accelerator("menuitem_paste");
        fix_menuitem_accelerator("menuitem_delete");
        */
        
        fix_menuitem_accelerator("menuitem_song");
        fix_menuitem_accelerator("menuitem_pattern");
        
        fix_menuitem_accelerator("menuitem_about");
    }
    
    void on_view_notebook_switch_page(GtkNotebookPage *page, guint index) {
        if (index == PagePatternView) {
            pattern_navigation->show_all();
        } else {
            pattern_navigation->hide_all();
        }
    }
    
    void run() {
        init_player();       
        
        builder = Gtk::Builder::create_from_file(JACKER_SHARE_DIR"/jacker.glade");
        assert(builder);
        
        builder->get_widget("main", window);
        assert(window);
        
        Gtk::AccelMap::add_entry(AccelPathPatternView, GDK_F4, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathTrackView, GDK_F3, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathPlay, GDK_F5, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathStop, GDK_F8, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathOpen, GDK_o, Gdk::CONTROL_MASK);
        Gtk::AccelMap::add_entry(AccelPathSave, GDK_s, Gdk::CONTROL_MASK);
        
        accel_group =
            Glib::RefPtr<Gtk::AccelGroup>::cast_static(
                builder->get_object("accel_group"));
        assert(accel_group);
        
        builder->get_widget("view_notebook", view_notebook);
        assert(view_notebook);
        view_notebook->signal_switch_page().connect(sigc::mem_fun(
            *this, &App::on_view_notebook_switch_page));
        
        builder->get_widget("statusbar", statusbar);
        assert(statusbar);
        
        init_menu();
        init_transport();
        init_model();
        init_pattern_view();
        init_song_view();
        init_timer();
        
        fix_accelerators();
        
        update_title();        
        window->show_all();
        show_song_view();        
        on_new_action();       
        
        if (player)
            player->activate();
        
        kit.run(*window);
        
        mix_timer.disconnect();
        
        shutdown_player();
    }
    
    bool mix(int i) {
        // check if player is dead
        if (player && player->defunct) {
            shutdown_player();
        }
        
        int frame = 0;
        if (player) {
            player->mix();
            frame = player->get_position();
        }
        
        Measure measure;
        measure.set_frame(model, frame);
        play_frames->set_text(measure.get_string());
        
        song_view->set_play_position(frame);
        
        // find out if our pattern is currently playing
        bool found = false;
        Pattern *active_pattern = pattern_view->get_pattern();
        if (active_pattern) {
            Song::IterList events;
            model.song.find_events(frame, events);
            if (!events.empty()) {
                Song::IterList::iterator iter;
                for (iter = events.begin(); iter != events.end(); ++iter) {
                    Song::Event &evt = (*iter)->second;
                    if (evt.pattern == active_pattern) {
                        found = true;
                        pattern_view->set_play_position(frame - evt.frame);
                        break;
                    }
                }
            }
        }
        if (!found)
            pattern_view->set_play_position(-1);
        return true;
    }
};
    
} // namespace Jacker

int main(int argc, char **argv) {
    Jacker::App app(argc, argv);
    if (app.parse_options(argc, argv)) {
        app.run();
    }
    
    return 0;
}

#ifdef WIN32
int WINAPI WinMain(      
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    return main(__argc, __argv);
}
#endif
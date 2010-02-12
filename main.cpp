
#include "jack.hpp"


#include <gtkmm.h>
#include <gtkmm/accelmap.h>
#include <assert.h>
#include <stdio.h>

#include "model.hpp"
#include "patternview.hpp"
#include "trackview.hpp"
#include "measure.hpp"
#include "player.hpp"

#include "jsong.hpp"

namespace Jacker {

const char AccelPathPlay[] = "<Jacker>/Transport/Play";
const char AccelPathStop[] = "<Jacker>/Transport/Stop";
const char AccelPathPatternView[] = "<Jacker>/View/Pattern";
const char AccelPathTrackView[] = "<Jacker>/View/Tracks";
    
class JackPlayer : public Jack::Client,
                   public Player {
public:
    Jack::MIDIPort *midi_omni_out;
    bool defunct;

    JackPlayer() : Jack::Client("jacker") {
        defunct = false;
        midi_omni_out = new Jack::MIDIPort(
            *this, "omni", Jack::MIDIPort::IsOutput);
    }
    
    ~JackPlayer() {
        delete midi_omni_out;
    }
    
    virtual void on_sample_rate(Jack::NFrames nframes) {
        set_sample_rate((int)nframes);
        reset();
    }
    
    virtual void on_message(const Message &msg) {
        int offset = (int)(msg.timestamp>>32);
        midi_omni_out->write_event(offset, msg);
    }
    
    virtual void on_process(Jack::NFrames size) {
        midi_omni_out->clear_buffer();
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

    Gtk::Window* window;
    PatternView *pattern_view;
    MeasureView *pattern_measure;
    Gtk::Entry *play_frames;

    TrackView *track_view;
    MeasureView *track_measure;
    Gtk::Menu *trackview_menu;

    Gtk::Notebook *view_notebook;

    sigc::connection mix_timer;

    JackPlayer *player;

    enum NotebookPages {
        PageTrackView = 0,
        PagePatternView,
    };

    App(int argc, char **argv)
        : kit(argc,argv) {
        player = NULL;
        pattern_view = NULL;
        track_view = NULL;
        track_measure = NULL;
        pattern_measure = NULL;
        play_frames = NULL;
        trackview_menu = NULL;
        view_notebook = NULL;
    }
    
    ~App() {
        assert(!player);
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
    
    void on_save_action() {
        Gtk::FileChooserDialog dialog("Save Song",
            Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.set_transient_for(*window);
        dialog.set_do_overwrite_confirmation();

        dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
        dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

        add_dialog_filters(dialog);

        int result = dialog.run();
        if (result != Gtk::RESPONSE_OK)
            return;
        
        write_jsong(model, dialog.get_filename());
    }
    
    void on_open_action() {
        Gtk::FileChooserDialog dialog("Open Song",
            Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.set_transient_for(*window);

        dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
        dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

        add_dialog_filters(dialog);

        int result = dialog.run();
        if (result != Gtk::RESPONSE_OK)
            return;
        
        load_song(dialog.get_filename());
    }
    
    void load_song(const std::string &filename) {
        read_jsong(model, filename);
        
        track_view->invalidate();
        pattern_view->set_song_event(model.song.end());
        track_view->set_loop(model.loop);
        update_measures();
        all_views_changed();
    }
    
    void all_views_changed() {
        pattern_view->invalidate();
        track_view->invalidate();
        track_measure->invalidate();
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
    
    void on_show_tracks_action() {
        show_track_view();
    }
    
    void init_menu() {
        connect_action("save_action", sigc::mem_fun(*this, &App::on_save_action));
        connect_action("open_action", sigc::mem_fun(*this, &App::on_open_action));
        connect_action("about_action", sigc::mem_fun(*this, &App::on_about_action));
    
        connect_action("show_pattern_action", 
            sigc::mem_fun(*this, &App::on_show_pattern_action),
            AccelPathPatternView);
        connect_action("show_tracks_action", 
            sigc::mem_fun(*this, &App::on_show_tracks_action),
            AccelPathTrackView);
    
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
    
    void on_pattern_view_play_event(const Pattern::Event &event) {
        if (!player)
            return;
        player->play_event(event);
    }
    
    void on_pattern_seek_request(int frame) {
    }
    
    void on_pattern_return_request() {
        show_track_view();
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
        
        builder->get_widget_derived("pattern_measure", pattern_measure);
        assert(pattern_measure);
        pattern_measure->set_model(model);
        pattern_measure->set_adjustment(pattern_vscroll->get_adjustment());
        pattern_measure->signal_seek_request().connect(
            sigc::mem_fun(*this, &App::on_pattern_seek_request));
        pattern_measure->set_orientation(MeasureView::OrientationVertical);
    }
    
    void show_pattern_view() {
        view_notebook->set_current_page(PagePatternView);
        pattern_view->grab_focus();
    }
    
    void show_track_view() {
        view_notebook->set_current_page(PageTrackView);
        track_view->grab_focus();
    }
    
    void on_track_view_edit_pattern(Song::iterator event) {
        pattern_view->set_song_event(event);
        show_pattern_view();
    }
    
    void on_track_view_context_menu(TrackView *view, GdkEventButton* event) {
        trackview_menu->popup(event->button, event->time);
    }
    
    void on_seek_request(int frame) {
        if (!player)
            return;
        player->seek(frame);
    }
    
    void on_measure_loop_changed() {
        track_view->set_loop(model.loop);
    }
    
    void on_track_view_loop_changed() {
        track_measure->invalidate();
    }
    
    void on_track_view_pattern_erased(Song::iterator event) {
        if (pattern_view->get_song_event() == event) {
            pattern_view->set_song_event(model.song.end());
        }
    }
    
    void init_track_view() {
        builder->get_widget_derived("track_view", track_view);
        assert(track_view);
        
        Gtk::HScrollbar *track_hscroll;
        builder->get_widget("track_hscroll", track_hscroll);
        Gtk::VScrollbar *track_vscroll;
        builder->get_widget("track_vscroll", track_vscroll);
        track_view->set_scroll_adjustments(
            track_hscroll->get_adjustment(), 
            track_vscroll->get_adjustment());
        
        track_view->set_model(model);
        track_view->signal_pattern_edit_request().connect(
            sigc::mem_fun(*this, &App::on_track_view_edit_pattern));
        track_view->signal_context_menu().connect(
            sigc::mem_fun(*this, &App::on_track_view_context_menu));
        track_view->signal_loop_changed().connect(
            sigc::mem_fun(*this, &App::on_track_view_loop_changed));
        track_view->signal_play_request().connect(
            sigc::mem_fun(*this, &App::on_play_request));
        track_view->signal_pattern_erased().connect(
            sigc::mem_fun(*this, &App::on_track_view_pattern_erased));
        
        builder->get_widget("trackview_menu", trackview_menu);
        assert(trackview_menu);
        
        connect_action("add_track_action", 
            sigc::mem_fun(*track_view, &TrackView::add_track));
            
        builder->get_widget_derived("track_measure", track_measure);
        assert(track_measure);
        track_measure->set_model(model);
        track_measure->set_adjustment(track_hscroll->get_adjustment());
        track_measure->signal_seek_request().connect(
            sigc::mem_fun(*this, &App::on_seek_request));
        track_measure->signal_loop_changed().connect(
            sigc::mem_fun(*this, &App::on_measure_loop_changed));
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
    
    void run() {
        init_player();       
        
        builder = Gtk::Builder::create_from_file("jacker.glade");
        assert(builder);
        
        builder->get_widget("main", window);
        assert(window);
        
        Gtk::AccelMap::add_entry(AccelPathPatternView, GDK_F4, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathTrackView, GDK_F3, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathPlay, GDK_F5, Gdk::ModifierType());
        Gtk::AccelMap::add_entry(AccelPathStop, GDK_F8, Gdk::ModifierType());

        accel_group = Gtk::AccelGroup::create();
        window->add_accel_group(accel_group);
                
        builder->get_widget("view_notebook", view_notebook);
        assert(view_notebook);
        
        init_menu();
        init_transport();
        init_model();
        init_pattern_view();
        init_track_view();
        init_timer();

        load_song("dump.jsong");
        
        window->show_all();
        show_track_view();
        
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
        
        track_view->set_play_position(frame);
        
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
    app.run();
    
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
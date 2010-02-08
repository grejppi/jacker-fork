
#include "jack.hpp"


#include <gtkmm.h>
#include <assert.h>
#include <stdio.h>

#include "model.hpp"
#include "patternview.hpp"
#include "trackview.hpp"
#include "player.hpp"

#include "jsong.hpp"

namespace Jacker {

class JackPlayer : public Jack::Client,
                   public Player {
public:
    Jack::MIDIPort *midi_omni_out;

    JackPlayer() : Jack::Client("jacker") {
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

};

class App {
public:
    Gtk::Main kit;
    Model model;

    Glib::RefPtr<Gtk::Builder> builder;

    Gtk::Window* window;
    PatternView *pattern_view;
    Gtk::Entry *play_frames;

    TrackView *track_view;
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
        player->play();
    }
    
    void on_stop_action() {
        if (!player)
            return;
        player->stop();
        player->set_position(0);
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
        
        pattern_view->set_pattern(NULL);
        track_view->invalidate();
    }
    
    void on_about_action() {
        Gtk::AboutDialog *dialog;
        builder->get_widget("aboutdialog", dialog);
        assert(dialog);
        dialog->run();
        dialog->hide();
    }
    
    template<typename T>
    void connect_action(const std::string &name, const T &signal) {
        Glib::RefPtr<Gtk::Action> action =
            Glib::RefPtr<Gtk::Action>::cast_static(
                builder->get_object(name));
        assert(action);
        action->signal_activate().connect(signal);
    }
    
    void init_menu() {
        connect_action("save_action", sigc::mem_fun(*this, &App::on_save_action));
        connect_action("open_action", sigc::mem_fun(*this, &App::on_open_action));
        connect_action("about_action", sigc::mem_fun(*this, &App::on_about_action));
    }
    
    void init_transport() {
        connect_action("play_action", sigc::mem_fun(*this, &App::on_play_action));
        connect_action("stop_action", sigc::mem_fun(*this, &App::on_stop_action));
        
        builder->get_widget("play_frames", play_frames);
    }
    
    void init_model() {
    }
    
    void on_pattern_view_play_event(const Pattern::Event &event) {
        if (!player)
            return;
        player->play_event(event);
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
    }
    
    void on_track_view_edit_pattern(Pattern *pattern) {
        view_notebook->set_current_page(PagePatternView);
        pattern_view->set_pattern(pattern);
    }
    
    void on_track_view_context_menu(TrackView *view, GdkEventButton* event) {
        trackview_menu->popup(event->button, event->time);
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
        
        builder->get_widget("trackview_menu", trackview_menu);
        assert(trackview_menu);
        
        connect_action("add_track_action", 
            sigc::mem_fun(*track_view, &TrackView::add_track));
    }
    
    void init_timer() {
        sigc::slot<bool> mix_timer_slot = sigc::bind(
            sigc::mem_fun(*this, &App::mix), 0);
        mix_timer = Glib::signal_timeout().connect(mix_timer_slot,
            100);
    }
    
    void init_player() {
        if (player)
            return;
        player = new JackPlayer();
        player->set_model(model);
        if (!player->init()) {
            delete player;
            player = NULL;
        }
    }
    
    void run() {
        init_player();       
        
        builder = Gtk::Builder::create_from_file("jacker.glade");
        assert(builder);
        
        builder->get_widget("main", window);
        assert(window);
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
        
        if (player)
            player->activate();
        
        kit.run(*window);
        
        mix_timer.disconnect();
            
        if (player) {
            player->deactivate();
            player->shutdown();
            delete player;
            player = NULL;
        }
    }
    
    bool mix(int i) {
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
            TrackEventRefList refs;
            model.find_events(frame, refs);
            if (!refs.empty()) {
                TrackEventRefList::iterator iter;
                for (iter = refs.begin(); iter != refs.end(); ++iter) {
                    TrackEvent &evt = iter->iter->second;
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
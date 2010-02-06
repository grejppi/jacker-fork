
#include "jack.hpp"


#include <gtkmm.h>
#include <assert.h>
#include <stdio.h>

#include "model.hpp"
#include "patternview.hpp"
#include "seqview.hpp"
#include "player.hpp"

#include "jsong.hpp"

namespace Jacker {

class App : public Jack::Client {
public:
    Gtk::Main kit;
    Jack::MIDIPort *midi_omni_out;
    Model model;

    Glib::RefPtr<Gtk::Builder> builder;

    Gtk::Window* window;
    PatternView *pattern_view;
    SeqView *seq_view;

    sigc::connection mix_timer;

    Player player;

    App(int argc, char **argv)
        : Jack::Client("jacker"),kit(argc,argv) {
        midi_omni_out = new Jack::MIDIPort(*this, "omni", Jack::MIDIPort::IsOutput);
        pattern_view = NULL;
        seq_view = NULL;
        player.set_model(model);
    }
    
    ~App() {
        delete midi_omni_out;
    }
    
    void on_play_action() {
        player.play();
    }
    
    void on_stop_action() {
        player.stop();
        player.set_position(0);
    }
    
    void on_save_action() {
        Gtk::FileChooserDialog dialog("Save Song",
            Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.set_transient_for(*window);
        dialog.set_do_overwrite_confirmation();

        dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
        dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

        Gtk::FileFilter filter_song;
        filter_song.set_name("Jacker songs");
        filter_song.add_pattern("*.jsong");
        dialog.add_filter(filter_song);

        Gtk::FileFilter filter_any;
        filter_any.set_name("Any files");
        filter_any.add_pattern("*");
        dialog.add_filter(filter_any);

        int result = dialog.run();
        if (result != Gtk::RESPONSE_OK)
            return;
        
        write_jsong(model, dialog.get_filename());
    }
    
    void init_transport() {
        Glib::RefPtr<Gtk::Action> play_action =
            Glib::RefPtr<Gtk::Action>::cast_static(
                builder->get_object("play_action"));
        assert(play_action);
        play_action->signal_activate().connect(
            sigc::mem_fun(*this, &App::on_play_action));
        
        Glib::RefPtr<Gtk::Action> stop_action =
            Glib::RefPtr<Gtk::Action>::cast_static(
                builder->get_object("stop_action"));
        assert(stop_action);
        stop_action->signal_activate().connect(
            sigc::mem_fun(*this, &App::on_stop_action));
        
        Glib::RefPtr<Gtk::Action> save_action =
            Glib::RefPtr<Gtk::Action>::cast_static(
                builder->get_object("save_action"));
        assert(save_action);
        save_action->signal_activate().connect(
            sigc::mem_fun(*this, &App::on_save_action));
    }
    
    void init_model() {
        bool result = read_jsong(model, "dump.jsong");
        assert(result);
        /*
        Pattern &pattern = model.new_pattern();
        pattern.name = "test";
        pattern.set_length(64);
        pattern.set_channel_count(4);
        
        for (int i = 0; i < 64; i += 8) {
            pattern.add_event(i,0,ParamNote,NOTE(C,4));
            pattern.add_event(i,3,ParamNote,NOTE(G,3));
        }
        
        int i = 0;
        
        while (i < 64) {
            switch(i%12) {
                case 0:
                {
                    pattern.add_event(i,1,ParamVolume,0x7f);
                    pattern.add_event(i,1,ParamNote,NOTE(F,6));
                } break;
                case 6:
                {
                    pattern.add_event(i,1,ParamVolume,0x2f);
                    pattern.add_event(i,1,ParamNote,NOTE(Ds,6));
                } break;
                case 2:
                case 8:
                {
                    pattern.add_event(i,1,ParamVolume,0x6f);
                    pattern.add_event(i,1,ParamNote,NOTE(G,6));
                } break;
                case 4:
                case 10:
                {
                    pattern.add_event(i,1,ParamVolume,0x5f);
                    pattern.add_event(i,1,ParamNote,NOTE(C,7));
                } break;
                default: break;
            }
            i++;
        }
        
        for (int i = 0; i < 5; ++i) {
            Track &track = model.new_track();
            track.name = "test";
            track.add_event(i*pattern.get_length(),pattern);
            //track.add_event((i+2)*pattern.get_length(),pattern);
        }
        */
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
        
        if (model.patterns.size())
            pattern_view->select_pattern(model, *(*model.patterns.begin()));
    }
    
    void init_track_view() {
        builder->get_widget_derived("seq_view", seq_view);
        assert(seq_view);
        
        Gtk::HScrollbar *seq_hscroll;
        builder->get_widget("seq_hscroll", seq_hscroll);
        Gtk::VScrollbar *seq_vscroll;
        builder->get_widget("seq_vscroll", seq_vscroll);
        seq_view->set_scroll_adjustments(
            seq_hscroll->get_adjustment(), 
            seq_vscroll->get_adjustment());
        
        seq_view->set_model(model);
    }
    
    void init_player() {
        sigc::slot<bool> mix_timer_slot = sigc::bind(
            sigc::mem_fun(*this, &App::mix), 0);
        mix_timer = Glib::signal_timeout().connect(mix_timer_slot,
            100);
    }
    
    void run() {
        if (!init())
            return;
        builder = Gtk::Builder::create_from_file("jacker.glade");
        
        builder->get_widget("main", window);
        
        init_transport();
        init_model();
        init_pattern_view();
        init_track_view();
        init_player();
        
        window->show_all();
        
        activate();
        
        kit.run(*window);
        
        mix_timer.disconnect();
            
        deactivate();
        shutdown();
    }
    
    bool mix(int i) {
        player.mix();
        int frame = player.get_position();
        seq_view->set_play_position(frame);
        
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
    
    virtual void on_sample_rate(Jack::NFrames nframes) {
        player.set_sample_rate((int)nframes);
        player.reset();
    }
    
    virtual void on_process(Jack::NFrames size) {
        midi_omni_out->clear_buffer();
        Player::Message msg;
        int s = (int)size;
        int offset = 0;
        while (s) {
            int delta = player.process(s,msg);
            if (s == delta) // nothing left to do
                break;
            s -= delta;
            offset += delta;
            midi_omni_out->write_event(offset, msg);
        }
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

#include "jack.hpp"


#include <gtkmm.h>
#include <assert.h>
#include <stdio.h>

#include "model.hpp"
#include "patternview.hpp"
#include "seqview.hpp"

#define USE_JACK_CLIENT

namespace Jacker {

class App : public Jack::Client {
public:
    Gtk::Main kit;
    Jack::MIDIPort *midi_omni_out;
    Model model;

    Glib::RefPtr<Gtk::Builder> builder;

    PatternView *pattern_view;
    SeqView *seq_view;

    sigc::connection mix_timer;

    Player player;

    App(int argc, char **argv)
        : Jack::Client("jacker"),kit(argc,argv) {
        midi_omni_out = new Jack::MIDIPort(*this, "omni", Jack::MIDIPort::IsOutput);
        pattern_view = NULL;
        seq_view = NULL;
    }
    
    ~App() {
        delete midi_omni_out;
    }
    
    void init_ui() {
        builder->get_widget_derived("pattern_view", pattern_view);
        assert(pattern_view);
        
        Gtk::HScrollbar *pattern_hscroll;
        builder->get_widget("pattern_hscroll", pattern_hscroll);
        Gtk::VScrollbar *pattern_vscroll;
        builder->get_widget("pattern_vscroll", pattern_vscroll);
        pattern_view->set_scroll_adjustments(
            pattern_hscroll->get_adjustment(), 
            pattern_vscroll->get_adjustment());
        
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
                    pattern.add_event(i,1,ParamVolume,0x7f);
                    pattern.add_event(i,1,ParamNote,NOTE(Ds,6));
                } break;
                case 2:
                case 8:
                {
                    pattern.add_event(i,1,ParamVolume,0x7f);
                    pattern.add_event(i,1,ParamNote,NOTE(G,6));
                } break;
                case 4:
                case 10:
                {
                    pattern.add_event(i,1,ParamVolume,0x7f);
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
        
        pattern_view->select_pattern(model, pattern);
        
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
        
        sigc::slot<bool> mix_timer_slot = sigc::bind(
            sigc::mem_fun(*this, &App::mix), 0);
        mix_timer = Glib::signal_timeout().connect(mix_timer_slot,
            100);
    }

    void run() {
        if (!init())
            return;
        builder = Gtk::Builder::create_from_file("jacker.glade");
        
        Gtk::Window* window = 0;
        builder->get_widget("main", window);
        
        init_ui();
        
        window->show_all();
        
        activate();
        
        kit.run(*window);
        
        mix_timer.disconnect();
            
        deactivate();
        shutdown();
    }
    
    bool mix(int i) {
        player.mix(model);
        return true;
    }
    
    virtual void on_process(Jack::NFrames size) {
        midi_omni_out->clear_buffer();
        Player::Message msg;
        while (player.process(size,msg)) {
            midi_omni_out->write_event(0, msg);
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
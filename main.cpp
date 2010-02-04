
#include "jack.hpp"


#include <gtkmm.h>
#include <assert.h>
#include <stdio.h>

#include "model.hpp"
#include "patternview.hpp"
#include "seqview.hpp"

namespace Jacker {

class Client : public Jack::Client {
public:
    Jack::MIDIPort *midi_omni_out;

    Client()
      : Jack::Client("jacker") {
        midi_omni_out = new Jack::MIDIPort(*this, "omni", Jack::MIDIPort::IsOutput);
    }
    
    ~Client() {
        delete midi_omni_out;
    }

    virtual void on_process(Jack::NFrames size) {
    }
};

class App {
public:
    Gtk::Main kit;
    Jacker::Client client;
    Model model;

    Glib::RefPtr<Gtk::Builder> builder;

    PatternView *pattern_view;
    SeqView *seq_view;

    App(int argc, char **argv)
        : kit(argc,argv) {
        pattern_view = NULL;
        seq_view = NULL;
    }
    
    ~App() {
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
        pattern.set_length(4096);
        pattern.set_channel_count(256);
        
        for (int i = 0; i < 64; i += 8) {
            pattern.add_event(i,0,ParamNote,NOTE(C,4));
            pattern.add_event(i,3,ParamNote,NOTE(C,4));
        }
        for (int i = 0; i < 64; i += 4) {
            pattern.add_event(i,1,ParamVolume,0x7f);
            pattern.add_event(i,1,ParamNote,NOTE(Ds,6));
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
    }

    void run() {
        //~ if (!client.init())
            //~ return;
        builder = Gtk::Builder::create_from_file("jacker.glade");
        
        Gtk::Window* window = 0;
        builder->get_widget("main", window);
        
        init_ui();
        
        window->show_all();
        
        //~ client.activate();
        
        kit.run(*window);
            
        //~ client.deactivate();
        //~ client.shutdown();
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
#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

namespace Jacker {

//=============================================================================

class PatternView : public Gtk::Widget {
public:
    class Cursor {
    public:
        Cursor();
        void next_row();
        void next_channel();
        void next_param();
    
        int get_row() const;
        int get_channel() const;
        int get_param() const;
    
        // true if param is the last param in a channel
        bool is_last_param() const;
    
        int alloc_row_height(int height);
        int alloc_cell_width(int width);
        void get_pos(int &x, int &y) const;
        void set_start_pos(int x, int y);
        void set_cell_margin(int margin);
        int get_cell_margin() const;
        void set_channel_margin(int margin);
        int get_channel_margin() const;
        void set_row_margin(int margin);
        int get_row_margin() const;
        void get_cell_size(int &w, int &h) const;
    
    protected:
        // index of current row
        int row;
        // index of current channel
        int channel;
        // index of current parameter
        int param;
        // height of active row
        int row_height;
        // height of active cell
        int cell_width;
        // x and position
        int x, y;
        // start x and y position
        int start_x, start_y;
        // margin between cells
        int cell_margin;
        // margin between channels
        int channel_margin;
        // margin between rows
        int row_margin;
    };
    
    PatternView(BaseObjectType* cobject, 
                const Glib::RefPtr<Gtk::Builder>& builder);

    void select_pattern(class Model &model, class Pattern &pattern);

    virtual void on_realize();
    virtual bool on_expose_event(GdkEventExpose* event);
    
    void render_background(Cursor &cursor);
    void render_cell(Cursor &cursor, Pattern::Event *event);

    Glib::RefPtr<Gdk::GC> gc;
    Glib::RefPtr<Gdk::Colormap> cm;
    Glib::RefPtr<Pango::Layout> layout;
    Glib::RefPtr<Gdk::Window> window;
    
    Gdk::Color bgcolor;
    Gdk::Color fgcolor;
    Gdk::Color row_color_bar;
    Gdk::Color row_color_beat;
    
    // how wide is a pattern character
    int text_width;
    // how high is a pattern character
    int text_height;
    
    // how many frames are in one beat
    int frames_per_beat;
    // how many beats are in one bar
    int beats_per_bar;
protected:
    class Model *model;
    class Pattern *pattern;
};

//=============================================================================

} // namespace Jacker
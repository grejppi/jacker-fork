#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

namespace Jacker {

class PatternCursor;
class PatternView;
class PatternLayout;

//=============================================================================

class CellRenderer {
public:
    virtual ~CellRenderer() {}
    virtual void render_background(PatternView &view, PatternCursor &cursor);
    virtual void render_cell(PatternView &view, PatternCursor &cursor, 
                             Pattern::Event *event);
    virtual int get_width(const PatternLayout &layout);
};

//=============================================================================

class CellRendererNote : public CellRenderer {
public:
    virtual void render_cell(PatternView &view, PatternCursor &cursor, 
                             Pattern::Event *event);
    virtual int get_width(const PatternLayout &layout);
};

//=============================================================================

class CellRendererByte : public CellRenderer {
public:
    virtual void render_cell(PatternView &view, PatternCursor &cursor, 
                             Pattern::Event *event);
    virtual int get_width(const PatternLayout &layout);
};

//=============================================================================

class PatternLayout {
public:
    PatternLayout();
    void set_row_height(int height);
    int get_row_height() const;
    void set_cell_renderer(int param, CellRenderer *renderer);
    CellRenderer *get_cell_renderer(int param) const;
    int get_cell_count() const;
    void set_origin(int x, int y);
    void get_origin(int &x, int &y);
    void set_cell_margin(int margin);
    int get_cell_margin() const;
    void set_channel_margin(int margin);
    int get_channel_margin() const;
    void set_row_margin(int margin);
    int get_row_margin() const;
    void get_cell_size(int param, int &w, int &h) const;
    void get_cell_pos(int row, int channel, int param, 
                      int &x, int &y) const;
    int get_channel_width() const;
    int get_param_offset(int param) const;
    void set_text_size(int width, int height);
    void get_text_size(int &width, int &height) const;
protected:
    typedef std::vector<CellRenderer *> CellRendererArray;

    // height of active row
    int row_height;
    // cell renderers indexed by param
    CellRendererArray renderers;
    // start x and y position
    int origin_x, origin_y;
    // margin between cells
    int cell_margin;
    // margin between channels
    int channel_margin;
    // margin between rows
    int row_margin;
    // how wide is a pattern character
    int text_width;
    // how high is a pattern character
    int text_height;
};

//=============================================================================

class PatternCursor {
public:
    PatternCursor(const PatternLayout &layout);
    void origin();
    void next_row();
    void next_channel();
    void next_param();

    int get_row() const;
    int get_channel() const;
    int get_param() const;

    // true if param is the last param in a channel
    bool is_last_param() const;

    void get_pos(int &x, int &y) const;
    void get_cell_size(int &w, int &h) const;

    const PatternLayout &get_layout() const;

protected:
    void update_position();

    PatternLayout layout;
    // index of current row
    int row;
    // index of current channel
    int channel;
    // index of current parameter
    int param;
    // x and position
    int x, y;
};

//=============================================================================

class PatternView : public Gtk::Widget {
public:
    PatternView(BaseObjectType* cobject, 
                const Glib::RefPtr<Gtk::Builder>& builder);

    void select_pattern(class Model &model, class Pattern &pattern);

    virtual void on_realize();
    virtual bool on_expose_event(GdkEventExpose* event);

    Glib::RefPtr<Gdk::GC> gc;
    Glib::RefPtr<Gdk::Colormap> cm;
    Glib::RefPtr<Pango::Layout> pango_layout;
    Glib::RefPtr<Gdk::Window> window;
    
    Gdk::Color bgcolor;
    Gdk::Color fgcolor;
    Gdk::Color row_color_bar;
    Gdk::Color row_color_beat;
    
    PatternLayout layout;
    
    // how many frames are in one beat
    int frames_per_beat;
    // how many beats are in one bar
    int beats_per_bar;
    
    CellRendererNote note_renderer;
    CellRendererByte byte_renderer;
protected:
    class Model *model;
    class Pattern *pattern;
};

//=============================================================================

} // namespace Jacker
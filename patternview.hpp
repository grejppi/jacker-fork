#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

namespace Jacker {

class PatternCursor;
class PatternView;

//=============================================================================

class CellRenderer {
public:
    CellRenderer();
    virtual ~CellRenderer() {}
    virtual void render_background(PatternCursor &cursor, 
                                   bool selected);
    virtual void render_cell(PatternCursor &cursor, 
                             Pattern::Event *event, bool draw_cursor, 
                             bool selected);
    virtual int get_width();
    virtual int get_item(int x);
    virtual int get_item_count();
        
    void set_view(PatternView &view);
    PatternView *get_view() const;
protected:
    PatternView *view;
};

//=============================================================================

class CellRendererNote : public CellRenderer {
public:
    virtual void render_cell(PatternCursor &cursor, 
                             Pattern::Event *event, bool draw_cursor, 
                             bool selected);
    virtual int get_width();
    virtual int get_item(int x);
    virtual int get_item_count();
};

//=============================================================================

class CellRendererByte : public CellRenderer {
public:
    virtual void render_cell(PatternCursor &cursor, 
                             Pattern::Event *event, bool draw_cursor, 
                             bool selected);
    virtual int get_width();
    virtual int get_item(int x);
    virtual int get_item_count();
};

//=============================================================================

class PatternCursor {
friend class PatternSelection;
public:
    PatternCursor();
    void origin();
    void next_row();
    void next_channel();
    void next_param();

    int get_row() const;
    int get_channel() const;
    int get_param() const;
    int get_item() const;
    int get_column() const;

    void set_row(int row);
    void set_channel(int channel);
    void set_param(int param);
    void set_item(int item);

    void set_last_item();
    bool is_last_item() const;
    void navigate_row(int delta);
    void navigate_column(int delta);

    // true if param is the last param in a channel
    bool is_last_param() const;

    void get_pos(int &x, int &y) const;
    void set_pos(int x, int y);
    void get_cell_size(int &w, int &h, bool include_margin=false) const;

    PatternView *get_view() const;
    void set_view(PatternView &view);
    
    // true if cursor shares row/channel/param with other cursor
    bool is_at(const PatternCursor &other) const;

protected:
    PatternView *view;
    // index of current row
    int row;
    // index of current channel
    int channel;
    // index of current parameter
    int param;
    // index of parameter item
    int item;
};

//=============================================================================

class PatternSelection {
public:
    PatternSelection();
    bool in_range(const PatternCursor &cursor) const;

    void set_active(bool active);
    bool get_active() const;

    void set_view(PatternView &view);

    bool get_rect(int &x, int &y, int &width, int &height) const;

    void sort();

    PatternCursor p0;
    PatternCursor p1;
protected:
    
    bool active;
    
};

//=============================================================================

class PatternView : public Gtk::Widget {
public:
    enum {
        CharBegin = 32,
        CharEnd = 128,
    };
    
    enum InteractMode {
        InteractNone = 0,
        InteractSelect = 1,
    };
    
    PatternView(BaseObjectType* cobject, 
                const Glib::RefPtr<Gtk::Builder>& builder);

    void select_pattern(class Model &model, class Pattern &pattern);

    virtual void on_realize();
    virtual bool on_expose_event(GdkEventExpose* event);
    virtual bool on_motion_notify_event(GdkEventMotion *event);
    virtual bool on_button_press_event(GdkEventButton* event);
    virtual bool on_button_release_event(GdkEventButton* event);
    virtual bool on_key_press_event(GdkEventKey* event);
    virtual bool on_key_release_event(GdkEventKey* event);
    virtual void on_size_allocate(Gtk::Allocation& allocation);

    void set_cursor(const PatternCursor &cursor);
    void set_cursor(int x, int y);

    Glib::RefPtr<Gdk::GC> gc;
    Glib::RefPtr<Gdk::GC> xor_gc;
    Glib::RefPtr<Gdk::Colormap> cm;
    Glib::RefPtr<Pango::Layout> pango_layout;
    Glib::RefPtr<Gdk::Window> window;
    std::vector< Glib::RefPtr<Gdk::Pixmap> > chars;
    
    std::vector<Gdk::Color> colors;

    // how many frames are in one beat
    int frames_per_beat;
    // how many beats are in one bar
    int beats_per_bar;
    
    CellRendererNote note_renderer;
    CellRendererByte byte_renderer;
    
    void set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                Gtk::Adjustment *vadjustment);
                                
    void on_adjustment_value_changed();
    void draw_text(int x, int y, const char *text);
    void navigate(int delta_x, int delta_y);
    
    void show_cursor();
    
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
    void get_cell_size(int param, int &w, int &h, bool include_margin=false) const;
    void get_cell_pos(int row, int channel, int param,
                      int &x, int &y) const;
    bool get_cell_location(int x, int y, int &row, int &channel,
        int &param, int &item) const;
    int get_channel_width() const;
    int get_param_offset(int param) const;
    void set_text_size(int width, int height);
    void get_text_size(int &width, int &height) const;
    
protected:
    void invalidate_cursor();
    void invalidate_selection();
    void clip_cursor(PatternCursor &c);
    void update_adjustments();
        
    InteractMode interact_mode;

    class Model *model;
    class Pattern *pattern;
    Gtk::Adjustment *hadjustment;
    Gtk::Adjustment *vadjustment;
    PatternCursor cursor;
    PatternSelection selection;

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
    // how wide is a pattern character
    int text_width;
    // how high is a pattern character
    int text_height;
};

//=============================================================================

} // namespace Jacker
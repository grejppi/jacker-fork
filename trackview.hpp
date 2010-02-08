#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

#include <set>

namespace Jacker {

class TrackView;
    
//=============================================================================

class TrackCursor {
public:
    TrackCursor();
    
    void set_view(TrackView &view);
    TrackView *get_view() const;

    void set_track(int track);
    int get_track() const;

    void set_frame(int frame);
    int get_frame() const;

    void origin();
    void next_track();
    void next_frame();

    void get_pos(int &x, int &y) const;
    void set_pos(int x, int y);

protected:
    // pointer to the view
    TrackView *view;

    // the current track the cursor is on
    int track;
    // the frame the cursor is on
    int frame;
};
    
//=============================================================================

class Drag {
public:
    int start_x, start_y;
    int x, y;
    int threshold;

    Drag();
    void start(int x, int y);
    void update(int x, int y);
    // returns true if the threshold has been reached
    bool threshold_reached();
    void get_delta(int &delta_x, int &delta_y);
};

//=============================================================================

class TrackView : public Gtk::Widget {
public:
    enum {
        TrackHeight = 22,
    };
    
    enum InteractMode {
        InteractNone = 0,
        InteractDrag,
        InteractMove,
        InteractResize,
    };
    
    enum SnapMode {
        SnapOff = 0,
        SnapBar = 1,
    };
    
    // signals
    typedef sigc::signal<void, Pattern *> type_pattern_edit_request;
    typedef sigc::signal<void, TrackView *, GdkEventButton*> type_context_menu;
    
    typedef std::set<TrackEventRef> EventSet;
    
    TrackView(BaseObjectType* cobject, 
            const Glib::RefPtr<Gtk::Builder>& builder);

    void set_model(class Model &model);

    virtual void on_realize();
    virtual bool on_expose_event(GdkEventExpose* event);
    virtual bool on_motion_notify_event(GdkEventMotion *event);
    virtual bool on_button_press_event(GdkEventButton* event);
    virtual bool on_button_release_event(GdkEventButton* event);
    virtual bool on_key_press_event(GdkEventKey* event);
    virtual bool on_key_release_event(GdkEventKey* event);
    virtual void on_size_allocate(Gtk::Allocation& allocation);

    void set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                Gtk::Adjustment *vadjustment);

    Glib::RefPtr<Gdk::Window> window;
    Glib::RefPtr<Gdk::GC> gc;
    Glib::RefPtr<Gdk::GC> xor_gc;
    Glib::RefPtr<Gdk::Colormap> cm;
    Glib::RefPtr<Pango::Layout> pango_layout;
    std::vector<Gdk::Color> colors;
    
    void set_origin(int x, int y);
    void get_origin(int &x, int &y) const;
    void get_event_pos(int frame, int track,
                       int &x, int &y) const;
    void get_event_size(int length, int &w, int &h) const;
    void get_event_length(int w, int h, int &length, int &track) const;
    void get_event_location(int x, int y, int &frame, int &track) const;
    
    void get_event_rect(const TrackEventRef &ref, int &x, int &y, int &w, int &h);
    bool find_event(const TrackCursor &cursor, TrackEventRef &ref);
    
    void clear_selection();
    void select_event(const TrackEventRef &ref);
    void deselect_event(const TrackEventRef &ref);
    bool is_event_selected(const TrackEventRef &ref);
    void set_play_position(int pos);
    void invalidate();
    
    void add_track();
    void new_pattern(const TrackCursor &cursor);
    void edit_pattern(const TrackEventRef &ref);
    
    type_pattern_edit_request signal_pattern_edit_request();
    type_context_menu signal_context_menu();
protected:
    void invalidate_selection();
    void invalidate_play_position();
    void render_event(const TrackEventRef &ref);
    void render_track(Track &track);

    bool can_resize_event(const TrackEventRef &ref, int x);
    void get_drag_offset(int &frame, int &track);
    int get_step_size();
    int quantize_frame(int frame); 

    bool resizing() const;
    bool dragging() const;
    bool moving() const;

    void apply_move();
    void apply_resize();

    void update_adjustments();
    void on_adjustment_value_changed();

    // zoomlevel (0=1:1, 1=1:2, 2=1:4, etc.)
    int zoomlevel;
    int snap_frames;
    
    // start x and y position
    int origin_x, origin_y;

    InteractMode interact_mode;
    SnapMode snap_mode;

    int play_position;
    Drag drag;

    Model *model;
    EventSet selection;
    TrackCursor cursor;

    type_pattern_edit_request _pattern_edit_request;
    type_context_menu _signal_context_menu;
    
    Gtk::Adjustment *hadjustment;
    Gtk::Adjustment *vadjustment;
};

//=============================================================================
    
} // namespace Jacker
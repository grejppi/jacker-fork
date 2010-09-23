#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"
#include "drag.hpp"

#include <set>

namespace Jacker {

class SongView;
    
//=============================================================================

class SongCursor {
public:
    SongCursor(SongView &view);
    SongCursor();
    
    void set_view(SongView &view);
    SongView *get_view() const;

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
    SongView *view;

    // the current track the cursor is on
    int track;
    // the frame the cursor is on
    int frame;
};

//=============================================================================

class SongView : public Gtk::Widget {
public:
    enum {
        TrackHeight = 22,
    };
    
    enum InteractMode {
        InteractNone = 0,
        InteractDrag,
        InteractMove,
        InteractResize,
        InteractSelect,
    };
    
    enum SnapMode {
        SnapOff = 0,
        SnapBar = 1,
    };
    
    // signals
    typedef sigc::signal<void, Song::iterator> type_pattern_edit_request;
    typedef sigc::signal<void, SongView *, GdkEventButton*> type_context_menu;
    typedef sigc::signal<void> type_loop_changed;
    typedef sigc::signal<void, int> type_play_request;
    typedef sigc::signal<void, Song::iterator> type_pattern_erased;
    typedef sigc::signal<void> type_tracks_changed;
    typedef sigc::signal<void, int> type_seek_request;
    
    SongView(BaseObjectType* cobject, 
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

    Song::iterator get_left_event(Song::iterator start);
    Song::iterator get_right_event(Song::iterator start);
    Song::iterator nearest_y_event(Song::iterator start, int direction);
    void navigate(int dir_x, int dir_y, bool increment=false);
    void select_first(bool increment=false);
    void select_last(bool increment=false);
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
    
    void get_event_rect(Song::iterator event, int &x, int &y, int &w, int &h);
    bool find_event(const SongCursor &cursor, Song::iterator &event);
    
    void clear_selection();
    void select_event(Song::iterator event);
    void deselect_event(Song::iterator event);
    bool is_event_selected(Song::iterator event);
    void set_play_position(int pos);
    void invalidate();
    
    void add_track();
    void new_pattern(const SongCursor &cursor, int length);
    void new_pattern(int length);
    void edit_pattern(Song::iterator event);
    void erase_events();
    
    void set_loop(const Loop &loop);
    void set_loop_begin();
    void set_loop_end();
    
    void reset();
    
    type_pattern_edit_request signal_pattern_edit_request();
    type_context_menu signal_context_menu();
    type_loop_changed signal_loop_changed();
    type_play_request signal_play_request();
    type_pattern_erased signal_pattern_erased();
    type_tracks_changed signal_tracks_changed();
    type_seek_request signal_seek_request();
protected:
    void invalidate_selection();
    void invalidate_play_position();
    void render_event(Song::iterator event);
    void render_track(int track);
    void render_select_box();
    void invalidate_select_box();
    void select_from_box(bool toggle);
    void render_loop();
    void invalidate_loop();
    bool get_selection_range(int &frame_begin, int &frame_end,
        int &track_begin, int &track_end);
    int get_selection_begin();
    int get_selection_end();
    void play_from_selection();
    void seek_to_mouse_cursor();
    void split_at_mouse_cursor();

    bool can_resize_event(Song::iterator event, int x);
    void get_drag_offset(int &frame, int &track);
    int get_step_size();
    int quantize_frame(int frame); 

    bool resizing() const;
    bool dragging() const;
    bool moving() const;
    bool selecting() const;

    void apply_move();
    void apply_resize();
    void do_move(int frame_ofs, int track_ofs);
    void do_resize(int frame_ofs);

    void update_adjustments();
    void on_adjustment_value_changed();
    
    void clone_selection(bool references=false);
    void join_selection();
    void toggle_mute_selection();
    
    void show_selection();

    // zoomlevel (0=1:1, 1=1:2, 2=1:4, etc.)
    int zoomlevel;
    int snap_frames;
    
    // last cursor position
    int cursor_x, cursor_y;
    
    // start x and y position
    int origin_x, origin_y;

    InteractMode interact_mode;
    SnapMode snap_mode;

    int play_position;
    Drag drag;
    
    Loop loop;

    Model *model;
    Song::IterList selection;

    type_pattern_edit_request _pattern_edit_request;
    type_context_menu _signal_context_menu;
    type_loop_changed _loop_changed;
    type_play_request _play_request;
    type_pattern_erased _pattern_erased;
    type_tracks_changed _tracks_changed;
    type_seek_request _seek_request;
    
    Gtk::Adjustment *hadjustment;
    Gtk::Adjustment *vadjustment;
};

//=============================================================================
    
} // namespace Jacker
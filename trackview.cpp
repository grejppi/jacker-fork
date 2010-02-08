#include "trackview.hpp"
#include "model.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace Jacker {

enum {
    ColorBlack = 0,
    ColorWhite,
    ColorBackground,
    ColorTrack,

    ColorCount,
};

enum {
    // how far away from the border can a resize be done
    ResizeThreshold = 8,
};

//=============================================================================

TrackCursor::TrackCursor() {
    view = NULL;
    track = 0;
    frame = 0;
}

void TrackCursor::set_view(TrackView &view) {
    this->view = &view;
}

TrackView *TrackCursor::get_view() const {
    return view;
}

void TrackCursor::set_track(int track) {
    this->track = track;
}

int TrackCursor::get_track() const {
    return track;
}

void TrackCursor::set_frame(int frame) {
    this->frame = frame;
}

int TrackCursor::get_frame() const {
    return frame;
}

void TrackCursor::get_pos(int &x, int &y) const {
    assert(view);
    view->get_event_pos(frame, track, x, y);
}

void TrackCursor::set_pos(int x, int y) {
    assert(view);
    view->get_event_location(x, y, frame, track);
}

//=============================================================================

Drag::Drag() {
    x = y = 0;
    start_x = start_y = 0;
    threshold = 8;
}

void Drag::start(int x, int y) {
    this->x = this->start_x = x;
    this->y = this->start_y = y;
}

void Drag::update(int x, int y) {
    this->x = x;
    this->y = y;
}

bool Drag::threshold_reached() {
    int delta_x;
    int delta_y;
    get_delta(delta_x, delta_y);
    float length = std::sqrt((float)(delta_x*delta_x + delta_y*delta_y));
    if ((int)(length+0.5) >= threshold)
        return true;
    return false;
}

void Drag::get_delta(int &delta_x, int &delta_y) {
    delta_x = (x - start_x);
    delta_y = (y - start_y);
}

//=============================================================================

TrackView::TrackView(BaseObjectType* cobject, 
                 const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    origin_x = 0;
    origin_y = 0;
    model = NULL;
    zoomlevel = 0;
    hadjustment = NULL;
    vadjustment = NULL;
    snap_mode = SnapBar;
    interact_mode = InteractNone;
    cursor.set_view(*this);
    colors.resize(ColorCount);
    colors[ColorBlack].set("#000000");
    colors[ColorWhite].set("#FFFFFF");
    colors[ColorBackground].set("#e0e0e0");
    colors[ColorTrack].set("#ffffff");
    play_position = 0;
}

void TrackView::set_model(class Model &model) {
    this->model = &model;
}

void TrackView::on_realize() {
    Gtk::Widget::on_realize();
    
    window = get_window();    
    // create drawing resources
    gc = Gdk::GC::create(window);
    cm = gc->get_colormap();
    
    for (std::vector<Gdk::Color>::iterator i = colors.begin(); 
         i != colors.end(); ++i) {
        cm->alloc_color(*i);
    }
    
    Pango::FontDescription font_desc("sans 8");
    
    pango_layout = Pango::Layout::create(get_pango_context());
    pango_layout->set_font_description(font_desc);
    pango_layout->set_width(-1);
    
    // create xor gc for drawing the cursor
    xor_gc = Gdk::GC::create(window);
    Glib::RefPtr<Gdk::Colormap> xor_cm = xor_gc->get_colormap();
    Gdk::Color xor_color;
    xor_color.set("#ffffff"); xor_cm->alloc_color(xor_color);
    xor_gc->set_function(Gdk::XOR);
    xor_gc->set_foreground(xor_color);
    xor_gc->set_background(xor_color);
    
    update_adjustments();
}

void TrackView::set_origin(int x, int y) {
    origin_x = x;
    origin_y = y;
}

void TrackView::get_origin(int &x, int &y) const {
    x = origin_x;
    y = origin_y;
}

void TrackView::get_event_pos(int frame, int track,
                            int &x, int &y) const {
    x = origin_x + (frame>>zoomlevel);
    y = origin_y + track*TrackHeight;
}


void TrackView::get_event_size(int length, int &w, int &h) const {
    w = length>>zoomlevel;
    h = TrackHeight;
}

void TrackView::get_event_length(int w, int h, int &length, int &track) const {
    length = w<<zoomlevel;
    track = h/TrackHeight;
}

void TrackView::get_event_location(int x, int y, int &frame, int &track) const {
    frame = (x - origin_x)<<zoomlevel;
    track = (y - origin_y)/TrackHeight;
}

bool TrackView::find_event(const TrackCursor &cur, TrackEventRef &ref) {
    if (cur.get_track() >= model->get_track_count())
        return false;
    Track *track = &model->get_track(cur.get_track());
    Track::iterator iter = track->find_event(cur.get_frame());
    if (iter == track->end())
        return false;
    ref.track = track;
    ref.iter = iter;
    return true;
}

void TrackView::render_event(const TrackEventRef &ref) {
    int x,y,w,h;
    get_event_rect(ref, x, y, w, h);
    gc->set_foreground(colors[ColorBlack]);
    bool selected = is_event_selected(ref);
    if (selected)
        window->draw_rectangle(gc, true, x, y, w, h);
    else
        window->draw_rectangle(gc, false, x, y, w-1, h-1);
}

void TrackView::render_track(Track &track) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    int x,y;
    get_event_pos(0, track.order, x, y);
    
    gc->set_foreground(colors[ColorTrack]);
    window->draw_rectangle(gc, true, 0, y, width, TrackHeight);
}

bool TrackView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    // clear screen
    gc->set_foreground(colors[ColorBackground]);
    window->draw_rectangle(gc, true, 0, 0, width, height);
    
    // first pass: render tracks
    TrackArray::iterator track_iter;
    for (track_iter = model->tracks.begin(); 
         track_iter != model->tracks.end(); ++track_iter) {
        Track &track = *(*track_iter);
        render_track(track);
    }
    
    // second pass: render events
    for (track_iter = model->tracks.begin(); 
         track_iter != model->tracks.end(); ++track_iter) {
        Track &track = *(*track_iter);
        for (Track::iterator iter = track.begin(); iter != track.end(); ++iter) {
            render_event(TrackEventRef(track,iter));
        }
    }

    // draw play cursor
    int play_x, play_y;
    get_event_pos(play_position, 0, play_x, play_y);
    window->draw_rectangle(xor_gc, true, play_x, 0, 2, height);

    return true;
}

void TrackView::invalidate_play_position() {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    int play_x, play_y;
    get_event_pos(play_position, 0, play_x, play_y);
    Gdk::Rectangle rect(play_x,0,2,height);
    window->invalidate_rect(rect, true);
}

bool TrackView::is_event_selected(const TrackEventRef &ref) {
    return (selection.find(ref) != selection.end());
}

void TrackView::clear_selection() {
    invalidate_selection();
    selection.clear();
}

void TrackView::select_event(const TrackEventRef &ref) {
    selection.insert(ref);
    invalidate_selection();
}

void TrackView::deselect_event(const TrackEventRef &ref) {
    invalidate_selection();
    selection.erase(ref);
}

void TrackView::add_track() {
    model->new_track();
    invalidate();
}

void TrackView::new_pattern(const TrackCursor &cur) {
    if (cur.get_track() < model->get_track_count()) {
        Track &track = model->get_track(cur.get_track());
        Pattern &pattern = model->new_pattern();
        TrackEventRef ref(track, 
            track.add_event(cur.get_frame(), pattern));
        clear_selection();
        select_event(ref);
    }
    
}

void TrackView::edit_pattern(const TrackEventRef &ref) {
    Pattern *pattern = ref.iter->second.pattern;
    _pattern_edit_request(pattern);
}


bool TrackView::dragging() const {
    return (interact_mode == InteractDrag);
}

bool TrackView::moving() const {
    return (interact_mode == InteractMove);
}

bool TrackView::resizing() const {
    return (interact_mode == InteractResize);
}

bool TrackView::on_button_press_event(GdkEventButton* event) {
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    /*
    bool shift_down = event->state & Gdk::SHIFT_MASK;
    bool alt_down = event->state & Gdk::MOD1_MASK;
    bool super_down = event->state & (Gdk::SUPER_MASK|Gdk::MOD4_MASK);
    */
    bool double_click = (event->type == GDK_2BUTTON_PRESS);
    
    grab_focus();
    
    if (event->button == 1) {
        TrackCursor cur(cursor);
        cur.set_pos(event->x, event->y);
        TrackEventRef ref;

        if (!ctrl_down && (selection.size() == 1))
            clear_selection();
        
        if (find_event(cur, ref)) {
            if (ctrl_down && is_event_selected(ref)) {
                deselect_event(ref);
            } else {
                select_event(ref);
                if (double_click) {
                    interact_mode = InteractNone;
                    edit_pattern(ref);
                } else {
                    interact_mode = InteractDrag;
                    drag.start(event->x, event->y);
                }
            }
        } else if (double_click) {
            cur.set_frame(quantize_frame(cur.get_frame()));
            new_pattern(cur);
        } else {
            clear_selection();
        }
    } else if (event->button == 3) {
        _signal_context_menu(this, event);
    }
    return true;
}

bool TrackView::can_resize_event(const TrackEventRef &ref, int x) {
    int ex, ey, ew, eh;
    get_event_rect(ref,ex,ey,ew,eh);
    return std::abs(x - (ex+ew)) < ResizeThreshold;
}

bool TrackView::on_motion_notify_event(GdkEventMotion *event) {    
    if (interact_mode == InteractNone) {
        TrackCursor cur(cursor);
        cur.set_pos(event->x, event->y);
        TrackEventRef ref;
        if (find_event(cur, ref)) {
            if (can_resize_event(ref, event->x))
                window->set_cursor(Gdk::Cursor(Gdk::SB_H_DOUBLE_ARROW));
            else
                window->set_cursor(Gdk::Cursor(Gdk::HAND1));
        } else
            window->set_cursor(Gdk::Cursor(Gdk::ARROW));
        return false;
    }
    if (dragging()) {
        drag.update(event->x, event->y);
        if (drag.threshold_reached()) {
            invalidate_selection();
            TrackCursor cur(cursor);
            cur.set_pos(drag.start_x, drag.start_y);
            TrackEventRef ref;
            if (find_event(cur, ref) && can_resize_event(ref,drag.start_x)) {
                interact_mode = InteractResize;
            } else {
                interact_mode = InteractMove;
            }
        }
    }
    if (resizing()||moving()) {
        invalidate_selection();
        drag.update(event->x, event->y);
        invalidate_selection();
    }
    return true;
}

void TrackView::apply_move() {
    invalidate_selection();
    
    int ofs_frame,ofs_track;
    get_drag_offset(ofs_frame, ofs_track);
    
    bool can_move = true;        
    // verify that we can move
    for (EventSet::iterator iter = selection.begin();
        iter != selection.end(); ++iter) {
        const TrackEventRef &ref = *iter;
        Track::Event &event = ref.iter->second;
        int frame = event.frame + ofs_frame;
        int track = ref.track->order + ofs_track;
        if ((frame < 0)||(track < 0)||(track >= model->get_track_count())) {
            can_move = false;
            break;
        }
    }
    
    if (can_move) {
        EventSet new_selection;
        
        // verify that we can move
        for (EventSet::iterator iter = selection.begin();
            iter != selection.end(); ++iter) {
            const TrackEventRef &ref = *iter;
            Track::Event event = ref.iter->second;
            
            event.frame += ofs_frame;
            int track_index = ref.track->order + ofs_track;
            
            model->delete_event(ref);
            
            Track &track = model->get_track(track_index);
            TrackEventRef new_ref(track, 
                track.add_event(event));
            new_selection.insert(new_ref);
        }
        
        selection.clear();
        //selection = new_selection;
    }
}

void TrackView::apply_resize() {
    
    int ofs_frame,ofs_track;
    get_drag_offset(ofs_frame, ofs_track);
    
    // verify that we can move
    for (EventSet::iterator iter = selection.begin();
        iter != selection.end(); ++iter) {
        const TrackEventRef &ref = *iter;
        Track::Event &event = ref.iter->second;
        
        int length = std::max(event.pattern->get_length() + ofs_frame, 
            get_step_size());
            
        event.pattern->set_length(length);
    }
    
    invalidate();
}

bool TrackView::on_button_release_event(GdkEventButton* event) {
    if (moving()) {
        apply_move();
    } else if (resizing()) {
        apply_resize();
    }
    interact_mode = InteractNone;
    invalidate_selection();
    return false;
}

bool TrackView::on_key_press_event(GdkEventKey* event) {
    return false;
}

bool TrackView::on_key_release_event(GdkEventKey* event) {
    return false;
}

void TrackView::on_size_allocate(Gtk::Allocation& allocation) {
    set_allocation(allocation);
    
    if (window) {
        window->move_resize(allocation.get_x(), allocation.get_y(),
            allocation.get_width(), allocation.get_height());
        update_adjustments();
    }
}

int absmod(int x, int m) {
    return (x >= 0)?(x%m):((m-x)%m);
}

void TrackView::get_drag_offset(int &frame, int &track) {
    int dx,dy;
    drag.get_delta(dx,dy);
    get_event_length(dx, dy, frame, track);
    frame = quantize_frame(frame);
}

int TrackView::get_step_size() {
    switch(snap_mode) {
        case SnapBar: return model->get_frames_per_bar();
        default: break;
    }
    return 1;
}

int TrackView::quantize_frame(int frame) {
    return frame - (frame%get_step_size());
}

void TrackView::get_event_rect(const TrackEventRef &ref, int &x, int &y, int &w, int &h) {
    Track::Event &event = ref.iter->second;
    int frame = event.frame;
    int track = ref.track->order;
    int length = event.pattern->get_length();
    if (moving() && is_event_selected(ref)) {
        int ofs_frame,ofs_track;
        get_drag_offset(ofs_frame, ofs_track);
        frame += ofs_frame;
        track += ofs_track;
    } else if (resizing() && is_event_selected(ref)) {
        int ofs_frame,ofs_track;
        get_drag_offset(ofs_frame, ofs_track);
        length = std::max(length + ofs_frame, get_step_size());
    }
    get_event_pos(frame, track, x, y);
    get_event_size(length, w, h);
}

void TrackView::invalidate() {
    if (!window)
        return;
    window->invalidate(true);
}

void TrackView::invalidate_selection() {
    EventSet::iterator iter;
    for (iter = selection.begin(); iter != selection.end(); ++iter) {
        int x,y,w,h;
        get_event_rect(*iter, x, y, w, h);
        Gdk::Rectangle rect(x,y,w,h);
        window->invalidate_rect(rect, true);
    }
}

void TrackView::set_play_position(int pos) {
    if (!window)
        return;
    if (pos == play_position)
        return;
    invalidate_play_position();
    play_position = pos;
    invalidate_play_position();
}

void TrackView::update_adjustments() {
    if (!window)
        return;
    Gtk::Allocation allocation = get_allocation();

    int width = allocation.get_width();
    int height = allocation.get_height();
    
    if (hadjustment) {
        int frame, track;
        get_event_location(width, height, frame, track);
        
        hadjustment->configure(0, // value
                               0, // lower
                               frame*4, // upper
                               1, // step increment
                               1, // page increment
                               frame); // page size
    }
    
}

void TrackView::on_adjustment_value_changed() {
    if (hadjustment) {
        int w,h;
        get_event_size((int)(hadjustment->get_value()+0.5),w,h);
        origin_x = -w;
    }
    
    invalidate();
}

void TrackView::set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                     Gtk::Adjustment *vadjustment) {
    this->hadjustment = hadjustment;
    this->vadjustment = vadjustment;
    if (hadjustment) {
        hadjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &TrackView::on_adjustment_value_changed));
    }
    if (vadjustment) {
        vadjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &TrackView::on_adjustment_value_changed));
    }                                         
}

TrackView::type_pattern_edit_request TrackView::signal_pattern_edit_request() {
    return _pattern_edit_request;
}

TrackView::type_context_menu TrackView::signal_context_menu() {
    return _signal_context_menu;
}

//=============================================================================

} // namespace Jacker
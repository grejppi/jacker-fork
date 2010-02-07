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

TrackView::TrackView(BaseObjectType* cobject, 
                 const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    model = NULL;
    zoomlevel = 1;
    cursor.set_view(*this);
    origin_x = origin_y = 0;
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
    
}

void TrackView::set_origin(int x, int y) {
    this->origin_x = origin_x;
    this->origin_y = origin_y;
}

void TrackView::get_origin(int &x, int &y) {
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
    window->draw_rectangle(gc, true, x, y, width, TrackHeight);
}

bool TrackView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    // clear screen
    gc->set_foreground(colors[ColorBackground]);
    window->draw_rectangle(gc, true, 0, 0, width, height);
    
    TrackArray::iterator track_iter;
    for (track_iter = model->tracks.begin(); 
         track_iter != model->tracks.end(); ++track_iter) {
        Track &track = *(*track_iter);
        render_track(track);
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

bool TrackView::on_motion_notify_event(GdkEventMotion *event) {
    return false;
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
        Track::iterator iter = track.add_event(cur.get_frame(), pattern);
        TrackEventRef ref;
        ref.track = &track;
        ref.iter = iter;
        clear_selection();
        select_event(ref);
    }
    
}

void TrackView::edit_pattern(const TrackEventRef &ref) {
    Pattern *pattern = ref.iter->second.pattern;
    _pattern_edit_request(pattern);
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

        if (!ctrl_down)
            clear_selection();
        
        if (find_event(cur, ref)) {
            if (ctrl_down && is_event_selected(ref)) {
                deselect_event(ref);
            } else {
                select_event(ref);
                if (double_click)
                    edit_pattern(ref);
            }
        } else if (double_click) {
            new_pattern(cur);
        }
    } else if (event->button == 3) {
        _signal_context_menu(this, event);
    }
    return false;
}

bool TrackView::on_button_release_event(GdkEventButton* event) {
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
    }    
}

void TrackView::get_event_rect(const TrackEventRef &ref, int &x, int &y, int &w, int &h) {
    Track::Event &event = ref.iter->second;
    get_event_pos(event.frame, ref.track->order, x, y);
    get_event_size(event.pattern->get_length(), w, h);
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

void TrackView::set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                     Gtk::Adjustment *vadjustment) {
}

TrackView::type_pattern_edit_request TrackView::signal_pattern_edit_request() {
    return _pattern_edit_request;
}

TrackView::type_context_menu TrackView::signal_context_menu() {
    return _signal_context_menu;
}

//=============================================================================

} // namespace Jacker
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
    ColorRowBar,
    ColorRowBeat,
    ColorSelBackground,
    ColorSelRowBar,
    ColorSelRowBeat,

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
    colors[ColorRowBar].set("#c0c0c0");
    colors[ColorRowBeat].set("#d0d0d0");
    colors[ColorSelBackground].set("#00a0ff");
    colors[ColorSelRowBar].set("#20c0ff");
    colors[ColorSelRowBeat].set("#40e0ff");        
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

bool TrackView::on_button_press_event(GdkEventButton* event) {
    grab_focus();
    TrackCursor cur(cursor);
    cur.set_pos(event->x, event->y);
    TrackEventRef ref;
    clear_selection();
    if (find_event(cur, ref)) {
        select_event(ref);
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

//=============================================================================

} // namespace Jacker
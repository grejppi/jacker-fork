#include "seqview.hpp"
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

SeqCursor::SeqCursor() {
    view = NULL;
    track = 0;
    frame = 0;
}

void SeqCursor::set_view(SeqView &view) {
    this->view = &view;
}

SeqView *SeqCursor::get_view() const {
    return view;
}

void SeqCursor::set_track(int track) {
    this->track = track;
}

int SeqCursor::get_track() const {
    return track;
}

void SeqCursor::set_frame(int frame) {
    this->frame = frame;
}

int SeqCursor::get_frame() const {
    return frame;
}

void SeqCursor::get_pos(int &x, int &y) const {
    assert(view);
    view->get_event_pos(frame, track, x, y);
}

void SeqCursor::set_pos(int x, int y) {
    assert(view);
    view->get_event_location(x, y, frame, track);
}

//=============================================================================

SeqView::SeqView(BaseObjectType* cobject, 
                 const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    model = NULL;
    zoomlevel = 1;
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
}

void SeqView::set_model(class Model &model) {
    this->model = &model;
}

void SeqView::on_realize() {
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

void SeqView::set_origin(int x, int y) {
    this->origin_x = origin_x;
    this->origin_y = origin_y;
}

void SeqView::get_origin(int &x, int &y) {
    x = origin_x;
    y = origin_y;
}

void SeqView::get_event_pos(int frame, int track,
                            int &x, int &y) const {
    x = origin_x + (frame>>zoomlevel);
    y = origin_y + track*TrackHeight;
}


void SeqView::get_event_size(int length, int &w, int &h) const {
    w = length>>zoomlevel;
    h = TrackHeight;
}

void SeqView::get_event_location(int x, int y, int &frame, int &track) const {
    frame = (x - origin_x)<<zoomlevel;
    track = (y - origin_y)/TrackHeight;
}

void SeqView::render_event(SeqCursor &cursor, Track::Event &event) {
    int x,y,w,h;
    cursor.get_pos(x,y);
    get_event_size(event.pattern->get_length(), w, h);
    gc->set_foreground(colors[ColorBlack]);
    window->draw_rectangle(gc, false, 0, 0, w-1, h-1);    
}

bool SeqView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    // clear screen
    gc->set_foreground(colors[ColorBackground]);
    window->draw_rectangle(gc, true, 0, 0, width, height);
    
    SeqCursor render_cursor;
    render_cursor.set_view(*this);
    
    for (int track_index = 0; track_index < model->get_track_count(); 
         ++track_index) {
             
        render_cursor.set_track(track_index);
        Track &track = model->get_track(track_index);
        for (Track::iterator iter = track.begin(); iter != track.end(); ++iter) {
            Track::Event &evt = iter->second;
            render_cursor.set_frame(evt.frame);
            render_event(render_cursor, evt);
        }
    }
    
    return true;
}

bool SeqView::on_motion_notify_event(GdkEventMotion *event) {
    return false;
}

bool SeqView::on_button_press_event(GdkEventButton* event) {
    grab_focus();
    SeqCursor cursor;
    cursor.set_view(*this);
    cursor.set_pos(event->x, event->y);
    return false;
}

bool SeqView::on_button_release_event(GdkEventButton* event) {
    return false;
}

bool SeqView::on_key_press_event(GdkEventKey* event) {
    return false;
}

bool SeqView::on_key_release_event(GdkEventKey* event) {
    return false;
}

void SeqView::on_size_allocate(Gtk::Allocation& allocation) {
    set_allocation(allocation);
    
    if (window) {
        window->move_resize(allocation.get_x(), allocation.get_y(),
            allocation.get_width(), allocation.get_height());
    }    
}

void SeqView::set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                     Gtk::Adjustment *vadjustment) {
}

//=============================================================================

} // namespace Jacker
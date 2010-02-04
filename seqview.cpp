#include "seqview.hpp"
#include "model.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace Jacker {

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

//=============================================================================

SeqView::SeqView(BaseObjectType* cobject, 
                 const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    model = NULL;
}

void SeqView::set_model(class Model &model) {
    this->model = &model;
}

void SeqView::on_realize() {
    Gtk::Widget::on_realize();
    
    window = get_window();    
    
}

bool SeqView::on_expose_event(GdkEventExpose* event) {
    return true;
}

bool SeqView::on_motion_notify_event(GdkEventMotion *event) {
    return false;
}

bool SeqView::on_button_press_event(GdkEventButton* event) {
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
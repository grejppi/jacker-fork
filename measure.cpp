#include "measure.hpp"
#include "model.hpp"


namespace Jacker {

enum {
    ColorBlack = 0,
    ColorWhite,
    ColorBackground,

    ColorCount,
};

MeasureView::MeasureView(BaseObjectType* cobject, 
                         const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    adjustment = NULL;
    model = NULL;
    colors.resize(ColorCount);
    colors[ColorBlack].set("#000000");
    colors[ColorWhite].set("#FFFFFF");
    colors[ColorBackground].set("#FFFFFF");
}

void MeasureView::set_model(class Model &model) {
    this->model = &model;
}

void MeasureView::on_realize() {
    Gtk::Widget::on_realize();
    
    window = get_window();    
    // create drawing resources
    gc = Gdk::GC::create(window);
    cm = gc->get_colormap();
    
    for (std::vector<Gdk::Color>::iterator i = colors.begin(); 
         i != colors.end(); ++i) {
        cm->alloc_color(*i);
    }
    
    Pango::FontDescription font_desc("sans 7");
    
    pango_layout = Pango::Layout::create(get_pango_context());
    pango_layout->set_font_description(font_desc);
    pango_layout->set_width(-1);
}

bool MeasureView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    // clear screen
    gc->set_foreground(colors[ColorBackground]);
    window->draw_rectangle(gc, true, 0, 0, width, height);
    
    double value = adjustment->get_value();
    double page_size = adjustment->get_page_size();
    double scale = (double)width / page_size;
    
    int fpb = model->get_frames_per_bar();
    
    int frame = (int)value;
    frame -= (frame%fpb);
    
    gc->set_foreground(colors[ColorBlack]);
    
    int end_frame = value + page_size;
    char buffer[16];    
    for (int i = frame; i < end_frame; i += fpb) {
        int x = int(((i-value)*scale)+0.5);
        int bar = i / fpb;
        if (!(bar % 4)) {
            window->draw_rectangle(gc, true, x, 0, 1, height);
            sprintf(buffer, "%i", bar);
            pango_layout->set_text(buffer);
            window->draw_layout(gc, x+2, 0, pango_layout);
        } else {
            window->draw_rectangle(gc, true, x, height/2, 1, height/2);
        }
    }
    
    window->draw_rectangle(gc, true, 0, height-1, width, 1);
    
    return true;
}

void MeasureView::on_seek(int x) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    double value = adjustment->get_value();
    double page_size = adjustment->get_page_size();
    double scale = (double)width / page_size;
    int frame = (x / scale) + value;
    _seek_request(frame);
}

bool MeasureView::on_motion_notify_event(GdkEventMotion *event) {
    
    return false;
}

bool MeasureView::on_button_press_event(GdkEventButton* event) {
    on_seek(event->x);
    return false;
}

bool MeasureView::on_button_release_event(GdkEventButton* event) {
    return false;
}

bool MeasureView::on_key_press_event(GdkEventKey* event) {
    return false;
}

bool MeasureView::on_key_release_event(GdkEventKey* event) {
    return false;
}

void MeasureView::on_size_allocate(Gtk::Allocation& allocation) {
    set_allocation(allocation);
    
    if (window) {
        window->move_resize(allocation.get_x(), allocation.get_y(),
            allocation.get_width(), allocation.get_height());
    }  
}

void MeasureView::invalidate() {
    if (!window)
        return;
    
    window->invalidate(true);
}

void MeasureView::on_adjustment_value_changed() {
    invalidate();
}

void MeasureView::set_adjustment(Gtk::Adjustment *adjustment) {
    this->adjustment = adjustment;
    if (adjustment) {
        adjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &MeasureView::on_adjustment_value_changed));
    }
}

MeasureView::type_seek_request MeasureView::signal_seek_request() {
    return _seek_request;
}

} // namespace Jacker
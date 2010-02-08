#include "measure.hpp"

namespace Jacker {

enum {
    ColorBlack = 0,
    ColorWhite,
    ColorBackground,

    ColorCount,
};

MeasureView::MeasureView(BaseObjectType* cobject, 
                         const Glib::RefPtr<Gtk::Builder>& builder) {
    adjustment = NULL;
    colors.resize(ColorCount);
    colors[ColorBlack].set("#000000");
    colors[ColorWhite].set("#FFFFFF");
    colors[ColorBackground].set("#e0e0e0");
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
    
    Pango::FontDescription font_desc("sans 8");
    
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
    
    return true;
}

bool MeasureView::on_motion_notify_event(GdkEventMotion *event) {
    return false;
}

bool MeasureView::on_button_press_event(GdkEventButton* event) {
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

} // namespace Jacker
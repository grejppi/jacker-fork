#include "patternview.hpp"
#include "model.hpp"

namespace Jacker {

//=============================================================================

PatternView::PatternView(BaseObjectType* cobject, 
             const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    model = NULL;
    pattern = NULL;
}

void PatternView::select_pattern(Model &model, Pattern &pattern) {
    this->model = &model;
    this->pattern = &pattern;
}

void PatternView::on_realize() {
    Gtk::Widget::on_realize();
}

bool PatternView::on_expose_event(GdkEventExpose* event) {
    Glib::RefPtr<Gdk::Window> window = get_window();
    
    Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
    if (event)
    {
        cr->rectangle(event->area.x, event->area.y,
                      event->area.width, event->area.height);
        cr->clip();
    }
    
    //Gdk::Cairo::set_source_color(cr, get_style()->get_bg(Gtk::STATE_NORMAL));
    cr->set_source_rgb(1,1,1);
    cr->paint();

    return true;
}

//=============================================================================

} // namespace Jacker
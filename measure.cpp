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
    orientation = OrientationHorizontal;
    colors.resize(ColorCount);
    colors[ColorBlack].set("#000000");
    colors[ColorWhite].set("#FFFFFF");
    colors[ColorBackground].set("#FFFFFF");
}

void MeasureView::set_model(class Model &model) {
    this->model = &model;
}

void MeasureView::set_orientation(Orientation orientation) {
    this->orientation = orientation;
    invalidate();
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

void MeasureView::flip(int &x, int &y) {
    if (orientation == OrientationHorizontal)
        return;
    std::swap(x,y);
}

void MeasureView::flip(int &x, int &y, int &w, int &h) {
    flip(x,y);
    flip(w,h);
}

bool MeasureView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);

    flip(width,height);
    
    // clear screen
    gc->set_foreground(colors[ColorBackground]);
    
    int x,y,w,h;
    x = 0; y = 0; w = width; h = height;
    flip(x,y,w,h);
    window->draw_rectangle(gc, true, x, y, w, h);
    
    double value = adjustment->get_value();
    double page_size = adjustment->get_page_size();
    double scale = (double)width / page_size;
    
    int fpbar = model->get_frames_per_bar();
    
    int stepsize = fpbar;
    int majorstep = 4;
    if (scale > 4.0) {
        stepsize = model->frames_per_beat;
        majorstep = model->beats_per_bar;
    }
    
    int frame = (int)value;
    frame -= (frame%stepsize);
    
    gc->set_foreground(colors[ColorBlack]);

    // measures
    int end_frame = value + page_size;
    char buffer[16];    
    for (int i = frame; i < end_frame; i += stepsize) {
        int c = int(((i-value)*scale)+0.5);
        
        int step = i / stepsize;
        if (!(step % majorstep)) {
            x = c; y = 0; w = 1; h = height; flip(x,y,w,h);
            window->draw_rectangle(gc, true, x, y, w, h);
            int bar = i / fpbar;
            sprintf(buffer, "%i", bar);
            pango_layout->set_text(buffer);
            x = c+2; y = 0; flip(x,y);
            window->draw_layout(gc, x, y, pango_layout);
        } else {
            x = c; y = height/2; w = 1; h = height/2; flip(x,y,w,h);
            window->draw_rectangle(gc, true, x, y, w, h);
        }
    }
    
    // loop
    if ((orientation == OrientationHorizontal) && model->enable_loop) {
        int lc1 = int(((model->loop.get_begin()-value)*scale)+0.5);
        int lc2 = int(((model->loop.get_end()-value)*scale)+0.5);
        
        std::vector<Gdk::Point> points;
        x = lc1; y = height-1; flip(x,y);
        points.push_back(Gdk::Point(x,y));
        x = lc1+8; y = height-1; flip(x,y);
        points.push_back(Gdk::Point(x,y));
        x = lc1; y = height-9; flip(x,y);
        points.push_back(Gdk::Point(x,y));
        window->draw_polygon(gc, true, points);
        points.clear();
        x = lc2; y = height-1; flip(x,y);
        points.push_back(Gdk::Point(x,y));
        x = lc2-8; y = height-1; flip(x,y);
        points.push_back(Gdk::Point(x,y));
        x = lc2; y = height-9; flip(x,y);
        points.push_back(Gdk::Point(x,y));
        window->draw_polygon(gc, true, points);
    }

    // bottom bar
    x = 0; y = height-1; w = width; h = 1; flip(x,y,w,h);
    window->draw_rectangle(gc, true, x,y,w,h);
    
    return true;
}

int MeasureView::get_frame(int x, int y) {
    flip(x,y);
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    flip(width, height);
    double value = adjustment->get_value();
    double page_size = adjustment->get_page_size();
    double scale = (double)width / page_size;
    int frame = (x / scale) + value; 
    frame -= frame % model->get_frames_per_bar();
    return frame;
}

bool MeasureView::on_motion_notify_event(GdkEventMotion *event) {
    
    return false;
}

bool MeasureView::on_button_press_event(GdkEventButton* event) {
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    bool alt_down = event->state & Gdk::MOD1_MASK;
    int frame = get_frame(event->x,event->y);
    if (ctrl_down) {
        model->loop.set_begin(frame);
        invalidate();
        _loop_changed();
    } else if (alt_down) {
        model->loop.set_end(frame);
        invalidate();
        _loop_changed();
    } else {
        _seek_request(frame);
    }
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

MeasureView::type_loop_changed MeasureView::signal_loop_changed() {
    return _loop_changed;
}

} // namespace Jacker
#include "measure.hpp"
#include "model.hpp"

#include <cassert>

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
    interact_mode = InteractNone;
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
    if (/*(orientation == OrientationHorizontal) &&*/ model->enable_loop) {
        int lc1 = int(((model->loop.get_begin()-value)*scale)+0.5);
        int lc2 = int(((model->loop.get_end()-value)*scale)+0.5);
        
        x = lc1; y = height-1;
        render_arrow(x,y, true);
        x = lc2; y = height-1;
        render_arrow(x,y, false);
    }

    // bottom bar
    x = 0; y = height-1; w = width; h = 1; flip(x,y,w,h);
    window->draw_rectangle(gc, true, x,y,w,h);
    
    return true;
}

Gdk::Point MeasureView::flip(const Gdk::Point &pt) {
    int x = pt.get_x();
    int y = pt.get_y();
    flip(x,y);
    return Gdk::Point(x,y);
}

void MeasureView::render_arrow(int x, int y, bool begin) {
    std::vector<Gdk::Point> points;
    y -= 1;
    const int R = 5;
    points.push_back(flip(Gdk::Point(x,y)));
    points.push_back(flip(Gdk::Point(x+R,y-R)));
    points.push_back(flip(Gdk::Point(x+R,y-R*3)));
    points.push_back(flip(Gdk::Point(x-R,y-R*3)));
    points.push_back(flip(Gdk::Point(x-R,y-R)));
    
    gc->set_foreground(colors[ColorWhite]);
    window->draw_polygon(gc, true, points);
    gc->set_foreground(colors[ColorBlack]);
    window->draw_polygon(gc, false, points);

    points.clear();
    if (begin) {
        points.push_back(flip(Gdk::Point(x-R+2,y-R-2)));
        points.push_back(flip(Gdk::Point(x+R-2,y-R-2)));
        points.push_back(flip(Gdk::Point(x-R+2,y-R*3+2)));
        window->draw_polygon(gc, false, points);
    } else {
        points.push_back(flip(Gdk::Point(x+R-1,y-R-1)));
        points.push_back(flip(Gdk::Point(x-R+1,y-R-1)));
        points.push_back(flip(Gdk::Point(x+R-1,y-R*3+1)));
        window->draw_polygon(gc, true, points);
    }
}

int MeasureView::get_pixel(int frame) {
    assert(adjustment);
    if (!window)
        return 0;
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    flip(width, height);
    double value = adjustment->get_value();
    double page_size = adjustment->get_page_size();
    double scale = (double)width / page_size;
    return int((frame - value)*scale+0.5);
}

int MeasureView::get_frame(int x, int y) {
    assert(adjustment);
    if (!window)
        return 0;
    flip(x,y);
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    flip(width, height);
    double value = adjustment->get_value();
    double page_size = adjustment->get_page_size();
    double scale = (double)width / page_size;
    return ((x / scale) + value);
}

int MeasureView::quantize_frame(int frame) {
    assert(model);
    int step = model->get_frames_per_bar();
    frame += step / 2;
    frame -= frame % step;
    return frame;
}

bool MeasureView::hit_loop_begin(int x) {
    if (!model->enable_loop)
        return false;
    return (std::abs(get_pixel(model->loop.get_begin()) - x) <= 4);
}

bool MeasureView::hit_loop_end(int x) {
    if (!model->enable_loop)
        return false;
    return (std::abs(get_pixel(model->loop.get_end()) - x) <= 4);
}

bool MeasureView::on_button_press_event(GdkEventButton* event) {
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    bool alt_down = event->state & Gdk::MOD1_MASK;
    int frame = get_frame(event->x,event->y);
    int x = event->x;
    int y = event->y;
    flip(x,y);
    if (ctrl_down) {
        model->loop.set_begin(quantize_frame(frame));
        invalidate();
        _loop_changed();
    } else if (alt_down) {
        model->loop.set_end(quantize_frame(frame));
        invalidate();
        _loop_changed();
    } else {
        drag.start(event->x, event->y);
        interact_mode = InteractDrag;
    }
    return false;
}

bool MeasureView::on_motion_notify_event(GdkEventMotion *event) {
    if (interact_mode == InteractDrag) {
        drag.update(event->x, event->y);
        if (drag.threshold_reached()) {
            int x = drag.start_x;
            int y = drag.start_y;
            flip(x,y);
            if (hit_loop_begin(x))
                interact_mode = InteractDragLoopBegin;
            else if (hit_loop_end(x))
                interact_mode = InteractDragLoopEnd;
            else
                interact_mode = InteractSeek;
        }
    }
    if (interact_mode == InteractDragLoopBegin) {
        int frame = quantize_frame(get_frame(event->x,event->y));
        if (frame != model->loop.get_begin()) {
            model->loop.set_begin(frame);
            invalidate();
            _loop_changed();
            return true;
        }
    } else if (interact_mode == InteractDragLoopEnd) {
        int frame = quantize_frame(get_frame(event->x,event->y));
        if (frame != model->loop.get_end()) {
            model->loop.set_end(frame);
            invalidate();
            _loop_changed();
            return true;
        }
    } else if (interact_mode == InteractNone) {
        int x = event->x;
        int y = event->y;
        flip(x,y);
        if (hit_loop_begin(x)||hit_loop_end(x))
            window->set_cursor(Gdk::Cursor(Gdk::HAND1));
        else
            window->set_cursor(Gdk::Cursor(Gdk::ARROW));
    }
    return false;
}

bool MeasureView::on_button_release_event(GdkEventButton* event) {
    if ((interact_mode == InteractSeek)||(interact_mode == InteractDrag)) {
        int frame = get_frame(event->x,event->y);
        _seek_request(quantize_frame(frame));
    }
    interact_mode = InteractNone;
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
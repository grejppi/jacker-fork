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

TrackCursor::TrackCursor(TrackView &view) {
    this->view = &view;
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
    origin_x = 0;
    origin_y = 0;
    model = NULL;
    zoomlevel = 0;
    hadjustment = NULL;
    vadjustment = NULL;
    snap_mode = SnapBar;
    interact_mode = InteractNone;
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
    
    Pango::FontDescription font_desc("sans 7");
    
    pango_layout = Pango::Layout::create(get_pango_context());
    pango_layout->set_font_description(font_desc);
    //pango_layout->set_width(-1);
    pango_layout->set_ellipsize(Pango::ELLIPSIZE_MIDDLE);
    
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

bool TrackView::find_event(const TrackCursor &cur, Song::iterator &event) {
    if (cur.get_track() >= model->get_track_count())
        return false;
    Song::IterList events;
    model->song.find_events(cur.get_frame(), events);
    if (events.empty())
        return false;
    for (Song::IterList::reverse_iterator iter = events.rbegin();
        iter != events.rend(); ++iter) {
        if ((*iter)->second.track == cur.get_track()) {
            event = *iter;
            return true;
        }
    }
    return false;
}

void TrackView::render_event(Song::iterator event) {
    bool selected = is_event_selected(event);
    int x,y,w,h;
    get_event_rect(event, x, y, w, h);
    // main border
    gc->set_foreground(colors[ColorBlack]);
    window->draw_rectangle(gc, false, x, y+1, w, h-3);
    // bottom shadow
    window->draw_rectangle(gc, true, x+1, y+h-1, w, 1);
    // right shadow
    window->draw_rectangle(gc, true, x+w+1, y+2, 1, h-2);
    pango_layout->set_width((w-4)*Pango::SCALE);
    pango_layout->set_text(event->second.pattern->name.c_str());
    if (selected) {
        // fill
        window->draw_rectangle(gc, true, x+2, y+3, w-3, h-6);
        // outline
        gc->set_foreground(colors[ColorWhite]);
        window->draw_rectangle(gc, false, x+1, y+2, w-2, h-5);
        // label
        window->draw_layout(gc, x+3, y+5, pango_layout);
    } else {
        // fill
        gc->set_foreground(colors[ColorWhite]);
        window->draw_rectangle(gc, true, x+1, y+2, w-1, h-4);
        // label
        gc->set_foreground(colors[ColorBlack]);
        window->draw_layout(gc, x+3, y+5, pango_layout);
    }
}

void TrackView::render_track(int track) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    int x,y;
    get_event_pos(0, track, x, y);
    
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
    for (int track = 0; track < model->get_track_count(); ++track) {
        render_track(track);
    }
    
    render_loop();
    
    // second pass: render events
    
    for (Song::iterator iter = model->song.begin();
         iter != model->song.end(); ++iter) {
        render_event(iter);
    }

    // draw play cursor
    int play_x, play_y;
    get_event_pos(play_position, 0, play_x, play_y);
    window->draw_rectangle(xor_gc, true, play_x, 0, 2, height);
    
    if (selecting()) {
        render_select_box();
    }

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

bool TrackView::is_event_selected(Song::iterator event) {
    return (std::find(selection.begin(), selection.end(), 
        event) != selection.end());
}

void TrackView::clear_selection() {
    invalidate_selection();
    selection.clear();
}

void TrackView::select_event(Song::iterator event) {
    if (is_event_selected(event))
        return;
    selection.push_back(event);
    invalidate_selection();
}

void TrackView::deselect_event(Song::iterator event) {
    if (!is_event_selected(event))
        return;
    invalidate_selection();
    selection.remove(event);
}

void TrackView::add_track() {
    // TODO
    //model->new_track();
    invalidate();
}

void TrackView::new_pattern(const TrackCursor &cur) {
    if (cur.get_track() < model->get_track_count()) {
        Pattern &pattern = model->new_pattern();
        Song::iterator event = model->song.add_event(cur.get_frame(), cur.get_track(), pattern);
        clear_selection();
        select_event(event);
    }
    
}

void TrackView::edit_pattern(Song::iterator iter) {
    Pattern *pattern = iter->second.pattern;
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

bool TrackView::selecting() const {
    return (interact_mode == InteractSelect);
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
        TrackCursor cur(*this);
        cur.set_pos(event->x, event->y);
        Song::iterator evt;

        if (!ctrl_down && (selection.size() == 1))
            clear_selection();
        
        if (find_event(cur, evt)) {
            if (ctrl_down && is_event_selected(evt)) {
                deselect_event(evt);
            } else {
                select_event(evt);
                if (double_click) {
                    interact_mode = InteractNone;
                    edit_pattern(evt);
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
            interact_mode = InteractSelect;
            drag.start(event->x, event->y);
        }
    } else if (event->button == 3) {
        _signal_context_menu(this, event);
    }
    return true;
}

bool TrackView::can_resize_event(Song::iterator event, int x) {
    int ex, ey, ew, eh;
    get_event_rect(event,ex,ey,ew,eh);
    return std::abs(x - (ex+ew)) < ResizeThreshold;
}

void TrackView::set_loop(const Loop &loop) {
    invalidate_loop();
    this->loop = loop;
    invalidate_loop();
}

void TrackView::render_loop() {
    if (!model->enable_loop)
        return;
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    gc->set_foreground(colors[ColorBlack]);
    int x, y;
    get_event_pos(loop.get_begin(), 0, x, y);
    window->draw_rectangle(gc, true, x, 0, 1, height);
    get_event_pos(loop.get_end(), 0, x, y);
    window->draw_rectangle(gc, true, x, 0, 1, height);
}

void TrackView::invalidate_loop() {
    if (!window)
        return;
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    int x, y;
    get_event_pos(loop.get_begin(), 0, x, y);
    window->invalidate_rect(Gdk::Rectangle(x,0,1,height), true);
    get_event_pos(loop.get_end(), 0, x, y);
    window->invalidate_rect(Gdk::Rectangle(x,0,1,height), true);
}

void TrackView::render_select_box() {
    int x,y,w,h;
    drag.get_rect(x,y,w,h);
    if (w <= 1)
        return;
    if (h <= 1)
        return;
    window->draw_rectangle(xor_gc, false, 
        x,y,w-1,h-1);
}

void TrackView::invalidate_select_box() {
    int x,y,w,h;
    drag.get_rect(x,y,w,h);
    if (w <= 1)
        return;
    if (h <= 1)
        return;
    Gdk::Rectangle rect(x,y,w,h);
    window->invalidate_rect(rect, true);
}
    
void TrackView::select_from_box() {
    int x0,y0,w,h;
    drag.get_rect(x0,y0,w,h);
    if (w <= 1)
        return;
    if (h <= 1)
        return;
    int x1 = x0+w;
    int y1 = y0+h;
    
    Song::iterator iter;
    for (iter = model->song.begin(); iter != model->song.end(); ++iter) {
        int ex0,ey0,ew,eh;
        get_event_rect(iter, ex0, ey0, ew, eh);
        int ex1 = ex0+ew;
        int ey1 = ey0+eh;
        if (ex0 >= x1)
            continue;
        if (ex1 < x0)
            continue;
        if (ey0 >= y1)
            continue;
        if (ey1 < y0)
            continue;
        selection.push_back(iter);
    }
}

void TrackView::clone_selection(bool references/*=false*/) {
    Song::IterList new_selection;
    
    Song::IterList::iterator iter;
    for (iter = selection.begin(); iter != selection.end(); ++iter) {
        Song::Event event = (*iter)->second;
        if (!references) {
            event.pattern = &model->new_pattern(event.pattern);
        }
        new_selection.push_back(model->song.add_event(event));
    }
    
    selection = new_selection;
}

bool TrackView::on_motion_notify_event(GdkEventMotion *event) {    
    bool shift_down = event->state & Gdk::SHIFT_MASK;
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    
    if (interact_mode == InteractNone) {
        TrackCursor cur(*this);
        cur.set_pos(event->x, event->y);
        Song::iterator evt;
        if (find_event(cur, evt)) {
            if (can_resize_event(evt, event->x))
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
            TrackCursor cur(*this);
            cur.set_pos(drag.start_x, drag.start_y);
            Song::iterator evt;
            if (find_event(cur, evt) && can_resize_event(evt,drag.start_x)) {
                interact_mode = InteractResize;
            } else {
                if (shift_down) {
                    invalidate_selection();
                    clone_selection(ctrl_down);
                }
                interact_mode = InteractMove;
            }
        }
    }
    if (resizing()||moving()) {
        invalidate_selection();
        drag.update(event->x, event->y);
        invalidate_selection();
    }
    else if (selecting()) {
        invalidate_select_box();
        drag.update(event->x, event->y);
        invalidate_select_box();
    }
    return true;
}

void TrackView::apply_move() {
    invalidate_selection();
    
    int ofs_frame,ofs_track;
    get_drag_offset(ofs_frame, ofs_track);
    
    bool can_move = true;        
    // verify that we can move
    for (Song::IterList::iterator iter = selection.begin();
        iter != selection.end(); ++iter) {
        Song::Event &event = (*iter)->second;
        int frame = event.frame + ofs_frame;
        int track = event.track + ofs_track;
        if ((frame < 0)||(track < 0)||(track >= model->get_track_count())) {
            can_move = false;
            break;
        }
    }
    
    if (can_move) {
        Song::IterList new_selection;
        
        // do the actual move
        for (Song::IterList::iterator iter = selection.begin();
            iter != selection.end(); ++iter) {
            Song::Event event = (*iter)->second;
            
            event.frame += ofs_frame;
            event.track += ofs_track;
            
            model->song.erase(*iter);
            Song::iterator new_event = model->song.add_event(event);
            new_selection.push_back(new_event);
        }
        
        selection = new_selection;
    }
}

void TrackView::apply_resize() {
    
    int ofs_frame,ofs_track;
    get_drag_offset(ofs_frame, ofs_track);
    
    // verify that we can move
    for (Song::IterList::iterator iter = selection.begin();
        iter != selection.end(); ++iter) {
        Song::Event &event = (*iter)->second;
        int length = std::max(event.pattern->get_length() + ofs_frame, 
            get_step_size());
            
        event.pattern->set_length(length);
    }
    
    invalidate();
}

bool TrackView::on_button_release_event(GdkEventButton* event) {
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    if (moving()) {
        apply_move();
    } else if (resizing()) {
        apply_resize();
    } else if (selecting()) {
        invalidate_select_box();
        if (!ctrl_down) {
            invalidate_selection();
            selection.clear();
        }
        select_from_box();
    }
    interact_mode = InteractNone;
    invalidate_selection();
    return false;
}

bool TrackView::on_key_press_event(GdkEventKey* event) {
    switch (event->keyval) {
        case GDK_Delete: erase_events(); break;
        case GDK_Return: {
            if (selection.size() == 1)
                edit_pattern(selection.front());
        } break;
        default: break;
    }
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

void TrackView::get_event_rect(Song::iterator iter, int &x, int &y, int &w, int &h) {
    Song::Event &event = iter->second;
    int frame = event.frame;
    int track = event.track;
    int length = event.pattern->get_length();
    if (moving() && is_event_selected(iter)) {
        int ofs_frame,ofs_track;
        get_drag_offset(ofs_frame, ofs_track);
        frame += ofs_frame;
        track += ofs_track;
    } else if (resizing() && is_event_selected(iter)) {
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
    Song::IterList::iterator iter;
    for (iter = selection.begin(); iter != selection.end(); ++iter) {
        int x,y,w,h;
        get_event_rect(*iter, x, y, w, h);
        w += 2;
        Gdk::Rectangle rect(x,y,w,h);
        window->invalidate_rect(rect, true);
    }
}

void TrackView::erase_events() {
    invalidate_selection();
    Song::IterList::iterator iter;
    for (iter = selection.begin(); iter != selection.end(); ++iter) {
        model->song.erase(*iter);
    }
    
    selection.clear();
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
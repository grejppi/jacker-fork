#include "songview.hpp"
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

SongCursor::SongCursor() {
    view = NULL;
    track = 0;
    frame = 0;
}

SongCursor::SongCursor(SongView &view) {
    this->view = &view;
    track = 0;
    frame = 0;
}

void SongCursor::set_view(SongView &view) {
    this->view = &view;
}

SongView *SongCursor::get_view() const {
    return view;
}

void SongCursor::set_track(int track) {
    this->track = track;
}

int SongCursor::get_track() const {
    return track;
}

void SongCursor::set_frame(int frame) {
    this->frame = frame;
}

int SongCursor::get_frame() const {
    return frame;
}

void SongCursor::get_pos(int &x, int &y) const {
    assert(view);
    view->get_event_pos(frame, track, x, y);
}

void SongCursor::set_pos(int x, int y) {
    assert(view);
    view->get_event_location(x, y, frame, track);
}

//=============================================================================

SongView::SongView(BaseObjectType* cobject, 
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

void SongView::set_model(class Model &model) {
    this->model = &model;
}

void SongView::on_realize() {
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

void SongView::set_origin(int x, int y) {
    origin_x = x;
    origin_y = y;
}

void SongView::get_origin(int &x, int &y) const {
    x = origin_x;
    y = origin_y;
}

void SongView::get_event_pos(int frame, int track,
                            int &x, int &y) const {
    x = origin_x + (frame>>zoomlevel);
    y = origin_y + track*TrackHeight;
}


void SongView::get_event_size(int length, int &w, int &h) const {
    w = length>>zoomlevel;
    h = TrackHeight;
}

void SongView::get_event_length(int w, int h, int &length, int &track) const {
    length = w<<zoomlevel;
    track = h/TrackHeight;
}

void SongView::get_event_location(int x, int y, int &frame, int &track) const {
    frame = (x - origin_x)<<zoomlevel;
    track = (y - origin_y)/TrackHeight;
}

bool SongView::find_event(const SongCursor &cur, Song::iterator &event) {
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

void SongView::render_event(Song::iterator event) {
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
        // TODO: make this fast
        window->draw_layout(gc, x+3, y+5, pango_layout);
    } else {
        // fill
        gc->set_foreground(colors[ColorWhite]);
        window->draw_rectangle(gc, true, x+1, y+2, w-1, h-4);
        // label
        gc->set_foreground(colors[ColorBlack]);
        // TODO: make this fast
        window->draw_layout(gc, x+3, y+5, pango_layout);
    }
}

void SongView::render_track(int track) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    int x,y;
    get_event_pos(0, track, x, y);
    
    gc->set_foreground(colors[ColorTrack]);
    window->draw_rectangle(gc, true, 0, y, width, TrackHeight);
}

bool SongView::on_expose_event(GdkEventExpose* event) {
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

void SongView::invalidate_play_position() {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    int play_x, play_y;
    get_event_pos(play_position, 0, play_x, play_y);
    Gdk::Rectangle rect(play_x,0,2,height);
    window->invalidate_rect(rect, true);
}

bool SongView::is_event_selected(Song::iterator event) {
    return (std::find(selection.begin(), selection.end(), 
        event) != selection.end());
}

void SongView::clear_selection() {
    invalidate_selection();
    selection.clear();
}

void SongView::select_event(Song::iterator event) {
    if (is_event_selected(event))
        return;
    selection.push_back(event);
    invalidate_selection();
}

void SongView::deselect_event(Song::iterator event) {
    if (!is_event_selected(event))
        return;
    invalidate_selection();
    selection.remove(event);
}

void SongView::add_track() {
    Track track;
    model->tracks.push_back(track);
    invalidate();
    _tracks_changed();
}

void SongView::new_pattern(const SongCursor &cur) {
    if (cur.get_track() >= model->get_track_count())
        return;
    int frame = cur.get_frame();
    int track = cur.get_track();
    Pattern &pattern = model->new_pattern();
    if (model->enable_loop && (frame >= model->loop.get_begin()) &&
        (frame < model->loop.get_end())) {
        frame = model->loop.get_begin();
        pattern.set_length(model->loop.get_end() - model->loop.get_begin());
    }
    Song::iterator event = model->song.add_event(frame, track, pattern);
    clear_selection();
    select_event(event);    
}

void SongView::edit_pattern(Song::iterator iter) {
    _pattern_edit_request(iter);
}


bool SongView::dragging() const {
    return (interact_mode == InteractDrag);
}

bool SongView::moving() const {
    return (interact_mode == InteractMove);
}

bool SongView::resizing() const {
    return (interact_mode == InteractResize);
}

bool SongView::selecting() const {
    return (interact_mode == InteractSelect);
}

bool SongView::on_button_press_event(GdkEventButton* event) {
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    bool shift_down = event->state & Gdk::SHIFT_MASK;
    /*
    bool alt_down = event->state & Gdk::MOD1_MASK;
    bool super_down = event->state & (Gdk::SUPER_MASK|Gdk::MOD4_MASK);
    */
    bool double_click = (event->type == GDK_2BUTTON_PRESS);
    
    grab_focus();
    
    if (event->button == 1) {
        SongCursor cur(*this);
        cur.set_pos(event->x, event->y);
        Song::iterator evt;

        if (find_event(cur, evt)) {
            if (!is_event_selected(evt)) {
                if (!ctrl_down)
                    clear_selection();
                select_event(evt);
            } else if (ctrl_down && !shift_down) {
                deselect_event(evt);
            }
            if (double_click) {
                interact_mode = InteractNone;
                edit_pattern(evt);
            } else {
                interact_mode = InteractDrag;
                drag.start(event->x, event->y);
            }
        } else if (double_click) {
            interact_mode = InteractNone;
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

bool SongView::can_resize_event(Song::iterator event, int x) {
    int ex, ey, ew, eh;
    get_event_rect(event,ex,ey,ew,eh);
    return std::abs(x - (ex+ew)) < ResizeThreshold;
}

void SongView::set_loop(const Loop &loop) {
    invalidate_loop();
    this->loop = loop;
    invalidate_loop();
}

void SongView::render_loop() {
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

void SongView::invalidate_loop() {
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

void SongView::render_select_box() {
    int x,y,w,h;
    drag.get_rect(x,y,w,h);
    if (w <= 1)
        return;
    if (h <= 1)
        return;
    window->draw_rectangle(xor_gc, false, 
        x,y,w-1,h-1);
}

void SongView::invalidate_select_box() {
    int x,y,w,h;
    drag.get_rect(x,y,w,h);
    if (w <= 1)
        return;
    if (h <= 1)
        return;
    Gdk::Rectangle rect(x,y,w,h);
    window->invalidate_rect(rect, true);
}
    
void SongView::select_from_box() {
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

void SongView::clone_selection(bool references/*=false*/) {
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

bool SongView::on_motion_notify_event(GdkEventMotion *event) {    
    bool shift_down = event->state & Gdk::SHIFT_MASK;
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    
    if (interact_mode == InteractNone) {
        SongCursor cur(*this);
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
            SongCursor cur(*this);
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

void SongView::apply_move() {
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
            
            _pattern_erased(*iter);
            model->song.erase(*iter);
            Song::iterator new_event = model->song.add_event(event);
            new_selection.push_back(new_event);
        }
        
        selection = new_selection;
    }
}

void SongView::apply_resize() {
    
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

bool SongView::on_button_release_event(GdkEventButton* event) {
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

void SongView::select_first() {
    if (model->song.empty())
        return;
    clear_selection();
    select_event(model->song.begin());
}

void SongView::select_last() {
    if (model->song.empty())
        return;
    Song::iterator iter = model->song.end();
    iter--;
    clear_selection();
    select_event(iter);
}

Song::iterator SongView::get_left_event(Song::iterator start) {
    Song::iterator not_found = model->song.end();
    if (start == model->song.begin())
        return not_found;

    Song::iterator iter = start;
    do {
        iter--;
        if (iter->first == start->first)
            continue;
        if (iter->second.track == start->second.track)
            return iter;
    } while(iter != model->song.begin());
    
    return not_found;
}

Song::iterator SongView::get_right_event(Song::iterator start) {
    Song::iterator not_found = model->song.end();

    Song::iterator iter = start;
    iter++;
    while (iter != model->song.end()) {
        if (iter->first != start->first) {
            if (iter->second.track == start->second.track)
                return iter;
        }
        iter++;
    }
    
    return not_found;
}


Song::iterator SongView::nearest_y_event(Song::iterator start, int direction) {
    Song::iterator not_found = model->song.end();
    
    int best_delta_x = 9999999;
    int best_delta_y = 9999999;
    Song::iterator best_iter = not_found;
    
    for (Song::iterator iter = model->song.begin();
         iter != model->song.end(); ++iter) {
        int delta_x = iter->second.track - start->second.track;
        int delta_y = iter->first - start->first;
        delta_x *= direction;
        if (delta_x <= 0)
            continue;
        if (delta_x > best_delta_x)
            continue;
        delta_y = std::abs(delta_y);
        if (delta_x == best_delta_x) {
            if (delta_y >= best_delta_y)
                continue;
        }
        best_delta_x = delta_x;
        best_delta_y = delta_y;
        best_iter = iter;
    }
    
    return best_iter;
}

void SongView::navigate(int dir_x, int dir_y) {
    if (selection.empty()) {
        select_first();
        return;
    }
    Song::iterator iter = selection.back();
    
    if (dir_x < 0)
        iter = get_left_event(iter);
    else if (dir_x > 0)
        iter = get_right_event(iter);
    else if (dir_y)
        iter = nearest_y_event(iter, dir_y);
    
    if (iter != model->song.end()) {
        clear_selection();
        select_event(iter);
    }
}

int SongView::get_selection_begin() {
    if (selection.empty())
        return -1;
    int best = selection.front()->first;
    for (Song::IterList::iterator iter = selection.begin();
      iter != selection.end(); ++iter) {
        best = std::min(best, (*iter)->first);
    }
    return best;
}

int SongView::get_selection_end() {
    if (selection.empty())
        return -1;
    int best = selection.front()->second.get_end();
    for (Song::IterList::iterator iter = selection.begin();
      iter != selection.end(); ++iter) {
        best = std::max(best, (*iter)->second.get_end());
    }
    return best;
}

void SongView::set_loop_begin() {
    int frame = get_selection_begin();
    if (frame == -1)
        return;
    invalidate_loop();    
    loop.set_begin(frame);
    model->loop = loop;
    _loop_changed();
    invalidate_loop();
}

void SongView::set_loop_end() {
    int frame = get_selection_end();
    if (frame == -1)
        return;
    invalidate_loop();
    loop.set_end(frame);
    model->loop = loop;
    _loop_changed();
    invalidate_loop();
}

void SongView::play_from_selection() {
    int frame = get_selection_begin();
    if (frame == -1)
        return;
    _play_request(frame);
}

bool SongView::on_key_press_event(GdkEventKey* event) {
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;

    if (ctrl_down) {
        switch (event->keyval) {
            case GDK_b: set_loop_begin(); return true;
            case GDK_e: set_loop_end(); return true;
            default: break;
        }
    } else {
        switch (event->keyval) {
            case GDK_Delete: erase_events(); return true;
            case GDK_Return: {
                if (selection.size() == 1)
                    edit_pattern(selection.front());
                return true;
            } break;
            case GDK_Left: navigate(-1,0); return true;
            case GDK_Right: navigate(1,0); return true;
            case GDK_Up: navigate(0,-1); return true;
            case GDK_Down: navigate(0,1); return true;
            case GDK_Home: select_first(); return true;
            case GDK_End: select_last(); return true;
            case GDK_F6: play_from_selection(); return true;
            default: break;
        }
    }
    return false;
}

bool SongView::on_key_release_event(GdkEventKey* event) {
    return false;
}

void SongView::on_size_allocate(Gtk::Allocation& allocation) {
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

void SongView::get_drag_offset(int &frame, int &track) {
    int dx,dy;
    drag.get_delta(dx,dy);
    get_event_length(dx, dy, frame, track);
    frame = quantize_frame(frame);
}

int SongView::get_step_size() {
    switch(snap_mode) {
        case SnapBar: return model->get_frames_per_bar();
        default: break;
    }
    return 1;
}

int SongView::quantize_frame(int frame) {
    return frame - (frame%get_step_size());
}

void SongView::get_event_rect(Song::iterator iter, int &x, int &y, int &w, int &h) {
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

void SongView::invalidate() {
    if (!window)
        return;
    window->invalidate(true);
}

void SongView::invalidate_selection() {
    Song::IterList::iterator iter;
    for (iter = selection.begin(); iter != selection.end(); ++iter) {
        int x,y,w,h;
        get_event_rect(*iter, x, y, w, h);
        w += 2;
        Gdk::Rectangle rect(x,y,w,h);
        window->invalidate_rect(rect, true);
    }
}

void SongView::erase_events() {
    invalidate_selection();
    Song::IterList::iterator iter;
    for (iter = selection.begin(); iter != selection.end(); ++iter) {
        _pattern_erased(*iter);
        model->song.erase(*iter);
    }
    
    selection.clear();
}

void SongView::set_play_position(int pos) {
    if (!window)
        return;
    if (pos == play_position)
        return;
    invalidate_play_position();
    play_position = pos;
    invalidate_play_position();
}

void SongView::update_adjustments() {
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

void SongView::on_adjustment_value_changed() {
    if (hadjustment) {
        int w,h;
        get_event_size((int)(hadjustment->get_value()+0.5),w,h);
        origin_x = -w;
    }
    
    invalidate();
}

void SongView::set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                     Gtk::Adjustment *vadjustment) {
    this->hadjustment = hadjustment;
    this->vadjustment = vadjustment;
    if (hadjustment) {
        hadjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &SongView::on_adjustment_value_changed));
    }
    if (vadjustment) {
        vadjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &SongView::on_adjustment_value_changed));
    }                                         
}

SongView::type_pattern_edit_request SongView::signal_pattern_edit_request() {
    return _pattern_edit_request;
}

SongView::type_context_menu SongView::signal_context_menu() {
    return _signal_context_menu;
}

SongView::type_loop_changed SongView::signal_loop_changed() {
    return _loop_changed;
}

SongView::type_play_request SongView::signal_play_request() {
    return _play_request;
}

SongView::type_pattern_erased SongView::signal_pattern_erased() {
    return _pattern_erased;
}

SongView::type_tracks_changed SongView::signal_tracks_changed() {
    return _tracks_changed;
}

//=============================================================================

} // namespace Jacker
#include "patternview.hpp"
#include "model.hpp"

#include <cassert>
#include <cmath>

/*
TODO:

- implement selection: cursor start -> cursor end
- implement basic navigation
- roll position on channel/row begin/end
- implement editing values
- implement pattern resize
- implement 
*/

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

void CellRenderer::render_background(PatternView &view, 
                                     PatternCursor &cursor,
                                     bool selected) {
    int row = cursor.get_row();
    int frames_per_bar = view.frames_per_beat * view.beats_per_bar;
    
    int color_base = 0;
    if (selected) {
        color_base = ColorSelBackground - ColorBackground;
    }
    if (!(row % frames_per_bar)) {
        view.gc->set_foreground(view.colors[color_base+ColorRowBar]);
    } else if (!(row % view.frames_per_beat)) {
        view.gc->set_foreground(view.colors[color_base+ColorRowBeat]);
    }
    else {
        return; // nothing to draw
    }
    
    int x, y, w, h;
    cursor.get_pos(x,y);
    cursor.get_cell_size(w,h);
    if (!cursor.is_last_param()) {
        w += cursor.get_layout()->get_cell_margin();
    }
    view.window->draw_rectangle(view.gc, true, x, y, w, h);
}

void CellRenderer::render_cell(PatternView &view, PatternCursor &cursor, 
                               Pattern::Event *event, bool draw_cursor, bool selected) {

}

int CellRenderer::get_width(const PatternLayout &layout) {
    return 0;
}

int CellRenderer::get_item(const PatternLayout &layout, int x) {
    return 0;
}

int CellRenderer::get_item_count() {
    return 0;
}

//=============================================================================

static const char *note_strings[] = {
    "C-",
    "C#",
    "D-",
    "D#",
    "E-",
    "F-",
    "F#",
    "G-",
    "G#",
    "A-",
    "A#",
    "B-",
};

static int sprint_note(char *buffer, int value) {
    if (value == ValueNone) {
        sprintf(buffer, "...");
    } else {
        int note = value % 12;
        int octave = value / 12;
        
        assert((size_t)note < (sizeof(note_strings)/sizeof(const char *)));
        sprintf(buffer, "%s%i", note_strings[note], octave);
    }
    return 3;
}

void CellRendererNote::render_cell(PatternView &view, PatternCursor &cursor, 
                                   Pattern::Event *event, bool draw_cursor, bool selected) {
                                       
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_note(text, value);
    
    render_background(view, cursor, selected);
    
    view.gc->set_foreground(view.colors[ColorBlack]);
    
    int x, y;
    cursor.get_pos(x,y);
    view.draw_text(x, y, text);
    
    if (draw_cursor) {
        int w,h;
        cursor.get_layout()->get_text_size(w,h);
        int item = cursor.get_item();
        if (item == 0)
            w *= 2;
        else if (item == 1)
            x += 2 * w;
        view.window->draw_rectangle(view.xor_gc, true, x, y, w, h);
    }
}

int CellRendererNote::get_width(const PatternLayout &layout) {
    int w,h;
    layout.get_text_size(w,h);
    return 3 * w;
}

int CellRendererNote::get_item(const PatternLayout &layout, int x) {
    int w,h;
    layout.get_text_size(w,h);
    int pos = x / w;
    if (pos < 2)
        return 0;
    else
        return 1;
}

int CellRendererNote::get_item_count() {
    return 2;
}

//=============================================================================

static int sprint_byte(char *buffer, int value) {
    if (value == ValueNone) {
        sprintf(buffer, "..");
    } else {
        sprintf(buffer, "%02X", value);
    }
    return 2;
}

void CellRendererByte::render_cell(PatternView &view, PatternCursor &cursor, 
                                   Pattern::Event *event, bool draw_cursor, bool selected) {
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_byte(text, value);
    
    render_background(view, cursor, selected);
    
    view.gc->set_foreground(view.colors[ColorBlack]);
    
    int x, y;
    cursor.get_pos(x,y);
    view.draw_text(x, y, text);

    if (draw_cursor) {
        int w,h;
        cursor.get_layout()->get_text_size(w,h);
        int item = cursor.get_item();
        x += w * item;
        view.window->draw_rectangle(view.xor_gc, true, x, y, w, h);
    }
}

int CellRendererByte::get_width(const PatternLayout &layout) {
    int w,h;
    layout.get_text_size(w,h);
    return 2 * w;
}

int CellRendererByte::get_item(const PatternLayout &layout, int x) {
    int w,h;
    layout.get_text_size(w,h);
    int pos = x / w;
    return std::min(pos, 1);
}

int CellRendererByte::get_item_count() {
    return 2;
}

//=============================================================================
    
PatternLayout::PatternLayout() {
    row_height = 0;
    origin_x = origin_y = 0;
    cell_margin = 0;
    channel_margin = 0;
    row_margin = 0;
    text_width = 0;
    text_height = 0;
    renderers.resize(ParamCount);
    for (size_t i = 0; i < renderers.size(); ++i) {
        renderers[i] = NULL;
    }
}

void PatternLayout::set_cell_renderer(int param, CellRenderer *renderer) {
    renderers[param] = renderer;
}

CellRenderer *PatternLayout::get_cell_renderer(
    int param) const {
    return renderers[param];
}

int PatternLayout::get_cell_count() const {
    return renderers.size();
}

void PatternLayout::set_origin(int x, int y) {
    origin_x = x;
    origin_y = y;
}

void PatternLayout::get_origin(int &x, int &y) {
    x = origin_x;
    y = origin_y;
}

void PatternLayout::set_cell_margin(int margin) {
    cell_margin = margin;
}

void PatternLayout::set_channel_margin(int margin) {
    channel_margin = margin;
}

void PatternLayout::set_row_margin(int margin) {
    row_margin = margin;
}

int PatternLayout::get_cell_margin() const {
    return cell_margin;
}

int PatternLayout::get_channel_margin() const {
    return channel_margin;
}

int PatternLayout::get_row_margin() const {
    return row_margin;
}

void PatternLayout::set_row_height(int height) {
    row_height = height;
}

int PatternLayout::get_row_height() const {
    return row_height;
}

void PatternLayout::get_cell_size(int param, int &w, int &h) const {
    w = 0;
    h = row_height;
    
    CellRenderer *renderer = renderers[param];
    if (renderer)
        w = renderer->get_width(*this);
}

int PatternLayout::get_param_offset(int param) const {
    int x = 0;
    for (size_t i = 0; i < (size_t)param; ++i) {
        CellRenderer *renderer = renderers[i];
        if (renderer)
            x += renderer->get_width(*this);
        if (i < (renderers.size()-1))
            x += cell_margin;
    }
    return x;
}

int PatternLayout::get_channel_width() const {
    return get_param_offset(renderers.size());
}

void PatternLayout::get_cell_pos(int row, int channel, int param,
                                       int &x, int &y) const {
    y = origin_y + row * (row_height + row_margin);
    x = origin_x + channel * (get_channel_width() + channel_margin) +
        get_param_offset(param);
    
}

bool PatternLayout::get_cell_location(int x, int y, int &row, int &channel,
    int &param, int &item) const {
    int channel_width = get_channel_width();
    assert(row_height);
    assert(channel_width);
    y -= origin_y;
    row = y / (row_height + row_margin);
    x -= origin_x;
    channel = x / (channel_width + channel_margin);
    x -= channel * (channel_width + channel_margin);
    param = renderers.size()-1;
    item = ((renderers[param])?(renderers[param]->get_item_count()-1):0);
    for (size_t i = 0; i < renderers.size(); ++i) {
        CellRenderer *renderer = renderers[i];
        if (renderer) {
            int width = renderer->get_width(*this);
            if (x < width) {
                param = i;
                item = renderer->get_item(*this,x);
                break;
            }
            x -= width;
        }
        if (i < (renderers.size()-1))
            x -= cell_margin;
    }
    return false;
}

void PatternLayout::set_text_size(int width, int height) {
    text_width = width;
    text_height = height;
}

void PatternLayout::get_text_size(int &width, int &height) const {
    width = text_width;
    height = text_height;
}

//=============================================================================

PatternCursor::PatternCursor() {
    row = 0;
    channel = 0;
    param = 0;
    item = 0;
    layout = NULL;
}

void PatternCursor::set_layout(PatternLayout &layout) {
    this->layout = &layout;
}

PatternLayout *PatternCursor::get_layout() const {
    assert(layout);
    return layout;
}

void PatternCursor::origin() {
    row = 0;
    channel = 0;
    param = 0;
    item = 0;
}

void PatternCursor::next_row() {
    row++;
    channel = 0;
    param = 0;
    item = 0;
}

void PatternCursor::next_channel() {
    channel++;
    param = 0;
    item = 0;
}

void PatternCursor::next_param() {
    param++;
    item = 0;
}

bool PatternCursor::is_last_param() const {
    return param == get_layout()->get_cell_count()-1;
}

int PatternCursor::get_row() const {
    return row;
}

int PatternCursor::get_channel() const {
    return channel;
}

int PatternCursor::get_param() const {
    return param;
}

int PatternCursor::get_column() const {
    assert(layout);
    return channel * layout->get_cell_count() + param;
}

void PatternCursor::get_pos(int &x, int &y) const {
    assert(layout);
    layout->get_cell_pos(row, channel, param, x, y);
}

void PatternCursor::set_pos(int x, int y) {
    assert(layout);
    layout->get_cell_location(x, y, row, channel, param, item);
}

void PatternCursor::get_cell_size(int &w, int &h) const {
    assert(layout);
    layout->get_cell_size(param, w, h);
}

int PatternCursor::get_item() const {
    return item;
}

void PatternCursor::set_row(int row) {
    this->row = row;
}

void PatternCursor::set_channel(int channel) {
    this->channel = channel;
}

void PatternCursor::set_param(int param) {
    this->param = param;
}

void PatternCursor::set_item(int item) {
    this->item = item;
}

void PatternCursor::set_last_item() {
    param = layout->get_cell_count()-1;
    item = 0;
    CellRenderer *renderer = layout->get_cell_renderer(param);
    if (renderer) {
        item = renderer->get_item_count()-1;
    }
}

// true if cursor shares row/channel/param with other cursor
bool PatternCursor::is_at(const PatternCursor &other) const {
    if (row != other.row)
        return false;
    if (channel != other.channel)
        return false;
    if (param != other.param)
        return false;
    return true;
}


//=============================================================================

PatternSelection::PatternSelection() {
    active = false;
}

void PatternSelection::set_active(bool active) {
    this->active = active;
}

bool PatternSelection::get_active() const {
    return active;
}

static bool test_range(int value, int r0, int r1) {
    if (r1 == r0)
        return false;
    if (r1 < r0) {
        int _temp = r0;
        r0 = r1;
        r1 = _temp;
    }
    return (value >= r0) && (value < r1);
}

bool PatternSelection::in_range(const PatternCursor &cursor) const {
    if (!active)
        return false;
    if (!test_range(cursor.get_row(), p0.get_row(), p1.get_row()))
        return false;
    if (!test_range(cursor.get_column(), p0.get_column(), p1.get_column()))
        return false;
    return true;
}

void PatternSelection::set_layout(PatternLayout &layout) {
    p0.set_layout(layout);
    p1.set_layout(layout);
}

//=============================================================================

PatternView::PatternView(BaseObjectType* cobject,
                         const Glib::RefPtr<Gtk::Builder>& builder)
  : Gtk::Widget(cobject) {
    model = NULL;
    pattern = NULL;
    frames_per_beat = 4;
    beats_per_bar = 4;
    hadjustment = 0;
    vadjustment = 0;
      
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

void PatternView::set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                         Gtk::Adjustment *vadjustment) {
    this->hadjustment = hadjustment;
    this->vadjustment = vadjustment;
    if (hadjustment) {
        hadjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &PatternView::on_adjustment_value_changed));
    }
    if (vadjustment) {
        vadjustment->signal_value_changed().connect(sigc::mem_fun(*this,
            &PatternView::on_adjustment_value_changed));
    }
}

void PatternView::select_pattern(Model &model, Pattern &pattern) {
    this->model = &model;
    this->pattern = &pattern;
    
    update_adjustments();
}

void PatternView::on_realize() {
    Gtk::Widget::on_realize();
    
    window = get_window();    
    
    // create drawing resources
    gc = Gdk::GC::create(window);
    cm = gc->get_colormap();
    
    for (std::vector<Gdk::Color>::iterator i = colors.begin(); 
         i != colors.end(); ++i) {
        cm->alloc_color(*i);
    }
    
    Pango::FontDescription font_desc("monospace 8");
    
    pango_layout = Pango::Layout::create(get_pango_context());
    pango_layout->set_font_description(font_desc);
    pango_layout->set_width(-1);
        
    // measure width of a single character
    pango_layout->set_text("W");
    int text_width, text_height;
    pango_layout->get_pixel_size(text_width, text_height);
    
    for (int i = CharBegin; i < CharEnd; ++i) {
        Glib::RefPtr<Gdk::Pixmap> pixmap = Gdk::Pixmap::create(
            window, text_width, text_height);
        
        char buffer[4];
        sprintf(buffer, "%c", (char)i);
        pango_layout->set_text(buffer);
        
        Glib::RefPtr<Gdk::GC> pm_gc = Gdk::GC::create(pixmap);
        pm_gc->set_colormap(cm);
        pm_gc->set_foreground(colors[ColorWhite]);
        pixmap->draw_rectangle(pm_gc, true, 0, 0, text_width, text_height);
        
        pm_gc->set_foreground(colors[ColorBlack]);
        pixmap->draw_layout(pm_gc, 0, 0, pango_layout);
        
        chars.push_back(pixmap);
    }
    
    // create xor gc for drawing the cursor
    xor_gc = Gdk::GC::create(window);
    Glib::RefPtr<Gdk::Colormap> xor_cm = xor_gc->get_colormap();
    Gdk::Color xor_color;
    xor_color.set("#ffffff"); xor_cm->alloc_color(xor_color);
    xor_gc->set_function(Gdk::XOR);
    xor_gc->set_foreground(xor_color);
    xor_gc->set_background(xor_color);


    // setup pattern layout
    layout.set_text_size(text_width, text_height);
    layout.set_origin(0,0);
    layout.set_cell_margin(5);
    layout.set_channel_margin(10);
    layout.set_row_height(text_height);
    
    layout.set_cell_renderer(ParamNote, &note_renderer);
    layout.set_cell_renderer(ParamVolume, &byte_renderer);
    layout.set_cell_renderer(ParamCCIndex0, &byte_renderer);
    layout.set_cell_renderer(ParamCCValue0, &byte_renderer);
    layout.set_cell_renderer(ParamCCIndex1, &byte_renderer);
    layout.set_cell_renderer(ParamCCValue1, &byte_renderer);
    
    cursor.set_layout(layout);
    selection.set_layout(layout);
    
    selection.p0.set_pos(100,100);
    selection.p1.set_pos(300,200);
 
    update_adjustments();
}

void PatternView::on_adjustment_value_changed() {
    int origin_x, origin_y;
    layout.get_origin(origin_x, origin_y);
    
    int x_scroll_value = origin_x;
    int y_scroll_value = origin_y;
    
    if (hadjustment) {
        int channel_width = layout.get_channel_width()+layout.get_channel_margin();
        x_scroll_value =
            int(hadjustment->get_value()+0.5)*channel_width;
    }
    if (vadjustment) {
        y_scroll_value = 
            int(vadjustment->get_value()+0.5)*layout.get_row_height();
    }
    
    layout.set_origin(-x_scroll_value, -y_scroll_value);
    window->invalidate(true);
}

void PatternView::update_adjustments() {
    if (!pattern)
        return;
    
    Gtk::Allocation allocation = get_allocation();
    
    if (hadjustment) {
        int channel_width = layout.get_channel_width()+layout.get_channel_margin();
        if (!channel_width)
            return;
        double page_size = allocation.get_width() / channel_width;
        
        hadjustment->configure(0, // value
                               0, // lower
                               pattern->get_channel_count(), // upper
                               1, // step increment
                               4, // page increment
                               page_size); // page size
    }
    
    if (vadjustment) {
        if (!layout.get_row_height())
            return;
        double page_size = allocation.get_height() / layout.get_row_height();
        
        vadjustment->configure(0, // value
                               0, // lower
                               pattern->get_length(), // upper
                               1, // step increment
                               8,  // page increment
                               page_size); // page size
    }
}

void PatternView::on_size_allocate(Gtk::Allocation& allocation) {
    set_allocation(allocation);
    
    if (window) {
        window->move_resize(allocation.get_x(), allocation.get_y(),
            allocation.get_width(), allocation.get_height());
    }
    
    update_adjustments();
}

void PatternView::draw_text(int x, int y, const char *text) {
    const char *s = text;
    int w,h;
    layout.get_text_size(w,h);
    gc->set_function(Gdk::AND);
    while (*s) {
        if ((*s) >= CharBegin && (*s) < CharEnd) {
            // PIXBUF_ALPHA_BILEVEL
            /*
            chars[(*s)-CharBegin]->render_to_drawable_alpha(
                window, 0, 0, x, y, w, h,
                Gdk::PIXBUF_ALPHA_FULL, 0, Gdk::RGB_DITHER_NONE, 0, 0);
            */
            /*
            chars[(*s)-CharBegin]->render_to_drawable(
                window, gc, 0, 0, x, y, w, h,
                Gdk::RGB_DITHER_NONE, 0, 0);
            */
            window->draw_drawable(gc, chars[(*s)-CharBegin], 0, 0,
                x, y, w, h);
        }
        x += w;
        s++;
    }
    gc->set_function(Gdk::COPY);
}

bool PatternView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    // clear screen
    gc->set_foreground(colors[ColorBackground]);
    window->draw_rectangle(gc, true, 0, 0, width, height);
    
    // build temporary row
    Pattern::Row row;
    // start iterating at start of pattern
    Pattern::iterator iter = pattern->begin();    
    
    int frame_count = pattern->get_length();
    int channel_count = pattern->get_channel_count();
    
    PatternCursor render_cursor;
    render_cursor.set_layout(layout);
    render_cursor.origin();
    
    int start_frame = 0;
    int end_frame = frame_count;
    
    int start_channel = 0;
    int end_channel = channel_count;
    
    Gdk::Rectangle area(&event->area);
    
    int y0 = area.get_y();
    int y1 = y0 + area.get_height();
    int x0 = area.get_x();
    int x1 = x0 + area.get_width();
    int param, item;
    layout.get_cell_location(x0, y0, start_frame, start_channel, param, item);
    layout.get_cell_location(x1, y1, end_frame, end_channel, param, item);
    end_frame++;
    if (end_frame > frame_count)
        end_frame = frame_count;
    end_channel++;
    if (end_channel > channel_count)
        end_channel = channel_count;
    
    render_cursor.set_row(start_frame);
    
    bool focus = has_focus();
    
    for (int frame = start_frame; frame < end_frame; ++frame) {
        // collect events from pattern
        pattern->collect_events(frame, iter, row);
        
        render_cursor.set_channel(start_channel);
        
        // now render all channels
        for (int channel = start_channel; channel < end_channel; ++channel) {
            // and all params in one channel
            for (int param = 0; param < layout.get_cell_count(); ++param) {
                CellRenderer *renderer = layout.get_cell_renderer(param);
                if (renderer) {
                    bool draw_cursor = false;
                    if (focus && cursor.is_at(render_cursor)) {
                        render_cursor.set_item(cursor.get_item());
                        draw_cursor = true;
                    }
                    bool selected = selection.in_range(render_cursor);
                    renderer->render_cell(*this,
                        render_cursor, row.get_event(channel, param), draw_cursor,
                        selected);
                }
                render_cursor.next_param();
            }
            render_cursor.next_channel();
        }
        render_cursor.next_row();
    }
    
    return true;
}

void PatternView::invalidate_cursor() {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    int x,y;
    cursor.get_pos(x,y);
    
    Gdk::Rectangle rect(0,y,width,layout.get_row_height());
    
    window->invalidate_rect(rect, true);
}

void PatternView::set_cursor(const PatternCursor &new_cursor) {
    invalidate_cursor();
    cursor = new_cursor;
    // sanity checks
    if (cursor.get_channel() >= pattern->get_channel_count()) {
        cursor.set_channel(pattern->get_channel_count()-1);
        cursor.set_last_item();
    }
    else if (cursor.get_param() >= layout.get_cell_count()) {
        cursor.set_last_item();
    }
    invalidate_cursor();
}

bool PatternView::on_motion_notify_event(GdkEventMotion *event) {
    return true;
}

bool PatternView::on_button_press_event(GdkEventButton* event) {
    grab_focus();
    PatternCursor new_cursor(cursor);
    new_cursor.set_pos(event->x, event->y);
    set_cursor(new_cursor);
    return false;
}

bool PatternView::on_button_release_event(GdkEventButton* event) {
    return false;
}

bool PatternView::on_key_press_event(GdkEventKey* event) {
    return false;
}

bool PatternView::on_key_release_event(GdkEventKey* event) {
    return false;
}


//=============================================================================

} // namespace Jacker
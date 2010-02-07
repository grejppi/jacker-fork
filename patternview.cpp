#include "patternview.hpp"
#include "model.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

/*
TODO:

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

// scancodes for both octaves of the piano, starting at C-0
static const guint16 piano_scancodes[] = {
#if defined(WIN32)
//  C     C#    D     D#    E     F     F#    G     G#    A     A#    B
    0x59, 0x53, 0x58, 0x44, 0x43, 0x56, 0x47, 0x42, 0x48, 0x4e, 0x4a, 0x4d, // -0
    0x51, 0x32, 0x57, 0x33, 0x45, 0x52, 0x35, 0x54, 0x36, 0x5a, 0x37, 0x55, // -1
    0x49, 0x39, 0x4f, 0x30, 0x50,                                           // -2
#else // linux
//  C     C#    D     D#    E     F     F#    G     G#    A     A#    B
    0x34, 0x27, 0x35, 0x28, 0x36, 0x37, 0x2a, 0x38, 0x2b, 0x39, 0x2c, 0x3a, // -0
    0x18, 0x0b, 0x19, 0x0c, 0x1a, 0x1b, 0x0e, 0x1c, 0x0f, 0x1d, 0x10, 0x1e, // -1
    0x1f, 0x12, 0x20, 0x13, 0x21,                                           // -2
#endif
};

//=============================================================================

CellRenderer::CellRenderer() {
    view = NULL;
}

void CellRenderer::set_view(PatternView &view) {
    this->view = &view;
}

PatternView *CellRenderer::get_view() const {
    return view;
}

void CellRenderer::render_background(PatternCursor &cursor,
                                     bool selected) {
    int row = cursor.get_row();
    int frames_per_bar = view->model->frames_per_beat
                          * view->model->beats_per_bar;
    
    int color_base = 0;
    if (selected) {
        color_base = ColorSelBackground - ColorBackground;
    }
    if (!(row % frames_per_bar)) {
        view->gc->set_foreground(view->colors[color_base+ColorRowBar]);
    } else if (!(row % view->model->frames_per_beat)) {
        view->gc->set_foreground(view->colors[color_base+ColorRowBeat]);
    } else if (selected) {
        view->gc->set_foreground(view->colors[color_base+ColorBackground]);
    } else {
        return; // nothing to draw
    }
    
    int x, y, w, h;
    cursor.get_pos(x,y);
    cursor.get_cell_size(w,h);
    if (!cursor.is_last_param()) {
        w += view->get_cell_margin();
    }
    view->window->draw_rectangle(view->gc, true, x, y, w, h);
}

void CellRenderer::render_cell(PatternCursor &cursor, Pattern::Event *event, 
                               bool draw_cursor, bool selected) {

}

int CellRenderer::get_width() {
    return 0;
}

int CellRenderer::get_item(int x) {
    return 0;
}

int CellRenderer::get_item_count() {
    return 0;
}

bool CellRenderer::on_key_press_event(GdkEventKey* event_key, 
                                      Pattern::Event &event, int item) {
    if (event_key->keyval == GDK_period) {
        event.value = ValueNone;
        view->navigate(0,1);
        return true;
    }
    return false;
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
    } else if (value == NoteOff) {
        sprintf(buffer, "===");
    } else {
        int note = value % 12;
        int octave = value / 12;
        
        assert((size_t)note < (sizeof(note_strings)/sizeof(const char *)));
        sprintf(buffer, "%s%i", note_strings[note], octave);
    }
    return 3;
}

void CellRendererNote::render_cell(PatternCursor &cursor, Pattern::Event *event, 
                                   bool draw_cursor, bool selected) {
                                       
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_note(text, value);
    
    render_background(cursor, selected);
    
    view->gc->set_foreground(view->colors[ColorBlack]);
    
    int x, y;
    cursor.get_pos(x,y);
    view->draw_text(x, y, text);
    
    if (draw_cursor) {
        int w,h;
        view->get_font_size(w,h);
        int item = cursor.get_item();
        if (item == ItemNote)
            w *= 3;
        else if (item == ItemOctave)
            x += 2 * w;
        view->window->draw_rectangle(view->xor_gc, true, x, y, w, h);
    }
}

int CellRendererNote::get_width() {
    int w,h;
    view->get_font_size(w,h);
    return 3 * w;
}

int CellRendererNote::get_item(int x) {
    int w,h;
    view->get_font_size(w,h);
    int pos = x / w;
    if (pos < 2)
        return ItemNote;
    else
        return ItemOctave;
}

int CellRendererNote::get_item_count() {
    return ItemCount;
}

bool CellRendererNote::on_key_press_event(GdkEventKey* event_key, 
     Pattern::Event &event, int item) {
    if (item == ItemNote) {
        if (event_key->keyval == GDK_1) {
            event.value = NoteOff;
            view->navigate(0,1);
            return true;
        }
        size_t count = sizeof(piano_scancodes) / sizeof(guint16);
        for (size_t i = 0; i < count; ++i) {
            if (event_key->hardware_keycode == piano_scancodes[i]) {
                event.value = 12*view->get_octave() + i;
                view->navigate(0,1);
                return true;
            }
        }
        
    } else if ((item == ItemOctave) && event.is_valid()) {
        int note = event.value % 12;
        if ((event_key->keyval >= GDK_0) && (event_key->keyval <= GDK_9)) {
            int octave = event_key->keyval - GDK_0;
            event.value = 12*octave + note;
            view->navigate(0,1);
            return true;
        }
    }
    return CellRenderer::on_key_press_event(event_key, event, item);
}

//=============================================================================

static int sprint_hex(char *buffer, int value, int length) {
    if (value == ValueNone) {
        char *s = buffer;
        for (int i = 0; i < length; ++i)
            *s++ = '.';
        *s = '\0';
    } else {
        char format[8];
        sprintf(format, "%%0%iX", length);
        sprintf(buffer, format, value);
    }
    return 2;
}

CellRendererHex::CellRendererHex(int value_length) {
    this->value_length = value_length;
}

void CellRendererHex::render_cell(PatternCursor &cursor, Pattern::Event *event, 
                                  bool draw_cursor, bool selected) {
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_hex(text, value, value_length);
    
    render_background(cursor, selected);
    
    view->gc->set_foreground(view->colors[ColorBlack]);
    
    int x, y;
    cursor.get_pos(x,y);
    view->draw_text(x, y, text);

    if (draw_cursor) {
        int w,h;
        view->get_font_size(w,h);
        int item = cursor.get_item();
        x += w * item;
        view->window->draw_rectangle(view->xor_gc, true, x, y, w, h);
    }
}

int CellRendererHex::get_width() {
    int w,h;
    view->get_font_size(w,h);
    return value_length * w;
}

int CellRendererHex::get_item(int x) {
    int w,h;
    view->get_font_size(w,h);
    int pos = x / w;
    return std::min(pos, value_length-1);
}

int CellRendererHex::get_item_count() {
    return value_length;
}

bool CellRendererHex::on_key_press_event(GdkEventKey* event_key, 
                                         Pattern::Event &event, int item) {
    int bit = (value_length-1-item)*4;
    int value = ValueNone;
    if ((event_key->keyval >= GDK_0) && (event_key->keyval <= GDK_9)) {
        value = event_key->keyval - GDK_0;
    }
    if ((event_key->keyval >= GDK_a) && (event_key->keyval <= GDK_f)) {
        value = 0xA + (event_key->keyval - GDK_a);
    }
    if (value != ValueNone) {
        if (event.is_valid())
            event.value ^= event.value & (0xF<<bit); // mask out
        else
            event.value = 0;
        event.value |= value<<bit;
        if (item < (value_length-1))
            view->navigate(1,0);
        else {
            view->navigate(-item,1);
        }
        return true;
    }
    return CellRenderer::on_key_press_event(event_key, event, item);
}

//=============================================================================

static int sprint_command(char *buffer, int value) {
    if (value == ValueNone) {
        sprintf(buffer, ".");
    } else {
        sprintf(buffer, "%c", (char)value);
    }
    return 1;
}

CellRendererCommand::CellRendererCommand() {
}

void CellRendererCommand::render_cell(PatternCursor &cursor, Pattern::Event *event, 
                                  bool draw_cursor, bool selected) {
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_command(text, value);
    
    render_background(cursor, selected);
    
    view->gc->set_foreground(view->colors[ColorBlack]);
    
    int x, y;
    cursor.get_pos(x,y);
    view->draw_text(x, y, text);

    if (draw_cursor) {
        int w,h;
        view->get_font_size(w,h);
        int item = cursor.get_item();
        x += w * item;
        view->window->draw_rectangle(view->xor_gc, true, x, y, w, h);
    }
}

int CellRendererCommand::get_width() {
    int w,h;
    view->get_font_size(w,h);
    return w;
}

int CellRendererCommand::get_item(int x) {
    return 0;
}

int CellRendererCommand::get_item_count() {
    return 1;
}

bool CellRendererCommand::on_key_press_event(GdkEventKey* event_key, 
                                         Pattern::Event &event, int item) {
    int value = ValueNone;
    if ((event_key->keyval >= GDK_0) && (event_key->keyval <= GDK_9)) {
        value = '0' + (event_key->keyval - GDK_0);
    }
    if ((event_key->keyval >= GDK_a) && (event_key->keyval <= GDK_z)) {
        value = 'A' + (event_key->keyval - GDK_a);
    }
    if (value != ValueNone) {
        event.value = value;
        view->navigate(0,1);
        return true;
    }
    return CellRenderer::on_key_press_event(event_key, event, item);
}

//=============================================================================

PatternCursor::PatternCursor() {
    row = 0;
    channel = 0;
    param = 0;
    item = 0;
    view = NULL;
}

void PatternCursor::set_view(PatternView &view) {
    this->view = &view;
}

PatternView *PatternCursor::get_view() const {
    assert(view);
    return view;
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

void PatternCursor::prev_channel() {
    if (!param && !item)
        channel = std::max(channel-1,0);
    param = 0;
    item = 0;
}

void PatternCursor::next_param() {
    param++;
    item = 0;
}

void PatternCursor::home() {
    if (param || item) {
        param = 0;
        item = 0;
    } else if (channel) {
        channel = 0;
    } else if (row) {
        row = 0;
    }
}

void PatternCursor::end() {
    if (!is_last_param() || !is_last_item()) {
        set_last_param();
        set_last_item();
    } else if (!is_last_channel()) {
        set_last_channel();
    } else if (!is_last_row()) {
        set_last_row();
    }
}

bool PatternCursor::is_last_param() const {
    assert(view);
    return param == view->get_cell_count()-1;
}

void PatternCursor::set_last_param() {
    assert(view);
    item = 0;
    param = view->get_cell_count()-1;
}

bool PatternCursor::is_last_channel() const {
    assert(view);
    return channel == view->get_pattern()->get_channel_count()-1;
}

void PatternCursor::set_last_channel() {
    assert(view);
    channel = view->get_pattern()->get_channel_count()-1;
}

bool PatternCursor::is_last_row() const {
    assert(view);
    return row == view->get_pattern()->get_length()-1;
}

void PatternCursor::set_last_row() {
    assert(view);
    row = view->get_pattern()->get_length()-1;
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
    assert(view);
    return channel * view->get_cell_count() + param;
}

void PatternCursor::get_pos(int &x, int &y) const {
    assert(view);
    view->get_cell_pos(row, channel, param, x, y);
}

void PatternCursor::set_pos(int x, int y) {
    assert(view);
    view->get_cell_location(x, y, row, channel, param, item);
}

void PatternCursor::get_cell_size(int &w, int &h, bool include_margin) const {
    assert(view);
    view->get_cell_size(param, w, h, include_margin);
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
    item = 0;
    CellRenderer *renderer = view->get_cell_renderer(param);
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

void PatternCursor::navigate_row(int delta) {
    row += delta;
}

bool PatternCursor::is_last_item() const {
    assert(view);
    CellRenderer *renderer = view->get_cell_renderer(param);
    if (!renderer)
        return true;
    return (item >= (renderer->get_item_count()-1));
}

void PatternCursor::navigate_column(int delta) {
    assert(view);
    if (delta > 0) {
        while (delta) {
            if (is_last_item()) {
                if (is_last_param()) {
                    channel++;
                    param = 0;
                } else {
                    param++;
                }
                item = 0;
            } else {
                item++;
            }
            delta--;
        }
    } else if (delta < 0) {
        while (delta) {
            if (!item) {
                if (!param) {
                    if (!channel) {
                        return; // can't go more left
                    } else {
                        channel--;
                    }
                    param = view->get_cell_count()-1;
                } else {
                    param--;
                }
                CellRenderer *renderer = view->get_cell_renderer(param);
                if (renderer)
                    item = renderer->get_item_count()-1;
                else
                    item = 0;
            } else {
                item--;
            }
            delta++;
        }
    }
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
    if (r1 < r0) {
        std::swap(r0,r1);
    }
    return (value >= r0) && (value <= r1);
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

void PatternSelection::set_view(PatternView &view) {
    p0.set_view(view);
    p1.set_view(view);
}

void PatternSelection::sort() {
    if (p1.row < p0.row)
        std::swap(p0.row, p1.row);
    if (p1.channel < p0.channel) {
        std::swap(p0.channel, p1.channel);
        std::swap(p0.param, p1.param);
        std::swap(p0.item, p1.item);
    } else if (p0.channel == p1.channel) {
        if (p1.param < p0.param) {
            std::swap(p0.param, p1.param);
        } else if (p1.param == p0.param) {
            if (p1.item < p0.item) {
                std::swap(p0.item, p1.item);
            }
        }
    }
}

bool PatternSelection::get_rect(int &x, int &y, int &width, int &height) const {
    if (!get_active())
        return false;
    
    PatternSelection sel(*this);
    sel.sort();
    
    int x0, y0;
    sel.p0.get_pos(x0, y0);
    int x1, y1;
    sel.p1.get_pos(x1, y1);
    int cw,ch;
    sel.p1.get_cell_size(cw,ch,true);
    x1 += cw;
    y1 += ch;
    x = x0;
    y = y0;
    width = x1 - x0;
    height = y1 - y0;
    return true;
}

//=============================================================================

PatternView::PatternView(BaseObjectType* cobject,
                         const Glib::RefPtr<Gtk::Builder>& builder)
  : Gtk::Widget(cobject), 
    byte_renderer(2) {
    model = NULL;
    pattern = NULL;
    hadjustment = 0;
    vadjustment = 0;
    interact_mode = InteractNone;
    row_height = 0;
    origin_x = origin_y = 0;
    cell_margin = 0;
    channel_margin = 0;
    font_width = 0;
    font_height = 0;
    octave = 4;
    play_position = -1;
    renderers.resize(ParamCount);
    for (size_t i = 0; i < renderers.size(); ++i) {
        renderers[i] = NULL;
    }
      
    colors.resize(ColorCount);
    colors[ColorBlack].set("#000000");
    colors[ColorWhite].set("#FFFFFF");
    colors[ColorBackground].set("#e0e0e0");
    colors[ColorRowBar].set("#c0c0c0");
    colors[ColorRowBeat].set("#d0d0d0");
    colors[ColorSelBackground].set("#00a0ff");
    colors[ColorSelRowBar].set("#40e0ff");
    colors[ColorSelRowBeat].set("#20c0ff");
}

void PatternView::invalidate_play_position() {
    if (!window)
        return;
    if (play_position < 0)
        return;
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    int play_x, play_y;
    get_cell_pos(play_position, 0, 0, play_x, play_y);
    Gdk::Rectangle rect(0, play_y, width, row_height);
    window->invalidate_rect(rect, true);
}

void PatternView::set_play_position(int pos) {
    if (play_position == pos)
        return;
    invalidate_play_position();
    play_position = pos;
    invalidate_play_position();
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

Pattern *PatternView::get_pattern() const {
    return this->pattern;
}

void PatternView::set_model(class Model &model) {
    this->model = &model;
}

void PatternView::set_pattern(class Pattern *pattern) {
    this->pattern = pattern;
    invalidate();
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
    pango_layout->get_pixel_size(font_width, font_height);
    
    for (int i = CharBegin; i < CharEnd; ++i) {
        Glib::RefPtr<Gdk::Pixmap> pixmap = Gdk::Pixmap::create(
            window, font_width, font_height);
        
        char buffer[4];
        sprintf(buffer, "%c", (char)i);
        pango_layout->set_text(buffer);
        
        Glib::RefPtr<Gdk::GC> pm_gc = Gdk::GC::create(pixmap);
        pm_gc->set_colormap(cm);
        pm_gc->set_foreground(colors[ColorWhite]);
        pixmap->draw_rectangle(pm_gc, true, 0, 0, font_width, font_height);
        
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
    set_origin(0,0);
    set_cell_margin(5);
    set_channel_margin(10);
    set_row_height(font_height);
    
    set_cell_renderer(ParamNote, &note_renderer);
    set_cell_renderer(ParamVolume, &byte_renderer);
    set_cell_renderer(ParamCommand, &command_renderer);
    set_cell_renderer(ParamValue, &byte_renderer);
    set_cell_renderer(ParamCCIndex, &byte_renderer);
    set_cell_renderer(ParamCCValue, &byte_renderer);
    
    cursor.set_view(*this);
    selection.set_view(*this);
 
    update_adjustments();
}

void PatternView::on_adjustment_value_changed() {
    int origin_x, origin_y;
    get_origin(origin_x, origin_y);
    
    int x_scroll_value = origin_x;
    int y_scroll_value = origin_y;
    
    if (hadjustment) {
        int channel_width = get_channel_width()+get_channel_margin();
        x_scroll_value =
            int(hadjustment->get_value()+0.5)*channel_width;
    }
    if (vadjustment) {
        y_scroll_value = 
            int(vadjustment->get_value()+0.5)*get_row_height();
    }
    
    set_origin(-x_scroll_value, -y_scroll_value);
    invalidate();
}

void PatternView::update_adjustments() {
    if (!pattern)
        return;
    
    Gtk::Allocation allocation = get_allocation();
    
    if (hadjustment) {
        int channel_width = get_channel_width()+get_channel_margin();
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
        if (!get_row_height())
            return;
        double page_size = allocation.get_height() / get_row_height();
        
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
    get_font_size(w,h);
    gc->set_function(Gdk::AND);
    while (*s) {
        if ((*s) >= CharBegin && (*s) < CharEnd) {
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
    
    if (!pattern)
        return true;
    
    // build temporary row
    Pattern::Row row;
    // start iterating at start of pattern
    Pattern::iterator iter = pattern->begin();    
    
    int frame_count = pattern->get_length();
    int channel_count = pattern->get_channel_count();
    
    PatternCursor render_cursor;
    render_cursor.set_view(*this);
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
    get_cell_location(x0, y0, start_frame, start_channel, param, item);
    get_cell_location(x1, y1, end_frame, end_channel, param, item);
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
            for (int param = 0; param < get_cell_count(); ++param) {
                CellRenderer *renderer = get_cell_renderer(param);
                if (renderer) {
                    bool draw_cursor = false;
                    if (focus && cursor.is_at(render_cursor)) {
                        render_cursor.set_item(cursor.get_item());
                        draw_cursor = true;
                    }
                    bool selected = selection.in_range(render_cursor);
                    renderer->render_cell(render_cursor, 
                        row.get_event(channel, param), draw_cursor, selected);
                }
                render_cursor.next_param();
            }
            render_cursor.next_channel();
        }
        render_cursor.next_row();
    }
    
    if (play_position >= 0) {
        int play_x, play_y;
        get_cell_pos(play_position, 0, 0, play_x, play_y);
        window->draw_rectangle(xor_gc, true, 0, play_y, width, row_height);
    }
    return true;
}

void PatternView::invalidate_selection() {
    int x,y,w,h;
    if (!selection.get_rect(x,y,w,h))
        return;
    Gdk::Rectangle rect(x,y,w,h);
    window->invalidate_rect(rect, true);
}

void PatternView::invalidate_cursor() {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    int x,y;
    cursor.get_pos(x,y);
    
    Gdk::Rectangle rect(0,y,width,get_row_height());
    
    window->invalidate_rect(rect, true);
}

void PatternView::clip_cursor(PatternCursor &c) {
    if (!pattern)
        return;
    // sanity checks/fixes
    if (c.get_row() < 0) {
        c.set_row(0);
    } else if (c.get_row() >= pattern->get_length()) {
        c.set_row(pattern->get_length()-1);
    }
    if (c.get_channel() < 0) {
        c.set_channel(0);
    } else if (c.get_channel() >= pattern->get_channel_count()) {
        c.set_channel(pattern->get_channel_count()-1);
        c.set_last_param();
        c.set_last_item();
    }
    else if (c.get_param() >= get_cell_count()) {
        c.set_last_param();
        c.set_last_item();
    }
}

void PatternView::set_cursor(int x, int y) {
    PatternCursor new_cursor(cursor);
    new_cursor.set_pos(x,y);
    set_cursor(new_cursor);
}

void PatternView::set_cursor(const PatternCursor &new_cursor) {
    invalidate_cursor();
    cursor = new_cursor;
    clip_cursor(cursor);
    invalidate_cursor();
    show_cursor();
}

void PatternView::show_cursor(const PatternCursor &cur, bool page_jump/*=false*/) {
    if (hadjustment) {
        int channel = cur.get_channel();
        hadjustment->clamp_page(channel,channel+1);
    }
    if (vadjustment) {
        int row = cur.get_row();
        if (page_jump) {
            int fpb = get_frames_per_bar();
            int value = vadjustment->get_value();
            int page_size = vadjustment->get_page_size();
            if (row < value)
                vadjustment->clamp_page(row-fpb,row);
            else if (row >= (value+page_size))
                vadjustment->clamp_page(row,row+fpb);
        } else {
            vadjustment->clamp_page(row,row+1);
        }
    }
}

void PatternView::show_cursor() {
    show_cursor(cursor, true);
}

bool PatternView::on_motion_notify_event(GdkEventMotion *event) {
    if (!pattern)
        return true;
    if (interact_mode == InteractSelect) {
        invalidate_selection();
        selection.set_active(true);
        selection.p1.set_pos(event->x, event->y);
        clip_cursor(selection.p1);
        invalidate_selection();
        show_cursor(selection.p1);
    }
    return true;
}

void PatternView::navigate(int delta_x, int delta_y) {
    PatternCursor new_cursor(cursor);
    if (delta_x) {
        new_cursor.navigate_column(delta_x);
    }
    if (delta_y) {
        new_cursor.navigate_row(delta_y);
    }
    set_cursor(new_cursor);
}

bool PatternView::on_button_press_event(GdkEventButton* event) {
    if (!pattern)
        return false;
    grab_focus();
    
    set_cursor(event->x, event->y);
    
    invalidate_selection();
    interact_mode = InteractSelect;
    selection.p0.set_pos(event->x, event->y);
    clip_cursor(selection.p0);
    selection.p1 = selection.p0;
    selection.set_active(false);
    return false;
}

bool PatternView::on_button_release_event(GdkEventButton* event) {
    if (!pattern)
        return false;
    interact_mode = InteractNone;
    return false;
}

bool PatternView::on_key_press_event(GdkEventKey* event) {
    bool shift_down = event->state & Gdk::SHIFT_MASK;
    /*
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    bool alt_down = event->state & Gdk::MOD1_MASK;
    bool super_down = event->state & (Gdk::SUPER_MASK|Gdk::MOD4_MASK);
    */
    if (!pattern)
        return true;
    
    PatternCursor new_cursor(cursor);
    switch (event->keyval) {
        case GDK_Left: navigate(-1,0); return true;
        case GDK_Right: navigate(1,0); return true;
        case GDK_Up: navigate(0,-1); return true;
        case GDK_Down: navigate(0,1); return true;
        case GDK_Page_Up: navigate(0,-get_frames_per_bar()); return true;
        case GDK_Page_Down: navigate(0,get_frames_per_bar()); return true;
        case GDK_Home: new_cursor.home(); set_cursor(new_cursor); return true;
        case GDK_End: new_cursor.end(); set_cursor(new_cursor); return true;
        case GDK_ISO_Left_Tab:
        case GDK_Tab: {
            if (shift_down)
                new_cursor.prev_channel();
            else
                new_cursor.next_channel();
            set_cursor(new_cursor);
            return true;
        } break;
        default: {
            CellRenderer *renderer = get_cell_renderer(cursor.get_param());
            if (renderer) {
                Pattern::Event evt;
                evt.frame = cursor.get_row();
                evt.channel = cursor.get_channel();
                evt.param = cursor.get_param();
                Pattern::iterator i = pattern->get_event(cursor.get_row(),
                    cursor.get_channel(), cursor.get_param());
                if (i != pattern->end())
                    evt = i->second;
                if (renderer->on_key_press_event(event, evt, 
                                                 cursor.get_item())) {
                    evt.sanitize_value();
                    if (evt.is_valid())
                        pattern->add_event(evt);
                    else if (i != pattern->end())
                        pattern->erase(i);
                    invalidate_cursor();
                    return true;
                }
            }            
        } break;
    }
    fprintf(stderr, "No handler for %s\n", gdk_keyval_name(event->keyval));
    return true;
}

bool PatternView::on_key_release_event(GdkEventKey* event) {
    return false;
}

void PatternView::set_cell_renderer(int param, CellRenderer *renderer) {
    renderers[param] = renderer;
    renderer->set_view(*this);
}

CellRenderer *PatternView::get_cell_renderer(
    int param) const {
    return renderers[param];
}

int PatternView::get_cell_count() const {
    return renderers.size();
}

void PatternView::set_origin(int x, int y) {
    origin_x = x;
    origin_y = y;
}

void PatternView::get_origin(int &x, int &y) {
    x = origin_x;
    y = origin_y;
}

void PatternView::set_cell_margin(int margin) {
    cell_margin = margin;
}

void PatternView::set_channel_margin(int margin) {
    channel_margin = margin;
}

int PatternView::get_cell_margin() const {
    return cell_margin;
}

int PatternView::get_channel_margin() const {
    return channel_margin;
}

void PatternView::set_row_height(int height) {
    row_height = height;
}

int PatternView::get_row_height() const {
    return row_height;
}

void PatternView::get_cell_size(int param, int &w, int &h, 
                                  bool include_margin) const {
    w = 0;
    h = row_height;
    
    CellRenderer *renderer = renderers[param];
    if (renderer)
        w = renderer->get_width();
    if (include_margin && ((size_t)param < (renderers.size()-1))) {
        w += get_cell_margin();
    }
}

int PatternView::get_param_offset(int param) const {
    int x = 0;
    for (size_t i = 0; i < (size_t)param; ++i) {
        CellRenderer *renderer = renderers[i];
        if (renderer)
            x += renderer->get_width();
        if (i < (renderers.size()-1))
            x += cell_margin;
    }
    return x;
}

int PatternView::get_channel_width() const {
    return get_param_offset(renderers.size());
}

void PatternView::get_cell_pos(int row, int channel, int param,
                                       int &x, int &y) const {
    y = origin_y + row * row_height;
    x = origin_x + channel * (get_channel_width() + channel_margin) +
        get_param_offset(param);
    
}

bool PatternView::get_cell_location(int x, int y, int &row, int &channel,
    int &param, int &item) const {
    int channel_width = get_channel_width();
    assert(row_height);
    assert(channel_width);
    y -= origin_y;
    row = y / row_height;
    x -= origin_x;
    channel = x / (channel_width + channel_margin);
    x -= channel * (channel_width + channel_margin);
    param = renderers.size()-1;
    item = ((renderers[param])?(renderers[param]->get_item_count()-1):0);
    for (size_t i = 0; i < renderers.size(); ++i) {
        CellRenderer *renderer = renderers[i];
        if (renderer) {
            int width = renderer->get_width();
            if (x < width) {
                param = i;
                item = renderer->get_item(x);
                break;
            }
            x -= width;
        }
        if (i < (renderers.size()-1))
            x -= cell_margin;
    }
    return false;
}

void PatternView::set_font_size(int width, int height) {
    font_width = width;
    font_height = height;
}

void PatternView::get_font_size(int &width, int &height) const {
    width = font_width;
    height = font_height;
}

int PatternView::get_frames_per_bar() const {
    return model->frames_per_beat * model->beats_per_bar;
}

int PatternView::get_octave() const {
    return this->octave;
}

void PatternView::set_octave(int octave) {
    if (octave == this->octave)
        return;
    this->octave = std::min(std::max(octave,0),9);
}

void PatternView::invalidate() {
    if (!window)
        return;
    window->invalidate(true);
}

//=============================================================================

} // namespace Jacker
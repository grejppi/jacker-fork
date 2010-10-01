#include "patternview.hpp"
#include "model.hpp"
#include "jsong.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

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
//  C     C#    D     D#    E     F     F#    G     G#    A     A#    B      Octave
    0x59, 0x53, 0x58, 0x44, 0x43, 0x56, 0x47, 0x42, 0x48, 0x4e, 0x4a, 0x4d, // +0
    0x51, 0x32, 0x57, 0x33, 0x45, 0x52, 0x35, 0x54, 0x36, 0x5a, 0x37, 0x55, // +1
    0x49, 0x39, 0x4f, 0x30, 0x50,                                           // +2
#else // linux
//  C     C#    D     D#    E     F     F#    G     G#    A     A#    B      Octave
    0x34, 0x27, 0x35, 0x28, 0x36, 0x37, 0x2a, 0x38, 0x2b, 0x39, 0x2c, 0x3a, // +0
    0x18, 0x0b, 0x19, 0x0c, 0x1a, 0x1b, 0x0e, 0x1c, 0x0f, 0x1d, 0x10, 0x1e, // +1
    0x1f, 0x12, 0x20, 0x13, 0x21,                                           // +2
#endif
};

enum {
#if defined(WIN32)
    ScanOctaveDown = 0xdb,
    ScanOctaveUp = 0xdd,
#else
    ScanOctaveDown = 0x14,
    ScanOctaveUp = 0x15,
#endif
};

static const char TargetFormatPattern[] = "jacker_pattern_block";

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
            view->play_event(event);
            view->navigate(0,1);
            return true;
        }
        size_t count = sizeof(piano_scancodes) / sizeof(guint16);
        for (size_t i = 0; i < count; ++i) {
            if (event_key->hardware_keycode == piano_scancodes[i]) {
                event.value = 12*view->get_octave() + i;
                view->play_event(event);
                view->navigate(0,1);
                return true;
            }
        }
        
    } else if ((item == ItemOctave) && event.is_valid()) {
        int note = event.value % 12;
        if ((event_key->keyval >= GDK_0) && (event_key->keyval <= GDK_9)) {
            int octave = event_key->keyval - GDK_0;
            event.value = 12*octave + note;
            view->set_octave(octave);
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
        /*
        if (item < (value_length-1))
            view->navigate(1,0);
        else {
            view->navigate(-item,1);
        }
        */
        view->navigate(0,1);
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

std::string PatternCursor::debug_string() const {
    char text[64];
    sprintf(text, "(#%i[%i]%i:%i)", row, channel, param, item);
    return text;
}

const PatternCursor &PatternCursor::operator =(const Pattern::Event &event) {
    row = event.frame;
    channel = event.channel;
    param = event.param;
    item = 0;
    return *this;
}

//=============================================================================

PatternSelection::PatternSelection() {
    active = false;
    view = NULL;
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
    this->view = &view;
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

std::string PatternSelection::debug_string() const {
    char text[128];
    sprintf(text, "{%s-%s}", p0.debug_string().c_str(), p1.debug_string().c_str());
    return text;
}

//=============================================================================

PatternView::PatternView(BaseObjectType* cobject,
                         const Glib::RefPtr<Gtk::Builder>& builder)
  : Gtk::Widget(cobject), 
    byte_renderer(2) {
    model = NULL;
    select_at_cursor = false;
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
    cursor_x = 0;
    cursor_y = 0;
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
    if (song_event == model->song.end())
        return NULL;
    return song_event->second.pattern;
}

void PatternView::set_model(class Model &model) {
    this->model = &model;
    this->song_event = this->model->song.end();
}

void PatternView::set_song_event(Song::iterator event) {
    song_event = event;
    selection.set_active(false);
    invalidate();
    update_adjustments();
}

Song::iterator PatternView::get_song_event() {
    return song_event;
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
    
    int x_scroll_value = -origin_x;
    int y_scroll_value = -origin_y;
    
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
    if (!get_pattern())
        return;
    
    Gtk::Allocation allocation = get_allocation();
    
    if (hadjustment) {
        int channel_width = get_channel_width()+get_channel_margin();
        if (!channel_width)
            return;
        double page_size = allocation.get_width() / channel_width;
        
        hadjustment->configure(0, // value
                               0, // lower
                               get_pattern()->get_channel_count(), // upper
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
                               get_pattern()->get_length(), // upper
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
    
    if (!get_pattern())
        return true;
    
    // build temporary row
    Pattern::Row row;
    // start iterating at start of pattern
    Pattern::iterator iter = get_pattern()->begin();    
    
    int frame_count = get_pattern()->get_length();
    int channel_count = get_pattern()->get_channel_count();
    
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
        get_pattern()->collect_events(frame, iter, row);
        
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

void PatternView::invalidate_range(const PatternSelection &range) {
    int x,y,w,h;
    if (!range.get_rect(x,y,w,h))
        return;
    Gdk::Rectangle rect(x,y,w,h);
    window->invalidate_rect(rect, true);
}

void PatternView::invalidate_selection() {
    invalidate_range(selection);
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
    if (!get_pattern())
        return;
    // sanity checks/fixes
    if (c.get_row() < 0) {
        c.set_row(0);
    } else if (c.get_row() >= get_pattern()->get_length()) {
        c.set_row(get_pattern()->get_length()-1);
    }
    if (c.get_channel() < 0) {
        c.set_channel(0);
    } else if (c.get_channel() >= get_pattern()->get_channel_count()) {
        c.set_channel(get_pattern()->get_channel_count()-1);
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

void PatternView::set_cursor(const PatternCursor &new_cursor, bool select/*=false*/) {
    invalidate_cursor();
    if (select) {
        if (!selection.get_active() || !select_at_cursor) {
            invalidate_selection();
            selection.p0 = cursor;
            selection.p1 = selection.p0;
            selection.set_active(true);
            invalidate_selection();
        }
        select_at_cursor = true;
    } else {
        select_at_cursor = false;
    }
    cursor = new_cursor;
    clip_cursor(cursor);
    if (select) {
        invalidate_selection();
        selection.p1 = cursor;
        invalidate_selection();
    }
    invalidate_cursor();
    show_cursor();
    update_navigation_status();
}

void PatternView::reset() {
    song_event = model->song.end();
}

void PatternView::show_cursor(const PatternCursor &cur, bool page_jump/*=false*/) {
    if (hadjustment) {
        int channel = cur.get_channel();
        hadjustment->clamp_page(channel,channel+1);
    }
    if (vadjustment) {
        int row = cur.get_row();
        if (page_jump) {
            int fpb = get_page_scroll_size();
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
    if (!get_pattern())
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

void PatternView::navigate(int delta_x, int delta_y, bool select/*=false*/) {
    PatternCursor new_cursor(cursor);
    if (delta_x) {
        new_cursor.navigate_column(delta_x);
    }
    if (delta_y) {
        new_cursor.navigate_row(delta_y);
    }
    set_cursor(new_cursor, select);
}

bool PatternView::on_button_press_event(GdkEventButton* event) {
    if (!get_pattern())
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
    if (!get_pattern())
        return false;
    interact_mode = InteractNone;
    return false;
}

void PatternView::transpose(int step) {
    Pattern::iterator iter;
    for (Pattern::iterator iter = get_pattern()->begin(); 
         iter != get_pattern()->end(); ++iter) {
        PatternCursor cur(cursor);
        cur = iter->second;
        if (!selection.in_range(cur))
            continue;
        if (iter->second.param != ParamNote)
            continue;
        if (iter->second.value == NoteOff)
            continue;
        iter->second.value += step;
        iter->second.sanitize_value();
    }    
    
    invalidate_selection();
}

void PatternView::move_frames(int step, bool all_channels/*=false*/) {
    int row = cursor.get_row();
    int channel = cursor.get_channel();
    
    get_pattern()->move_frames(row, step, (all_channels?-1:channel));
    
    invalidate();
}

void PatternView::on_clipboard_get(Gtk::SelectionData &data, guint info) {
    const std::string target = data.get_target();
    if (target != TargetFormatPattern) {
        printf("can't provide target %s\n", target.c_str());
        return;
    }
    data.set(TargetFormatPattern, clipboard_jsong);
}

void PatternView::on_clipboard_clear() {
}

void PatternView::on_clipboard_received(const Gtk::SelectionData &data) {
    Pattern *pattern = get_pattern();
    if (!pattern)
        return;
    const std::string target = data.get_target();
    if (target != TargetFormatPattern) {
        printf("can't receive target %s\n", target.c_str());
        return;
    }
    std::string text = data.get_data_as_string();
    if (text.empty())
        return;
    
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(text, root)) {
        std::cout << "Error parsing JSong: " << reader.getFormatedErrorMessages();
        return;
    }
    
    JSongReader jsong_reader;
    Pattern block;
    jsong_reader.build(root, block);
    if (root.empty())
        return;

    int range_begin = cursor.get_row();
    int range_end = range_begin + block.get_length()-1;
    
    int channel_begin = cursor.get_channel();
    int channel_end = channel_begin + block.get_channel_count()-1;
    
    int param_begin = 0;
    int param_end = 0;
    JSongReader::extract(root["first_param"], param_begin);
    JSongReader::extract(root["last_param"], param_end);
    
    // paste selection
    PatternSelection paste_sel(selection);
    paste_sel.p0.set_row(range_begin);
    paste_sel.p0.set_channel(channel_begin);
    paste_sel.p0.set_param(param_begin);
    paste_sel.p1.set_row(range_end);
    paste_sel.p1.set_channel(channel_end);
    paste_sel.p1.set_param(param_end);
    paste_sel.set_active(true);
    
    // clear pattern at paste position
    Pattern::iterator iter;
    for (Pattern::iterator iter = pattern->begin(); 
         iter != pattern->end(); ++iter) {
        Pattern::Event &event = iter->second;
        PatternCursor cur(cursor);
        cur.set_row(event.frame);
        cur.set_channel(event.channel);
        cur.set_param(event.param);
        if (paste_sel.in_range(cur)) {
            event.frame = -1; // will be deleted
        }
    }
    
    pattern->update_keys();
    
    
    for (Pattern::iterator iter = block.begin(); iter != block.end();
         iter++) {
        Pattern::Event event = iter->second;
        event.frame += cursor.get_row();
        event.channel += cursor.get_channel();
        if (event.channel >= pattern->get_channel_count())
            continue; // skip
        if (event.frame >= pattern->get_length())
            continue; // skip
        pattern->add_event(event);
    }
    invalidate();
}

void PatternView::cut_block() {
    copy_block();
    clear_block();
}

void PatternView::copy_block() {
    if (!get_pattern())
        return;
    clipboard_jsong = "";
    Pattern block;
    block.copy_from(*get_pattern());
    // clip everything that's not in range
    for (Pattern::iterator iter = block.begin(); iter != block.end();
         iter++) {
        PatternCursor cur(cursor);
        cur.set_row(iter->second.frame);
        cur.set_channel(iter->second.channel);
        cur.set_param(iter->second.param);
        if (!selection.in_range(cur)) {
            iter->second.frame = -1; // clip
        } else {
            iter->second.frame -= selection.p0.get_row();
            iter->second.channel -= selection.p0.get_channel();
        }
    }
    block.update_keys(); // remove clipped keys
    int length = selection.p1.get_row() - selection.p0.get_row() + 1;
    block.set_length(length);
    int channel_count = selection.p1.get_channel() - 
                        selection.p0.get_channel() + 1;
    block.set_channel_count(channel_count);
    
    JSongWriter jsong_writer;
    Json::Value root;
    jsong_writer.collect(root,block);
    root["first_param"] = selection.p0.get_param();
    root["last_param"] = selection.p1.get_param();
    
    if (root.empty())
        return;
    Json::StyledWriter writer;
    clipboard_jsong = writer.write(root);
    
    Glib::RefPtr<Gtk::Clipboard> clipboard = Gtk::Clipboard::get();
    std::list<Gtk::TargetEntry> list_targets;
    list_targets.push_back(Gtk::TargetEntry(TargetFormatPattern));
    clipboard->set(list_targets,
        sigc::mem_fun(*this, &PatternView::on_clipboard_get),
        sigc::mem_fun(*this, &PatternView::on_clipboard_clear));
    clipboard->store();
}

void PatternView::paste_block() {
    Glib::RefPtr<Gtk::Clipboard> clipboard = Gtk::Clipboard::get();
    clipboard->request_contents(TargetFormatPattern,
        sigc::mem_fun(*this, &PatternView::on_clipboard_received));
}

void PatternView::clear_block() {
    Pattern::iterator iter;
    for (Pattern::iterator iter = get_pattern()->begin(); 
         iter != get_pattern()->end(); ++iter) {
        PatternCursor cur(cursor);
        cur = iter->second;
        if (!selection.in_range(cur))
            continue;
        iter->second.frame = -1; // will be deleted
    }
    
    get_pattern()->update_keys();
    
    invalidate_selection();
}

void PatternView::cycle_block_selection() {
    invalidate_selection();
    selection.sort();
    // single channel selected?
    if (selection.p1.get_channel() == selection.p0.get_channel()) {
        // whole channel selected?
        if ((selection.p0.get_param() == 0) && (selection.p1.is_last_param())) {
            // select all channels
            selection.p0.set_channel(0);
            selection.p1.set_last_channel();
        } else { // not whole channel selected, so select whole channel
            selection.p0.set_param(0);
            selection.p1.set_last_param();
        }
    // all channels selected?
    } else if ((selection.p0.get_channel() == 0) && selection.p1.is_last_channel()) {
        // select single param
        selection.p0.set_channel(cursor.get_channel());
        selection.p0.set_param(cursor.get_param());
        selection.p1.set_channel(cursor.get_channel());
        selection.p1.set_param(cursor.get_param());
    } else { // some channels selected
        // select all channels
        selection.p0.set_param(0);
        selection.p0.set_channel(0);
        selection.p1.set_last_channel();
        selection.p1.set_last_param();
    }
    invalidate_selection();
}

void PatternView::begin_block() {
    invalidate_selection();
    if (!selection.get_active()) {
        selection.p0 = selection.p1 = cursor;
        selection.set_active(true);
    } else if (cursor.get_row() == selection.p0.get_row()) {
        cycle_block_selection();
    } else {
        selection.p0 = cursor;
    }
    invalidate_selection();
}

void PatternView::end_block() {
    invalidate_selection();
    if (!selection.get_active()) {
        selection.p0 = selection.p1 = cursor;
        selection.set_active(true);
    } else if (cursor.get_row() == selection.p1.get_row()) {
        cycle_block_selection();
    } else {
        selection.p1 = cursor;
    }
    invalidate_selection();
}


bool PatternView::on_key_press_event(GdkEventKey* event) {
    bool shift_down = event->state & Gdk::SHIFT_MASK;
    bool alt_down = event->state & Gdk::MOD1_MASK;
    bool ctrl_down = event->state & Gdk::CONTROL_MASK;
    /*
    bool super_down = event->state & (Gdk::SUPER_MASK|Gdk::MOD4_MASK);
    */
    if (!get_pattern())
        return true;
    
    PatternCursor new_cursor(cursor);
    
    if (ctrl_down) {
        switch (event->keyval) {
            case GDK_b: begin_block(); return true;
            case GDK_e: end_block(); return true;
            case GDK_x: cut_block(); return true;
            case GDK_c: copy_block(); return true;
            case GDK_v: paste_block(); return true;
            case GDK_d: clear_block(); return true;
            case GDK_plus:
            case GDK_KP_Add:
            {
                get_pattern()->set_channel_count(
                    get_pattern()->get_channel_count()+1);
                invalidate();
                return true;
            } break;
            case GDK_minus:
            case GDK_KP_Subtract:
            {
                get_pattern()->set_channel_count(
                    get_pattern()->get_channel_count()-1);
                invalidate();
                return true;
            } break;
            default: break;
        }
        
    }
    else if (alt_down) {
        switch (event->keyval) {
            case GDK_Home: set_octave(get_octave()-1); return true;
            case GDK_End: set_octave(get_octave()+1); return true;
            case GDK_Insert: move_frames(1,true); return true;
            case GDK_Delete: move_frames(-1,true); return true;
            default: break;
        }
    }
    else {
        switch (event->keyval) {
            case GDK_F6: play_pattern(); return true;
            case GDK_F7: play_from_cursor(); return true;
            case GDK_Return: _return_request(); return true;
            case GDK_Insert: move_frames(1); return true;
            case GDK_Delete: move_frames(-1); return true;
            case GDK_Left: navigate(-1,0,shift_down); return true;
            case GDK_Right: navigate(1,0,shift_down); return true;
            case GDK_Up: navigate(0,-1,shift_down); return true;
            case GDK_Down: navigate(0,1,shift_down); return true;
            case GDK_Page_Up: navigate(0,-get_page_step_size(),shift_down); return true;
            case GDK_Page_Down: navigate(0,get_page_step_size(),shift_down); return true;
            case GDK_KP_Divide: set_octave(get_octave()-1); return true;
            case GDK_KP_Multiply: set_octave(get_octave()+1); return true;
            case GDK_Home: new_cursor.home(); set_cursor(new_cursor,shift_down); return true;
            case GDK_End: new_cursor.end(); set_cursor(new_cursor,shift_down); return true;
            case GDK_ISO_Left_Tab:
            case GDK_Tab: {
                if (shift_down)
                    new_cursor.prev_channel();
                else
                    new_cursor.next_channel();
                set_cursor(new_cursor);
                return true;
            } break;
            case GDK_plus:
            case GDK_KP_Add:
            {
                if (shift_down)
                    transpose(1);
                return true;
            } break;
            case GDK_minus:
            case GDK_KP_Subtract:
            {
                if (shift_down)
                    transpose(-1);
                return true;
            } break;
            default: {
                CellRenderer *renderer = get_cell_renderer(cursor.get_param());
                if (renderer) {
                    Pattern::Event evt;
                    evt.frame = cursor.get_row();
                    evt.channel = cursor.get_channel();
                    evt.param = cursor.get_param();
                    Pattern::iterator i = get_event(cursor);
                    if (i != get_pattern()->end())
                        evt = i->second;
                    if (renderer->on_key_press_event(event, evt, 
                                                     cursor.get_item())) {
                        evt.sanitize_value();
                        if (evt.is_valid())
                            get_pattern()->add_event(evt);
                        else if (i != get_pattern()->end())
                            get_pattern()->erase(i);
                        invalidate_cursor();
                        return true;
                    }
                }            
            } break;
        }
        switch (event->hardware_keycode) {
            case ScanOctaveUp: set_octave(get_octave()+1); return true;
            case ScanOctaveDown: set_octave(get_octave()-1); return true;
            default:
                break;
        }
    }
    fprintf(stderr, "No handler for %s (0x%02x)\n", 
        gdk_keyval_name(event->keyval), event->hardware_keycode);
    return true;
}

bool PatternView::on_key_release_event(GdkEventKey* event) {
    return false;
}

Pattern::iterator PatternView::get_event(PatternCursor &cur) {
    return get_pattern()->get_event(
        cur.get_row(), cur.get_channel(), cur.get_param());
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

void PatternView::play_pattern() {
    if (!get_pattern())
        return;
    int frame = song_event->second.frame;
    _play_request(frame);
}

void PatternView::play_from_mouse_cursor() {
    if (!get_pattern())
        return;
    PatternCursor new_cursor(cursor);
    new_cursor.set_pos(cursor_x,cursor_y);
    int frame = song_event->second.frame + new_cursor.get_row();
    _play_request(frame);
}

void PatternView::play_from_cursor() {
    if (!get_pattern())
        return;
    int frame = song_event->second.frame + cursor.get_row();
    _play_request(frame);
}

void PatternView::play_event(const Pattern::Event &event) {
    if (!get_pattern())
        return;
    _play_event_request(song_event->second.track, event);
}

void PatternView::set_font_size(int width, int height) {
    font_width = width;
    font_height = height;
}

void PatternView::get_font_size(int &width, int &height) const {
    width = font_width;
    height = font_height;
}

int PatternView::get_page_scroll_size() const {
    return model->frames_per_beat;
}

int PatternView::get_page_step_size() const {
    if (vadjustment) {
        int page_size = vadjustment->get_page_size();
        return std::min(page_size,get_frames_per_bar());
    }
    return get_frames_per_bar();
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

void PatternView::update_navigation_status() {
    if (!get_pattern()) {
        _navigation_status_request("","","");
        return;
    }
    Measure measure;
    measure.set_frame(*model, cursor.get_row()); 
    int param = cursor.get_param();
    int value = ValueNone;
    Pattern::iterator event = get_pattern()->get_event(
        cursor.get_row(), cursor.get_channel(), cursor.get_param());
    if (event != get_pattern()->end()) {
        value = event->second.value;
    }
    
    char timestr[64];
    sprintf(timestr, "%s #%i", 
        measure.get_string().c_str(), cursor.get_channel()); 
    
    _navigation_status_request(timestr, 
        model->get_param_name(param), 
        model->format_param_value(param,value));
}

PatternView::type_play_event_request PatternView::signal_play_event_request() {
    return _play_event_request;
}

PatternView::type_return_request PatternView::signal_return_request() {
    return _return_request;
}

PatternView::type_play_request PatternView::signal_play_request() {
    return _play_request;
}

PatternView::type_navigation_status_request 
    PatternView::signal_navigation_status_request() {
    return _navigation_status_request;
}

//=============================================================================

} // namespace Jacker
#include "patternview.hpp"
#include "model.hpp"

#include <cassert>

namespace Jacker {

//=============================================================================

PatternView::Cursor::Cursor() {
    row = 0;
    channel = 0;
    param = 0;
    row_height = 0;
    cell_width = 0;
    x = 0;
    y = 0;
    this->start_x = 0;
    this->start_y = 0;
    cell_margin = 0;
    channel_margin = 0;
    row_margin = 0;
}

void PatternView::Cursor::next_row() {
    x = start_x;
    y += row_height + row_margin;
    
    row++;
    channel = 0;
    param = 0;
    row_height = 0;
    cell_width = 0;
}

void PatternView::Cursor::next_channel() {
    // remove last added cell margin
    x += cell_width + channel_margin - cell_margin;
    
    channel++;
    param = 0;
    cell_width = 0;
}

void PatternView::Cursor::next_param() {
    x += cell_width + cell_margin;
    
    param++;
    cell_width = 0;
}

bool PatternView::Cursor::is_last_param() const {
    return param == ParamCount-1;
}

int PatternView::Cursor::get_row() const {
    return row;
}

int PatternView::Cursor::get_channel() const {
    return channel;
}

int PatternView::Cursor::get_param() const {
    return param;
}

int PatternView::Cursor::alloc_row_height(int height) {
    row_height = std::max(row_height, height);
    return row_height;
}

int PatternView::Cursor::alloc_cell_width(int width) {
    cell_width = std::max(cell_width, width);
    return cell_width;
}

void PatternView::Cursor::get_pos(int &x, int &y) const {
    x = this->x;
    y = this->y;
}

void PatternView::Cursor::set_start_pos(int x, int y) {
    start_x = x;
    start_y = y;
}

void PatternView::Cursor::set_cell_margin(int margin) {
    cell_margin = margin;
}

void PatternView::Cursor::set_channel_margin(int margin) {
    channel_margin = margin;
}

void PatternView::Cursor::set_row_margin(int margin) {
    row_margin = margin;
}

void PatternView::Cursor::get_cell_size(int &w, int &h) const {
    w = cell_width;
    h = row_height;
}

int PatternView::Cursor::get_cell_margin() const {
    return cell_margin;
}

int PatternView::Cursor::get_channel_margin() const {
    return channel_margin;
}

int PatternView::Cursor::get_row_margin() const {
    return row_margin;
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

static int sprint_byte(char *buffer, int value) {
    if (value == ValueNone) {
        sprintf(buffer, "..");
    } else {
        sprintf(buffer, "%02X", value);
    }
    return 2;
}

//=============================================================================

PatternView::PatternView(BaseObjectType* cobject, 
             const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    model = NULL;
    pattern = NULL;
    text_width = 0;
    text_height = 0;
    frames_per_beat = 4;
    beats_per_bar = 4;
}

void PatternView::render_background(Cursor &cursor) {
    
    int row = cursor.get_row();
    int frames_per_bar = frames_per_beat * beats_per_bar;
    
    if (!(row % frames_per_bar)) {
        gc->set_foreground(row_color_bar);
    } else if (!(row % frames_per_beat)) {
        gc->set_foreground(row_color_beat);
    }
    else {
        return; // nothing to draw
    }
    
    int x, y, w, h;
    cursor.get_pos(x,y);
    cursor.get_cell_size(w,h);
    if (!cursor.is_last_param()) {
        w += cursor.get_cell_margin();
    }
    window->draw_rectangle(gc, true, x, y, w, h);
}

void PatternView::render_cell(Cursor &cursor, Pattern::Event *event) {
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    switch(cursor.get_param()) {
    case ParamNote: {
        chars = sprint_note(text, value);
    } break;
    case ParamVolume: {
        chars = sprint_byte(text, value);
    } break;
    case ParamCCIndex0:
    case ParamCCIndex1: {
        chars = sprint_byte(text, value);
    } break;
    case ParamCCValue0:
    case ParamCCValue1: {
        chars = sprint_byte(text, value);
    } break;
    default: {
        assert(0); // there must be a case for every param
    } break;
    }
    
    cursor.alloc_cell_width(text_width*chars);
    
    render_background(cursor);
    
    gc->set_foreground(fgcolor);
    layout->set_text(text);
    
    int x, y;
    cursor.get_pos(x,y);
    window->draw_layout(gc, x, y, layout);
}

void PatternView::select_pattern(Model &model, Pattern &pattern) {
    this->model = &model;
    this->pattern = &pattern;
}

void PatternView::on_realize() {
    Gtk::Widget::on_realize();
    
    window = get_window();    
    
    // create drawing resources
    gc = Gdk::GC::create(window);
    cm = Gdk::Colormap::get_system();
    
    bgcolor.set("#ffffff"); cm->alloc_color(bgcolor);
    fgcolor.set("#000000"); cm->alloc_color(fgcolor);
    row_color_bar.set("#c0c0c0"); cm->alloc_color(row_color_bar);
    row_color_beat.set("#e0e0e0"); cm->alloc_color(row_color_beat);
    
    Pango::FontDescription font_desc("monospace 8");
    
    layout = Pango::Layout::create(get_pango_context());
    layout->set_font_description(font_desc);
    layout->set_width(-1);
    
    // measure width of a single character
    layout->set_text("W");
    layout->get_pixel_size(text_width, text_height);
}

bool PatternView::on_expose_event(GdkEventExpose* event) {
    int width = 0;
    int height = 0;
    window->get_size(width, height);
    
    // clear screen
    gc->set_foreground(bgcolor);
    window->draw_rectangle(gc, true, 0, 0, width, height);
    
    // build temporary row
    Pattern::Row row;
    // start iterating at start of pattern
    Pattern::iterator iter = pattern->begin();    
    
    int frame_count = pattern->get_length();
    int channel_count = pattern->get_channel_count();
    
    Cursor render_cursor;
    render_cursor.set_cell_margin(5);
    render_cursor.set_channel_margin(10);
    
    for (int frame = 0; frame < frame_count; ++frame) {
        // collect events from pattern
        pattern->collect_events(frame, iter, row);
        
        render_cursor.alloc_row_height(text_height);
        // now render all channels
        for (int channel = 0; channel < channel_count; ++channel) {
            // and all params in one channel
            for (int param = 0; param < ParamCount; ++param) {
                render_cell(render_cursor, row.get_event(channel, param));
                render_cursor.next_param();
            }
            render_cursor.next_channel();
        }
        render_cursor.next_row();
    }

    return true;
}

//=============================================================================

} // namespace Jacker
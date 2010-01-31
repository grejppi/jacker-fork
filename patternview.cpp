#include "patternview.hpp"
#include "model.hpp"

#include <cassert>

namespace Jacker {

//=============================================================================

void CellRenderer::render_background(PatternView &view, 
                                     PatternCursor &cursor) {
    int row = cursor.get_row();
    int frames_per_bar = view.frames_per_beat * view.beats_per_bar;
    
    if (!(row % frames_per_bar)) {
        view.gc->set_foreground(view.row_color_bar);
    } else if (!(row % view.frames_per_beat)) {
        view.gc->set_foreground(view.row_color_beat);
    }
    else {
        return; // nothing to draw
    }
    
    int x, y, w, h;
    cursor.get_pos(x,y);
    cursor.get_cell_size(w,h);
    if (!cursor.is_last_param()) {
        w += cursor.get_layout().get_cell_margin();
    }
    view.window->draw_rectangle(view.gc, true, x, y, w, h);
}

void CellRenderer::render_cell(PatternView &view, PatternCursor &cursor, 
                               Pattern::Event *event) {

}

int CellRenderer::get_width(const PatternLayout &layout) {
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
                                   Pattern::Event *event) {
                                       
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_note(text, value);
    
    render_background(view, cursor);
    
    view.gc->set_foreground(view.fgcolor);
    view.pango_layout->set_text(text);
    
    int x, y;
    cursor.get_pos(x,y);
    view.window->draw_layout(view.gc, x, y, view.pango_layout);

}

int CellRendererNote::get_width(const PatternLayout &layout) {
    int w,h;
    layout.get_text_size(w,h);
    return 3 * w;
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
                                   Pattern::Event *event) {
                                       
    char text[16];
    
    int chars = 0;
    int value = ValueNone;
    if (event)
        value = event->value;
    
    chars = sprint_byte(text, value);
    
    render_background(view, cursor);
    
    view.gc->set_foreground(view.fgcolor);
    view.pango_layout->set_text(text);
    
    int x, y;
    cursor.get_pos(x,y);
    view.window->draw_layout(view.gc, x, y, view.pango_layout);

}

int CellRendererByte::get_width(const PatternLayout &layout) {
    int w,h;
    layout.get_text_size(w,h);
    return 2 * w;
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

void PatternLayout::set_text_size(int width, int height) {
    text_width = width;
    text_height = height;
}

void PatternLayout::get_text_size(int &width, int &height) const {
    width = text_width;
    height = text_height;
}

//=============================================================================

PatternCursor::PatternCursor(const PatternLayout &layout) {
    this->layout = layout;
    row = 0;
    channel = 0;
    param = 0;
    x = 0;
    y = 0;
}

const PatternLayout &PatternCursor::get_layout() const {
    return layout;
}

void PatternCursor::origin() {
    layout.get_origin(x,y);
}

void PatternCursor::next_row() {
    row++;
    channel = 0;
    param = 0;
    
    update_position();
}

void PatternCursor::next_channel() {
    channel++;
    param = 0;
    
    update_position();
}

void PatternCursor::next_param() {
    param++;
    
    update_position();
}

bool PatternCursor::is_last_param() const {
    return param == get_layout().get_cell_count()-1;
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

void PatternCursor::get_pos(int &x, int &y) const {
    x = this->x;
    y = this->y;
}

void PatternCursor::get_cell_size(int &w, int &h) const {
    layout.get_cell_size(param, w, h);
}

void PatternCursor::update_position() {
    layout.get_cell_pos(row, channel, param, x,y);
}

//=============================================================================

PatternView::PatternView(BaseObjectType* cobject, 
             const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {
    model = NULL;
    pattern = NULL;
    frames_per_beat = 4;
    beats_per_bar = 4;
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
    
    pango_layout = Pango::Layout::create(get_pango_context());
    pango_layout->set_font_description(font_desc);
    pango_layout->set_width(-1);
    
    // measure width of a single character
    pango_layout->set_text("W");
    int text_width, text_height;
    pango_layout->get_pixel_size(text_width, text_height);

    // setup pattern layout
    layout.set_text_size(text_width, text_height);
    layout.set_origin(5,5);
    layout.set_cell_margin(5);
    layout.set_channel_margin(10);
    layout.set_row_height(text_height);
    
    layout.set_cell_renderer(ParamNote, &note_renderer);
    layout.set_cell_renderer(ParamVolume, &byte_renderer);
    layout.set_cell_renderer(ParamCCIndex0, &byte_renderer);
    layout.set_cell_renderer(ParamCCValue0, &byte_renderer);
    layout.set_cell_renderer(ParamCCIndex1, &byte_renderer);
    layout.set_cell_renderer(ParamCCValue1, &byte_renderer);
    
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
    
    PatternCursor render_cursor(layout);
    render_cursor.origin();
    
    for (int frame = 0; frame < frame_count; ++frame) {
        // collect events from pattern
        pattern->collect_events(frame, iter, row);
        
        // now render all channels
        for (int channel = 0; channel < channel_count; ++channel) {
            // and all params in one channel
            for (int param = 0; param < layout.get_cell_count(); ++param) {
                CellRenderer *renderer = layout.get_cell_renderer(param);
                if (renderer) {
                    renderer->render_cell(*this,
                        render_cursor, row.get_event(channel, param));
                }
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
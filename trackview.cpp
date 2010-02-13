#include "trackview.hpp"

#include <cassert>

namespace Jacker {

enum {
    TrackHeight = 22,
};

//=============================================================================

TrackBar::TrackBar(int index, TrackView &view) {
    this->model = view.model;
    this->index = index;
    set_spacing(5);
    set_size_request(-1, TrackHeight);
    
    name.set_alignment(0,0.5);
    pack_start(name, true, true);
    view.group_names->add_widget(name);
    
    for (int i = 0; i < 16; ++i) {
        char buffer[64];
        sprintf(buffer, "Channel %i", i+1);
        channel_menu.items().push_back(
            Gtk::Menu_Helpers::MenuElem(buffer, 
                sigc::bind<int>(
                    sigc::mem_fun(*this, &TrackBar::on_channel), i)                    
            )
        );
    }

    for (int i = 0; i < MaxPorts; ++i) {
        char buffer[64];
        sprintf(buffer, "port-%i", i);
        port_menu.items().push_back(
            Gtk::Menu_Helpers::MenuElem(buffer, 
                sigc::bind<int>(
                    sigc::mem_fun(*this, &TrackBar::on_port), i)                    
            )
        );
    }
    
    channel.set_alignment(1,0.5);
    view.group_channels->add_widget(channel);
    channel_eventbox.add(channel);
    channel_eventbox.set_visible_window(false);
    channel_eventbox.set_events(Gdk::BUTTON_PRESS_MASK);
    channel_eventbox.signal_button_press_event().connect(
        sigc::mem_fun(*this, &TrackBar::on_channel_button_press_event));
    pack_start(channel_eventbox, false, true);
    
    port.set_alignment(0,0.5);
    view.group_ports->add_widget(port);
    port_eventbox.add(port);
    port_eventbox.set_visible_window(false);
    port_eventbox.set_events(Gdk::BUTTON_PRESS_MASK);
    port_eventbox.signal_button_press_event().connect(
        sigc::mem_fun(*this, &TrackBar::on_port_button_press_event));
    pack_start(port_eventbox, false, true);
    
    update();
}

void TrackBar::on_channel(int channel) {
    model->tracks[index].midi_channel = channel;
    update();
}

void TrackBar::on_port(int port) {
    model->tracks[index].midi_port = port;
    update();
}

bool TrackBar::on_channel_button_press_event(GdkEventButton *event) {
    if ((event->type == GDK_BUTTON_PRESS) &&
        (event->button == 1)) {
        channel_menu.popup(event->button, event->time);
        return true;
    }
    return false;
}

bool TrackBar::on_port_button_press_event(GdkEventButton *event) {
    if ((event->type == GDK_BUTTON_PRESS) &&
        (event->button == 1)) {
        port_menu.popup(event->button, event->time);
        return true;
    }
    return false;
}

void TrackBar::update() {
    char buffer[64];
    sprintf(buffer, "Track %i", index+1);
    name.set_text(buffer);
    
    Track &track = model->tracks[index];
    
    sprintf(buffer, "CH%i", track.midi_channel+1);
    channel.set_text(buffer);
    
    sprintf(buffer, "port-%i", track.midi_port);
    port.set_text(buffer);
}

//=============================================================================

TrackView::TrackView(BaseObjectType* cobject, 
    const Glib::RefPtr<Gtk::Builder>& builder) 
    : Gtk::VBox(cobject) {
    model = NULL;
    group_names = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    group_channels = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    group_ports = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
}

void TrackView::destroy_bars() {
    TrackBarArray::iterator iter;
    for (iter = bars.begin(); iter != bars.end(); ++iter) {
        delete *iter;
    }
    bars.clear();

}

void TrackView::set_model(class Model &model) {
    this->model = &model;
}

void TrackView::update_tracks() {
    assert(model);
    destroy_bars();

    for (size_t i = 0; i < model->tracks.size(); i++) {
        TrackBar *bar = new TrackBar((int)i, *this);
        bars.push_back(bar);
        pack_start(*bar, false, false);
    }
}

//=============================================================================

} // namespace Jacker
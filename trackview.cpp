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
    this->view = &view;
    set_spacing(5);
    set_size_request(-1, TrackHeight);
    
    name.set_alignment(0,0.5);
    view.group_names->add_widget(name);

    name_eventbox.add(name);
    name_eventbox.set_visible_window(false);
    name_eventbox.set_events(Gdk::BUTTON_PRESS_MASK);
    name_eventbox.signal_button_press_event().connect(
        sigc::mem_fun(*this, &TrackBar::on_name_button_press_event));

    pack_start(name_eventbox, true, true);
    
    for (int i = 0; i < 16; ++i) {
        char buffer[64];
        sprintf(buffer, "Channel %i", i+1);
        channel_menu.items().push_back(
            Gtk::Menu_Helpers::RadioMenuElem(channel_radio_group, buffer, 
                sigc::bind<int>(
                    sigc::mem_fun(*this, &TrackBar::on_channel), i)                    
            )
        );
    }

    for (int i = 0; i < MaxPorts; ++i) {
        char buffer[64];
        sprintf(buffer, "port-%i", i);
        port_menu.items().push_back(
            Gtk::Menu_Helpers::RadioMenuElem(port_radio_group, buffer, 
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

    mute.set_label("M");
    mute.signal_toggled().connect(
        sigc::mem_fun(*this, &TrackBar::on_mute_toggled));
    view.group_mutes->add_widget(mute);
    pack_start(mute, false, true);
    
    update();
    
    view.pack_start(*this, false, false);
}

TrackBar::~TrackBar() {
    view->group_names->remove_widget(name);
    view->group_channels->remove_widget(channel);
    view->group_ports->remove_widget(port);
    view->remove(*this);
}

void TrackBar::on_mute_toggled() {
    bool active = mute.get_active();
    if (model->tracks[index].mute == active)
        return;
    model->tracks[index].mute = active;
    update();
    view->_mute_toggled(index, active);
}

void TrackBar::on_channel(int channel) {
    model->tracks[index].midi_channel = channel;
    update();
}

void TrackBar::on_port(int port) {
    model->tracks[index].midi_port = port;
    update();
}

bool TrackBar::on_name_button_press_event(GdkEventButton *event) {
    if ((event->type == GDK_BUTTON_PRESS) &&
        (event->button == 1)) {
        Track &track = model->tracks[index];
        model->midi_control_port = track.midi_port;
        return true;
    }
    return false;
}

bool TrackBar::on_channel_button_press_event(GdkEventButton *event) {
    if ((event->type == GDK_BUTTON_PRESS) &&
        (event->button == 1)) {
        int channel = model->tracks[index].midi_channel;
        Gtk::CheckMenuItem &item = (Gtk::CheckMenuItem &)
            channel_menu.items()[channel];
        item.set_active();
        channel_menu.popup(event->button, event->time);
        return true;
    }
    return false;
}

bool TrackBar::on_port_button_press_event(GdkEventButton *event) {
    if ((event->type == GDK_BUTTON_PRESS) &&
        (event->button == 1)) {
        int port = model->tracks[index].midi_port;
        Gtk::CheckMenuItem &item = (Gtk::CheckMenuItem &)
            port_menu.items()[port];
        item.set_active();
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
    
    mute.set_active(track.mute);
}

//=============================================================================

TrackView::TrackView(BaseObjectType* cobject, 
    const Glib::RefPtr<Gtk::Builder>& builder) 
    : Gtk::VBox(cobject) {
    model = NULL;
    group_names = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    group_channels = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    group_ports = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    group_mutes = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
}

TrackView::~TrackView() {
}

void TrackView::destroy_bars() {
    TrackBarArray::iterator iter;
    for (iter = bars.begin(); iter != bars.end(); ++iter) {
        delete *iter;
    }
    bars.clear();

}

TrackView::type_mute_toggled TrackView::signal_mute_toggled() {
    return _mute_toggled;
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
    }
    
    show_all();
}

//=============================================================================

} // namespace Jacker
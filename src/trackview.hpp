#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

#include <vector>

namespace Jacker {

//=============================================================================

class TrackBar : public Gtk::HBox {
public:
    TrackBar(int index, class TrackView &view);
    ~TrackBar();
    
    Gtk::Label name;
    Gtk::EventBox name_eventbox;
    
    Gtk::EventBox channel_eventbox;
    Gtk::Menu channel_menu;
    Gtk::Label channel;
    Gtk::RadioButtonGroup channel_radio_group;
    
    Gtk::EventBox port_eventbox;
    Gtk::Menu port_menu;
    Gtk::Label port;
    Gtk::RadioButtonGroup port_radio_group;
    Gtk::ToggleButton mute;

    Model *model;
    class TrackView *view;
    int index;

    void update();
    bool on_channel_button_press_event(GdkEventButton *event);
    bool on_port_button_press_event(GdkEventButton *event);
    bool on_name_button_press_event(GdkEventButton *event);
    void on_mute_toggled();

    void on_channel(int channel);
    void on_port(int port);
};

//=============================================================================

class TrackView : public Gtk::VBox {
    friend class TrackBar;
public:
    typedef std::vector<TrackBar *> TrackBarArray;    
    typedef sigc::signal<void, int, bool> type_mute_toggled;

    TrackView(BaseObjectType* cobject, 
            const Glib::RefPtr<Gtk::Builder>& builder);
    ~TrackView();

    Glib::RefPtr<Gtk::SizeGroup> group_names;
    Glib::RefPtr<Gtk::SizeGroup> group_channels;
    Glib::RefPtr<Gtk::SizeGroup> group_ports;
    Glib::RefPtr<Gtk::SizeGroup> group_mutes;

    Model *model;
    TrackBarArray bars;

    void destroy_bars();
    void set_model(class Model &model);
    void update_tracks();

    type_mute_toggled signal_mute_toggled();

protected:
    type_mute_toggled _mute_toggled;
};

//=============================================================================

} // namespace Jacker
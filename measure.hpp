#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

namespace Jacker {

//=============================================================================

class MeasureView : public Gtk::Widget {
public:
    enum Orientation {
        OrientationHorizontal = 0,
        OrientationVertical,
    };
    
    typedef sigc::signal<void, int> type_seek_request;
    typedef sigc::signal<void> type_loop_changed;

    MeasureView(BaseObjectType* cobject, 
                const Glib::RefPtr<Gtk::Builder>& builder);

    void set_model(class Model &model);

    virtual void on_realize();
    virtual bool on_expose_event(GdkEventExpose* event);
    virtual bool on_motion_notify_event(GdkEventMotion *event);
    virtual bool on_button_press_event(GdkEventButton* event);
    virtual bool on_button_release_event(GdkEventButton* event);
    virtual bool on_key_press_event(GdkEventKey* event);
    virtual bool on_key_release_event(GdkEventKey* event);
    virtual void on_size_allocate(Gtk::Allocation& allocation);

    void set_adjustment(Gtk::Adjustment *adjustment);
    void set_orientation(Orientation orientation);
    void invalidate();

    Glib::RefPtr<Gdk::Window> window;
    Glib::RefPtr<Gdk::GC> gc;
    Glib::RefPtr<Gdk::Colormap> cm;
    Glib::RefPtr<Pango::Layout> pango_layout;
    std::vector<Gdk::Color> colors;
    
    type_seek_request signal_seek_request();
    type_loop_changed signal_loop_changed();
protected:
    void on_adjustment_value_changed();
    void flip(int &x, int &y);
    void flip(int &x, int &y, int &w, int &h);
    int get_frame(int x, int y);

    Orientation orientation;
    Gtk::Adjustment *adjustment;
    Model *model;

    type_seek_request _seek_request;
    type_loop_changed _loop_changed;
};
    
} // namespace Jacker

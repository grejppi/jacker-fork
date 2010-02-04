#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

namespace Jacker {

class SeqView;
    
//=============================================================================

class SeqCursor {
public:
    SeqCursor();
    
    void set_view(SeqView &view);
    SeqView *get_view() const;

    void set_track(int track);
    int get_track() const;

    void set_frame(int frame);
    int get_frame() const;

protected:
    // pointer to the view
    SeqView *view;

    // the current track the cursor is on
    int track;
    // the frame the cursor is on
    int frame;
};
    
//=============================================================================

class SeqView : public Gtk::Widget {
public:
    SeqView(BaseObjectType* cobject, 
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

    void set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                Gtk::Adjustment *vadjustment);

    Glib::RefPtr<Gdk::Window> window;

protected:
    Model *model;
};
    
//=============================================================================
    
} // namespace Jacker
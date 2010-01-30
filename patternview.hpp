#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

namespace Jacker {

//=============================================================================

class PatternView : public Gtk::Widget {
public:
    PatternView(BaseObjectType* cobject, 
                const Glib::RefPtr<Gtk::Builder>& builder);

    void select_pattern(class Model &model, class Pattern &pattern);

    virtual void on_realize();
    virtual bool on_expose_event(GdkEventExpose* event);
protected:
    class Model *model;
    class Pattern *pattern;
};

//=============================================================================

} // namespace Jacker
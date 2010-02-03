#pragma once

#include <gtkmm.h>

#if !defined(GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED)
#error GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED must be set.
#endif    

#include "model.hpp"

namespace Jacker {
    
//=============================================================================

class SeqView : public Gtk::Widget {
public:
    SeqView(BaseObjectType* cobject, 
            const Glib::RefPtr<Gtk::Builder>& builder);

    void set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                Gtk::Adjustment *vadjustment);

};
    
//=============================================================================
    
} // namespace Jacker
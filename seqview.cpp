#include "seqview.hpp"
#include "model.hpp"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace Jacker {
    
//=============================================================================

SeqView::SeqView(BaseObjectType* cobject, 
                 const Glib::RefPtr<Gtk::Builder>& builder)
    : Gtk::Widget(cobject) {

}

void SeqView::set_scroll_adjustments(Gtk::Adjustment *hadjustment, 
                                     Gtk::Adjustment *vadjustment) {
}

//=============================================================================

} // namespace Jacker
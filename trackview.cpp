#include "trackview.hpp"

namespace Jacker {

//=============================================================================

TrackView::TrackView(BaseObjectType* cobject, 
    const Glib::RefPtr<Gtk::Builder>& builder) 
    : Gtk::VBox(cobject) {
    
}

void TrackView::set_model(class Model &model) {
}

void TrackView::update_tracks() {
}

//=============================================================================

} // namespace Jacker
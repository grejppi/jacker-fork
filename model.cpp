#include "model.hpp"

namespace Jacker {

//=============================================================================

PatternEvent::Command::Command() {
    cc = value = -1;
}

PatternEvent::PatternEvent() {
    frame = -1;
    channel = -1;
    note = -1;
    volume = -1;
}

//=============================================================================

Pattern::Pattern() {
    length = 0;
}

//=============================================================================

Model::Model() {
    end_cue = 0;
}

Pattern &Model::new_pattern() {
    Pattern *pattern = new Pattern();
    patterns.push_back(pattern);
    return *pattern;
}

//=============================================================================

} // namespace Jacker
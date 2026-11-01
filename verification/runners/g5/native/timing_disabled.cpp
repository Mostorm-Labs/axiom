#include "canvas/foundation/internal/frame_timing.hpp"
int main(){int sideEffect=0;AXIOM_FRAME_TIMING_BEGIN(++sideEffect);AXIOM_FRAME_TIMING_ADD(missing,++sideEffect);AXIOM_FRAME_TIMING_OUTCOME(++sideEffect);return sideEffect;}

/*
 * Storage for the in-game randomizer's settings (see randomizer_state.h). Linked right
 * before gPool, so it only moves the start of the memory pool. Empty unless
 * RANDOMIZER=1, so the default build still matches the original ROM.
 */
#include "randomizer_state.h"

#ifdef RANDOMIZER
RandomizerState gRandomizerState;
#endif

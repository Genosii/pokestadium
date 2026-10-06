#ifndef _FRAGMENT61_RANDOMIZER_MENU_H_
#define _FRAGMENT61_RANDOMIZER_MENU_H_

#include "global.h"

#ifdef RANDOMIZER
#include "src/controller.h"
#include "src/randomizer_state.h"

/*
 * The randomizer's options panels and settings are a fragment of their own,
 * randomizer_menu (linker_scripts/us/randomizer.ld), shared by the screens that show the
 * panels, the pick screen (fragment61) and Rules (fragment55), and by Options (fragment56),
 * which has the battle camera setting. Each
 * loads it before its own randomizer fragment, whose references to it are then relocated
 * to where it is. It refers to no screen's code or data itself, so any of them can load it.
 */

// The options window's tabs
enum {
    RANDOMIZER_TAB_MODE,
    RANDOMIZER_TAB_PLAYER,
    RANDOMIZER_TAB_OPPONENT,
    RANDOMIZER_TAB_COUNT,
    RANDOMIZER_PANEL_CLOSED = -1
};

extern u8 randomizer_menu_TEXT_START[];
extern u8 randomizer_menu_ROM_START[];
extern u8 randomizer_menu_relocs_ROM_END[];

void Randomizer_MenuEntry(void);

// The settings, set up the first time (from the save file, or the defaults)
RandomizerState* Randomizer_State(void);
// Writes the settings to the save file, if they changed since they were read or written
void Randomizer_SaveSettings(void);

// redraw: called when the screen should redraw what a closing panel leaves behind (or NULL)
void Randomizer_PanelReset(void (*redraw)(void));
void Randomizer_PanelOpen(s32 tab);
s32 Randomizer_PanelIsOpen(void);
// 1 if the window took this frame's input: it was open, or C-Up just opened it
s32 Randomizer_PanelInput(Controller* cont);
void Randomizer_PanelDraw(void);
const char* Randomizer_ModeName(void);

#define RANDOMIZER_DPAD (BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT)
// Which of the buttons were pressed this frame, or have been held long enough to repeat:
// for going through lists by holding the D-pad. (A button held, not just pressed, is in
// the result but not in cont->buttonPressed: lists stop at their ends rather than wrap.)
u16 Randomizer_Repeat(Controller* cont, u16 buttons);
#endif

#endif // _FRAGMENT61_RANDOMIZER_MENU_H_

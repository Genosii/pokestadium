#ifndef _FRAGMENT61_RANDOMIZER_MENU_H_
#define _FRAGMENT61_RANDOMIZER_MENU_H_

#include "global.h"

#ifdef RANDOMIZER
#include "src/controller.h"
#include "src/randomizer_state.h"

/*
 * The randomizer's options panels and settings are a fragment of their own,
 * randomizer_menu (linker_scripts/us/randomizer.ld), shared by the screens that show the
 * panels: the pick screen (fragment61), Options (fragment56) and Rules (fragment55). Each
 * loads it before its own randomizer fragment, whose references to it are then relocated
 * to where it is. It refers to no screen's code or data itself, so any of them can load it.
 */

// The two panels
enum { RANDOMIZER_PANEL_TEAM, RANDOMIZER_PANEL_OPPONENTS, RANDOMIZER_PANEL_COUNT, RANDOMIZER_PANEL_CLOSED = -1 };

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
void Randomizer_PanelOpen(s32 panel);
s32 Randomizer_PanelIsOpen(void);
// 1 if the panel took this frame's input: it was open, or C-Up or C-Right just opened it
s32 Randomizer_PanelInput(Controller* cont);
void Randomizer_PanelDraw(void);
const char* Randomizer_ModeName(void);
#endif

#endif // _FRAGMENT61_RANDOMIZER_MENU_H_

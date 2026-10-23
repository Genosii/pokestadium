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

// The options window's tabs, and "Choose your rules" (one tab of its own, no L/R)
enum {
    RANDOMIZER_TAB_MODE,
    RANDOMIZER_TAB_PLAYER,
    RANDOMIZER_TAB_OPPONENT,
    RANDOMIZER_TAB_COUNT,
    RANDOMIZER_TAB_FRONTIER = RANDOMIZER_TAB_COUNT,
    RANDOMIZER_PANEL_CLOSED = -1
};

// "Choose your rules"' cups: which of the Stadium's rule sets, trainers and screens a run has
enum { FRONTIER_CUP_POKE, FRONTIER_CUP_PRIME, FRONTIER_CUP_PETIT, FRONTIER_CUP_PIKA, FRONTIER_CUP_COUNT };

#define FRONTIER_MONO_TYPES 15 // Gen 1's types, after "Off"

/*
 * A Battle Frontier mode's rules, as "Choose your rules" sets them; saved for each mode
 * (randomizer_menu.c). The rest is fixed by the mode: Factory's DVs and stat exp at their
 * highest, Rogue's those of the game's rentals and trainers (which build up from ball to
 * ball), and no Shared types rule.
 */
typedef struct RandomizerFrontierRules {
    /* 0x0 */ u8 cup; // FRONTIER_CUP_*
    /* 0x1 */ u8 randomOpponents;
    /* 0x2 */ u8 autoBattlePick;
    /* 0x3 */ u8 moveset; // RandomizerMoveset, the player's
    /* 0x4 */ u8 opponentMoveset;
    /* 0x5 */ u8 tradeback;
    /* 0x6 */ u8 legendaries;
    /* 0x7 */ u8 finalEvos; // on: only final evolutions; off: RANDOMIZER_NOT_FINAL_EVOS
    /* 0x8 */ u8 monoType;  // 0 Off, else one of the FRONTIER_MONO_TYPES, the player's
    /* 0x9 */ u8 pad[3];
} RandomizerFrontierRules; // size = 0xC

extern u8 randomizer_menu_TEXT_START[];
extern u8 randomizer_menu_ROM_START[];
extern u8 randomizer_menu_relocs_ROM_END[];

void Randomizer_MenuEntry(void);

// The settings, set up the first time (from the save file, or the defaults)
RandomizerState* Randomizer_State(void);
// Writes the settings to the save file, if they changed since they were read or written
// (not during a Battle Frontier run, while they're the run's)
void Randomizer_SaveSettings(void);

// The Battle Frontier's rules for a mode (FRONTIER_FACTORY or FRONTIER_ROGUE), read from
// the save file the first time after Randomizer_FrontierReset
void Randomizer_FrontierReset(void);
RandomizerFrontierRules* Randomizer_FrontierRules(s32 kind);
// Writes them to the save file if they changed
void Randomizer_FrontierSave(void);
// The game's rule set (and Stadium mode) for a FRONTIER_CUP_*
s32 Randomizer_FrontierRuleSet(s32 cup);
// A run with these rules starts: they go where the screens read the options from
void Randomizer_FrontierBegin(s32 kind);
// No more run: the player's own options back
void Randomizer_FrontierEnd(void);
// Gen 1 type of a FRONTIER_MONO_TYPES choice (1 and up), and its name
s32 Randomizer_FrontierType(s32 choice);
const char* Randomizer_FrontierTypeName(s32 choice);

// redraw: called when the screen should redraw what a closing panel leaves behind (or NULL)
void Randomizer_PanelReset(void (*redraw)(void));
void Randomizer_PanelOpen(s32 tab);
s32 Randomizer_PanelIsOpen(void);
// 1 if the window took this frame's input: it was open, or C-Up just opened it
s32 Randomizer_PanelInput(Controller* cont);
void Randomizer_PanelDraw(void);
// "Choose your rules" for a Battle Frontier mode, open until Start (1) or B (-1)
void Randomizer_FrontierPanelOpen(s32 kind);
s32 Randomizer_FrontierPanelInput(Controller* cont);

#define RANDOMIZER_DPAD (BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT)
// Which of the buttons were pressed this frame, or have been held long enough to repeat:
// for going through lists by holding the D-pad. (A button held, not just pressed, is in
// the result but not in cont->buttonPressed: lists stop at their ends rather than wrap.)
u16 Randomizer_Repeat(Controller* cont, u16 buttons);
#endif

#endif // _FRAGMENT61_RANDOMIZER_MENU_H_

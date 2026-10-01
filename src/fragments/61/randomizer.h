#ifndef _FRAGMENT61_RANDOMIZER_H_
#define _FRAGMENT61_RANDOMIZER_H_

#include "fragment61.h"

#ifdef RANDOMIZER
#include "randomizer_build.h"
#include "randomizer_menu.h"
#include "src/randomizer_state.h"

/*
 * The randomizer's pick-screen code is a fragment of its own, randomizer_pick
 * (linker_scripts/us/randomizer.ld), so that fragment61 keeps its original size. The
 * pick screen (func_84203E6C) loads it when it starts, after randomizer_core and
 * randomizer_menu (the options panels, randomizer_menu.h), and gets back the functions
 * it calls into it through.
 */
typedef struct RandomizerPickHooks {
    void (*listInput)(unk_D_842168A0* list); // the rental list's input handler (func_8420AA08)
    void (*draw)(void);                      // at the end of each frame's drawing (func_84202718)
    void (*editTeam)(unk_D_84211B50* team);  // "Edit Pokemon" picked in the team's menu (func_8420720C, func_842073A4)
    void (*editInput)(unk_D_84211B50* team); // the team's input while it's edited (func_8420776C)
    s32 (*levelSumTooHigh)(unk_D_84211B50* team); // func_84206A68, moved here to make room in fragment61
    // "Check registered Pokemon"'s input on the teams, with the one highlighted (func_8420F86C):
    // 1 if the teambuilder took it
    s32 (*checkInput)(struct unk_D_84229EB0* viewer, unk_D_84229EB0_00024* set);
    // The rental card's prompt and its input (func_8420B40C, func_8420C368), with "Edit"
    void (*cardPrompt)(s16 x, s16 y, s16 mode, s16 answer);
    void (*cardInput)(unk_D_8423D3A8* card);
} RandomizerPickHooks;

// The team panel's state while the team is edited (unk_D_84211B50.unk_0001, func_8420776C)
#define RANDOMIZER_TEAM_STATE_EDIT 17
// The line "Edit Pokemon" adds to the team's menus: menu 0 (OK, OK to Register, Reselect
// some, Reselect all) and menu 10 (Registration's OK, Reselect some, Reselect all)
#define RANDOMIZER_EDIT_LINE_MENU_0 5
#define RANDOMIZER_EDIT_LINE_MENU_10 4

typedef RandomizerPickHooks* (*RandomizerPickEntry)(void);

extern u8 randomizer_pick_TEXT_START[];
extern u8 randomizer_pick_ROM_START[];
extern u8 randomizer_pick_relocs_ROM_END[];

// In fragment61, set while the pick screen runs
extern RandomizerPickHooks* gRandomizerPickHooks;

// In randomizer_pick
RandomizerPickHooks* Randomizer_PickEntry(void);
void Randomizer_ListInput(unk_D_842168A0* list);
s32 Randomizer_FillTeam(unk_D_842168A0* list);
void Randomizer_TeamReset(void);
void Randomizer_TeamDraw(void);
void Randomizer_HintsDraw(void);
void Randomizer_EditorReset(void);
s32 Randomizer_EditorIsOpen(void);
s32 Randomizer_EditorTakesListInput(void);
void Randomizer_EditorOpen(unk_D_84211B50* team);
void Randomizer_EditorInput(unk_D_84211B50* team);
void Randomizer_EditorDraw(void);
s32 Randomizer_EditorMenuCoversHints(void);
s32 Randomizer_LevelSumTooHigh(unk_D_84211B50* team);
s32 Randomizer_EditorCheckInput(struct unk_D_84229EB0* viewer, unk_D_84229EB0_00024* set);
s32 Randomizer_EditorInCheck(void);
void Randomizer_CardPrompt(s16 x, s16 y, s16 mode, s16 answer);
void Randomizer_CardInput(unk_D_8423D3A8* card);
void Randomizer_EditorAfterCard(void);
#endif

#endif // _FRAGMENT61_RANDOMIZER_H_

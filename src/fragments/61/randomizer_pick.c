/*
 * Entry point of the randomizer's pick-screen fragment (randomizer_pick, see
 * randomizer.h), which the pick screen loads when it starts. Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * It also holds the rental list's input handler, moved here from fragment61
 * (func_8420AA08) with the randomizer's buttons added: C-Up for the options window and
 * Z for a random team. C-Down, held anywhere on the screen, shows the team's moves
 * (randomizer_team.c), and a line under the list shows these buttons
 * (randomizer_hints.c). This fragment is loaded after fragment61, so its references to
 * fragment61's functions and data are relocated to where fragment61 is.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/controller.h"

// Set to have the pick screen redraw everything (func_84202718)
extern s16 D_84210D40;

static void Randomizer_PickDraw(void);
static void Randomizer_PickRedraw(void);

static RandomizerPickHooks sHooks = {
    Randomizer_ListInput,        Randomizer_PickDraw,    Randomizer_EditorOpen,
    Randomizer_EditorInput,      Randomizer_LevelSumTooHigh, Randomizer_EditorCheckInput,
    Randomizer_CardPrompt,       Randomizer_CardInput,
};

RandomizerPickHooks* Randomizer_PickEntry(void) {
    // The generator's tables, in randomizer_core, loaded just before
    Randomizer_UnpackData();

    // Fragments aren't cleared when they're loaded
    Randomizer_PanelReset(Randomizer_PickRedraw);
    Randomizer_TeamReset();
    Randomizer_EditorReset();

    // A new run: random opponents get a new seed, unless Z makes a team, whose seed
    // they then share (Randomizer_FillTeam)
    Randomizer_State()->opponentSeed = osGetCount();
    return &sHooks;
}

// Has the pick screen redraw everything (func_84202718), where a panel was
static void Randomizer_PickRedraw(void) {
    D_84210D40 = 2;
}

// At the end of each frame's drawing: the options panel, the team's moves when they're
// shown, the control hints, and the teambuilder or its line in the team's menu
static void Randomizer_PickDraw(void) {
    // "Edit" picked in the rental card opens the teambuilder once the Pokemon is in
    Randomizer_EditorAfterCard();

    // The teambuilder covers the rest
    if (!Randomizer_EditorIsOpen()) {
        Randomizer_PanelDraw();
        Randomizer_TeamDraw();
        if (!Randomizer_EditorMenuCoversHints()) {
            Randomizer_HintsDraw();
        }
    }
    Randomizer_EditorDraw();
}

// func_8420AA08, plus the randomizer's buttons
void Randomizer_ListInput(unk_D_842168A0* arg0) {
    Controller* cont = &gControllers[arg0->unk_00003];

    // The teambuilder opened from the rental card takes the input
    if (Randomizer_EditorTakesListInput()) {
        arg0->unk_00008 = 8;
        arg0->unk_00007 = 0;
    } else if (Randomizer_PanelInput(cont)) {
        arg0->unk_00008 = 8;
        arg0->unk_00007 = 0;
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        func_8420A288(arg0);
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        func_8420A938(arg0);
    } else if (BTN_IS_PRESSED(cont, BTN_START)) {
        func_8420A9B8(arg0);
    } else if (BTN_IS_PRESSED(cont, BTN_R)) {
        func_8420A678(arg0);
    } else if (BTN_IS_PRESSED(cont, BTN_L)) {
        func_8420A7D8(arg0);
    } else if (BTN_IS_PRESSED(cont, BTN_Z)) {
        Randomizer_FillTeam(arg0);
    } else if (BTN_IS_DOWN(cont, BTN_DLEFT)) {
        func_8420A594(arg0);
    } else if (BTN_IS_DOWN(cont, BTN_DRIGHT)) {
        func_8420A604(arg0);
    } else if (BTN_IS_DOWN(cont, BTN_DUP)) {
        func_8420A3E4(arg0);
    } else if (BTN_IS_DOWN(cont, BTN_DDOWN)) {
        func_8420A4B8(arg0);
    } else {
        arg0->unk_00008 = 8;
        arg0->unk_00007 = 0;
    }

    arg0->unk_00014 = arg0->unk_00010;
    arg0->unk_00016 = arg0->unk_00012;
    arg0->unk_00009 = 2;
}

#endif

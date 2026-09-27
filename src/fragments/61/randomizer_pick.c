/*
 * Entry point of the randomizer's pick-screen fragment (randomizer_pick, see
 * randomizer.h), which the pick screen loads when it starts. Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * It also holds the rental list's input handler, moved here from fragment61
 * (func_8420AA08) with the randomizer's buttons added: C-Up for the options panel and
 * Z for a random team. C-Down, held anywhere on the screen, shows the team's moves
 * (randomizer_team.c). This fragment is loaded after fragment61, so its references to
 * fragment61's functions and data are relocated to where fragment61 is.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/controller.h"

static void Randomizer_PickDraw(void);

static RandomizerPickHooks sHooks = {
    Randomizer_ListInput,
    Randomizer_PickDraw,
};

RandomizerPickHooks* Randomizer_PickEntry(void) {
    // Fragments aren't cleared when they're loaded
    Randomizer_PanelReset();
    Randomizer_TeamReset();
    return &sHooks;
}

// At the end of each frame's drawing: the options panel, or the team's moves while
// C-Down is held
static void Randomizer_PickDraw(void) {
    Randomizer_PanelDraw();
    Randomizer_TeamDraw();
}

// func_8420AA08, plus the randomizer's buttons
void Randomizer_ListInput(unk_D_842168A0* arg0) {
    Controller* cont = &gControllers[arg0->unk_00003];

    if (Randomizer_PanelInput(cont)) {
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

/*
 * In-game team randomizer for the Pokemon pick screen. Built only with RANDOMIZER=1;
 * the file is empty otherwise so the default build still matches the original ROM.
 *
 * Pressing Z while the rental list is active fills all six entry slots with a team
 * made the way the random team generator website makes one (randomizer_logic.c), then
 * hands over to the game's own "team complete" step, as if the player had picked the
 * last Pokemon by hand: the cup's level-sum check and the confirmation menu run as usual.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "randomizer_build.h"
#include "src/1AB70.h"
#include "src/22630.h"
#include "src/232C0.h"
#include "src/49790.h"
#include "src/hal_libc.h"

#define RENTAL_LIST 0xD
#define NOT_A_RENTAL 0xFF

// A team that breaks the cup's level-sum rule is thrown away and another one rolled
#define MAX_ATTEMPTS 20

// Index of the species in the rental list, so the list marks it as picked
static s32 Randomizer_RentalIndex(unk_D_842168A0_0013C* rentals, s32 species) {
    u32 i;

    for (i = 0; i < rentals->unk_00; i++) {
        if (rentals->unk_04[i].unk_00.unk_00 == species) {
            return i;
        }
    }
    return NOT_A_RENTAL;
}

/*
 * Called from the rental list's input handler (Randomizer_ListInput). Returns 1 if the team
 * was filled.
 */
s32 Randomizer_FillTeam(unk_D_842168A0* list) {
    unk_D_84211B50* team = list->unk_13608;
    unk_D_842168A0_0013C* rentals = list->unk_0013C;
    RandomizerState* state = Randomizer_State();
    RandomizerMon mons[RANDOMIZER_TEAM_SIZE];
    RandomizerRules rules;
    s32 attempt;
    s32 i;

    // Only while the team panel is waiting for a pick (the state func_84207BD4 needs),
    // and only in modes with rentals, which the new Pokemon borrow a trainer from.
    if ((team->unk_0001 != 3) || (rentals == NULL) || (rentals->unk_00 == 0)) {
        func_80048B90(8);
        return 0;
    }

    // Modes that allow any level use the level of their own rentals
    Randomizer_GetRules(D_800AE540.unk_0001, rentals->unk_04[0].unk_24, &rules);
    // The "Stadium" options take the moves and stats of the rentals listed here
    Randomizer_UseRentals(&rules, (RandomizerRentalList*)rentals);

    if (state->useEnteredSeed) {
        state->lastSeed = state->enteredSeed;
        state->useEnteredSeed = 0;
    } else {
        state->lastSeed = osGetCount();
    }
    Randomizer_Seed(state->lastSeed);
    // The trainers faced with this team get theirs from its seed too, so a shared seed
    // gives the same whole run
    state->opponentSeed = state->lastSeed;
    for (attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        if (!Randomizer_GenerateTeam(&state->settings, &rules, mons)) {
            func_80048B90(8);
            return 0;
        }
        if (Randomizer_TeamFitsLevelSum(&rules, mons)) {
            break;
        }
    }

    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        unk_D_838067F0_0168_0000* slot = &team->unk_0030[i];
        s32 index = Randomizer_RentalIndex(rentals, mons[i].species);

        // The website can go over the cup's top level when its filters leave too few
        // Pokemon; the game wouldn't accept that
        if ((rules.levelMax != 0) && (mons[i].level > rules.levelMax)) {
            mons[i].level = rules.levelMax;
        }

        Randomizer_BuildPokemon(&slot->unk_004, &mons[i], &rentals->unk_04[0]);
        func_8001B0DC(slot->unk_058, 0, &slot->unk_004);

        // How func_84209DB8 marks a rental the player picks by hand
        slot->unk_000 = list->unk_00004;
        slot->unk_001 = RENTAL_LIST;
        slot->unk_002 = index;
        slot->unk_003 = mons[i].species;
        slot->unk_004.unk_53 = index;
        slot->unk_004.unk_52 = (list->unk_00004 * 0x10) | RENTAL_LIST;
    }

    // What func_84207BD4 does when the last slot is filled
    team->unk_0006 = RANDOMIZER_TEAM_SIZE;
    team->unk_0008 = RANDOMIZER_TEAM_SIZE;
    team->unk_0010 = (RANDOMIZER_TEAM_SIZE - 1) % 3;
    team->unk_0012 = (RANDOMIZER_TEAM_SIZE - 1) / 3;
    team->unk_0003 = 2;
    team->unk_0004 = 0;
    if (func_84206A68(team) != 0) {
        team->unk_0001 = 15;
    } else if (team->unk_0000 == 0) {
        team->unk_0001 = 7;
    } else {
        team->unk_0001 = 14;
    }

    // And what the list does after handing a pick to the team (func_8420A0E4)
    list->unk_00009 = 2;
    list->unk_00001 = 4;

    func_80048B90(2);
    return 1;
}

#endif

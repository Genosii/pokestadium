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

#include "randomizer_logic.h"
#include "src/1AB70.h"
#include "src/22630.h"
#include "src/232C0.h"
#include "src/49790.h"
#include "src/hal_libc.h"

#define RENTAL_LIST 0xD
#define NOT_A_RENTAL 0xFF
#define NICKNAME_LENGTH 10
#define MAX_PP_UPS (3 << 6)

// A team that breaks the cup's level-sum rule is thrown away and another one rolled
#define MAX_ATTEMPTS 20

// The website's defaults
static const RandomizerSettings sDefaultSettings = { RANDOMIZER_MOVESET_LEGAL, 0, 0, 0, 0, 0, 0, 0 };

// The settings, set up with the defaults if they aren't there yet (see randomizer_state.h)
RandomizerState* Randomizer_State(void) {
    if (!RANDOMIZER_STATE_VALID()) {
        bzero(&gRandomizerState, sizeof(gRandomizerState));
        gRandomizerState.settings = sDefaultSettings;
        gRandomizerState.magic = RANDOMIZER_STATE_MAGIC;
    }
    return &gRandomizerState;
}

/*
 * The rules for the current mode, from the rule set index the mode chose
 * (D_800AE540.unk_0001). Level ranges and caps are the ones the pick screen checks
 * itself (func_8420ACA8, func_84206A68); pools and levels follow the website's cups.
 */
static void Randomizer_GetRules(unk_D_842168A0_0013C* rentals, RandomizerRules* rules) {
    rules->level = 0;
    rules->levelMin = 0;
    rules->levelMax = 0;
    rules->levelSum = 0;

    switch (D_800AE540.unk_0001) {
        case 3: // Poke Cup
            rules->cup = RANDOMIZER_CUP_POKE;
            rules->levelMin = 50;
            rules->levelMax = 55;
            rules->levelSum = 155;
            break;

        case 1:
        case 2: // the other level 50-55 modes, which have no level-sum rule
            rules->cup = RANDOMIZER_CUP_POKE;
            rules->levelMin = 50;
            rules->levelMax = 55;
            break;

        case 4: // Petit Cup
            rules->cup = RANDOMIZER_CUP_PETIT;
            rules->levelMin = 25;
            rules->levelMax = 30;
            rules->levelSum = 80;
            break;

        case 5: // Pika Cup
            rules->cup = RANDOMIZER_CUP_PIKA;
            rules->levelMin = 15;
            rules->levelMax = 20;
            rules->levelSum = 50;
            break;

        case 6: // Prime Cup
            rules->cup = RANDOMIZER_CUP_PRIME;
            rules->level = 100;
            break;

        default: // modes that allow any level: use the level of the mode's own rentals
            rules->cup = RANDOMIZER_CUP_PRIME;
            rules->level = rentals->unk_04[0].unk_24;
            break;
    }
}

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
 * Builds a Pokemon from scratch: species data from the game's own tables, experience
 * for the level, and PP Ups on every move; func_80022734 then works out the level,
 * stats, HP and PP. The trainer name and ID are taken from the mode's rentals.
 */
static void Randomizer_BuildPokemon(unk_func_80026268_arg0* mon, const RandomizerMon* src,
                                    unk_func_80026268_arg0* rental) {
    char name[0x20];
    s32 i;

    bzero(mon, sizeof(*mon));

    mon->unk_00.unk_00 = src->species;
    // Accessed through D_80070F84 the way CalculateStatValue does, the species' types
    // follow its base stats (Bulbasaur: grass, poison)
    mon->unk_06 = D_80070F84[src->species].unk0B[0];
    mon->unk_07 = D_80070F84[src->species].unk0B[1];
    for (i = 0; i < 4; i++) {
        mon->unk_09[i] = src->moves[i];
        mon->unk_20[i] = (src->moves[i] != 0) ? MAX_PP_UPS : 0;
    }
    mon->unk_0E = rental->unk_0E;
    mon->unk_10 = func_800224B8(src->species, src->level);
    mon->unk_14 = src->statExp[0];
    mon->unk_16 = src->statExp[1];
    mon->unk_18 = src->statExp[2];
    mon->unk_1A = src->statExp[3];
    mon->unk_1C = src->statExp[4];
    mon->unk_1E = (src->dvs[0] << 12) | (src->dvs[1] << 8) | (src->dvs[2] << 4) | src->dvs[3];
    func_80022734(mon);

    func_80021CA4(name, src->species);
    for (i = 0; (i < NICKNAME_LENGTH) && (name[i] != '\0'); i++) {
        mon->unk_30[i] = name[i];
    }
    _bcopy(rental->unk_3B, mon->unk_3B, sizeof(mon->unk_3B));
    func_800228B0(mon);
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

    Randomizer_GetRules(rentals, &rules);

    if (state->useEnteredSeed) {
        state->lastSeed = state->enteredSeed;
        state->useEnteredSeed = 0;
    } else {
        state->lastSeed = osGetCount();
    }
    Randomizer_Seed(state->lastSeed);
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

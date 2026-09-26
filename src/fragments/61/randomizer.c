/*
 * In-game team randomizer for the Pokemon pick screen. Built only with RANDOMIZER=1;
 * the file is empty otherwise so the default build still matches the original ROM.
 *
 * Pressing Z while the rental list is active fills all six entry slots at once and
 * hands over to the game's own "team complete" step, as if the player had picked
 * the last Pokemon by hand: the cup's level-sum check and the confirmation menu run
 * as usual.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/1AB70.h"
#include "src/232C0.h"
#include "src/49790.h"
#include "src/hal_libc.h"

#define TEAM_SIZE 6
#define RENTAL_LIST 0xD

static u32 sRandomizerState;

// The website's seeded rng(): a 32-bit LCG with the Numerical Recipes constants.
static u32 Randomizer_Next(void) {
    sRandomizerState = (sRandomizerState * 0x19660D) + 0x3C6EF35F;
    return sRandomizerState;
}

// Math.floor(rng() * n) on the website, without floats.
static s32 Randomizer_Below(s32 n) {
    return ((u64)Randomizer_Next() * (u32)n) >> 32;
}

/*
 * Picks TEAM_SIZE rentals with different species and writes them to the team slots,
 * set up the way func_84209DB8 sets up a rental the player picks by hand. The slots
 * are only touched once a full team has been found.
 */
static s32 Randomizer_PickRentals(unk_D_842168A0* list, unk_D_84211B50* team) {
    unk_D_842168A0_0013C* rentals = list->unk_0013C;
    s32 picks[TEAM_SIZE];
    s32 count = 0;
    s32 tries;
    s32 i;

    for (tries = 0; (count < TEAM_SIZE) && (tries < 1000); tries++) {
        s32 index = Randomizer_Below(rentals->unk_00);

        for (i = 0; i < count; i++) {
            if (rentals->unk_04[picks[i]].unk_00.unk_00 == rentals->unk_04[index].unk_00.unk_00) {
                break;
            }
        }
        if (i == count) {
            picks[count++] = index;
        }
    }

    if (count < TEAM_SIZE) {
        return 0;
    }

    for (i = 0; i < TEAM_SIZE; i++) {
        unk_D_838067F0_0168_0000* slot = &team->unk_0030[i];

        _bcopy(&rentals->unk_04[picks[i]], &slot->unk_004, sizeof(slot->unk_004));
        func_800228B0(&slot->unk_004);
        func_8001B0DC(slot->unk_058, 0, &slot->unk_004);

        slot->unk_000 = list->unk_00004;
        slot->unk_001 = RENTAL_LIST;
        slot->unk_002 = picks[i];
        slot->unk_003 = slot->unk_004.unk_00.unk_00;
        slot->unk_004.unk_53 = picks[i];
        slot->unk_004.unk_52 = (list->unk_00004 * 0x10) | RENTAL_LIST;
    }
    return 1;
}

/*
 * Called from the rental list's input handler (func_8420AA08). Returns 1 if the team
 * was filled.
 */
s32 Randomizer_FillTeam(unk_D_842168A0* list) {
    unk_D_84211B50* team = list->unk_13608;

    // Only while the team panel is waiting for a pick (the state func_84207BD4 needs),
    // and only for cups that have rentals.
    if ((team->unk_0001 != 3) || (list->unk_0013C == NULL) || (list->unk_0013C->unk_00 < TEAM_SIZE)) {
        func_80048B90(8);
        return 0;
    }

    sRandomizerState = osGetCount();

    if (!Randomizer_PickRentals(list, team)) {
        func_80048B90(8);
        return 0;
    }

    // What func_84207BD4 does when the last slot is filled.
    team->unk_0006 = TEAM_SIZE;
    team->unk_0008 = TEAM_SIZE;
    team->unk_0010 = (TEAM_SIZE - 1) % 3;
    team->unk_0012 = (TEAM_SIZE - 1) / 3;
    team->unk_0003 = 2;
    team->unk_0004 = 0;
    if (func_84206A68(team) != 0) {
        team->unk_0001 = 15;
    } else if (team->unk_0000 == 0) {
        team->unk_0001 = 7;
    } else {
        team->unk_0001 = 14;
    }

    // And what the list does after handing a pick to the team (func_8420A0E4).
    list->unk_00009 = 2;
    list->unk_00001 = 4;

    func_80048B90(2);
    return 1;
}

#endif

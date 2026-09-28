/*
 * Random battle team for the in-game randomizer, on the screen where each player picks
 * which of their Pokemon go into the next battle. This is the randomizer's battle-select
 * fragment (randomizer_battle, see randomizer_battle.h), which the screen loads when it
 * starts. Built only with RANDOMIZER=1; the file is empty otherwise so the default build
 * still matches the original ROM.
 *
 * Z (or the auto option, once per visit to the screen) clears the player's picks and
 * picks at random instead, through the same function a button press uses
 * (func_84802614), so the game's own checks and "Is this OK?" prompt still apply. In
 * cups with a level-sum rule only trios that fit are considered, as the random team
 * generator website's generateBattleTeam() does.
 *
 * It also holds a player's picking input handler, moved here from fragment64
 * (func_848027F0) with the randomizer added. This fragment is loaded after fragment64,
 * so its references to fragment64's functions are relocated to where fragment64 is.
 */
#include "randomizer_battle.h"

#ifdef RANDOMIZER

#include "src/49790.h"
#include "src/controller.h"
#include "src/randomizer_state.h"

#define MAX_TEAM 6
#define MAX_TRIOS 20 // 6 choose 3

// Set when a CPU player picks, through a simulated controller
#define PLAYER_IS_CPU(player) ((player)->unk_001C->unk_000 & 2)

static u32 sRandomState;
static u8 sAutoPicked[4];

static void Randomizer_BattlePick(unk_D_848037A0* player);
static s32 Randomizer_AutoBattlePick(unk_D_848037A0* player);

static RandomizerBattleHooks sHooks = {
    Randomizer_PickInput,
    Randomizer_DrawFooter,
};

// Loaded again each time the screen opens, so this is once per visit
RandomizerBattleHooks* Randomizer_BattleEntry(void) {
    bzero(sAutoPicked, sizeof(sAutoPicked));
    return &sHooks;
}

// The website's rng(): the same LCG as the team generator
static s32 Randomizer_BattleBelow(s32 n) {
    sRandomState = (sRandomState * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandomState * (u32)n) >> 32;
}

// The cap on the sum of three levels, as func_8480247C checks it (0 = none)
static s32 Randomizer_LevelSumCap(void) {
    switch (D_800AE540.unk_0001) {
        case 3: // Poke Cup
            return 155;
        case 4: // Petit Cup
            return 80;
        case 5: // Pika Cup
            return 50;
        default:
            return 0;
    }
}

// How many Pokemon the player has to pick, as func_848025A0 counts it
static s32 Randomizer_PicksNeeded(unk_D_848037A0* player) {
    if ((D_800AE540.unk_0001 != 0) && (D_800AE540.unk_0001 != 8)) {
        return 3;
    }
    if ((player->unk_0002 != -1) && (player->unk_0017 >= 4)) {
        return 3;
    }
    return player->unk_0017;
}

static void Randomizer_BattlePick(unk_D_848037A0* player) {
    s8 picks[MAX_TEAM];
    s32 count = player->unk_0017;
    s32 needed = Randomizer_PicksNeeded(player);
    s32 cap = Randomizer_LevelSumCap();
    s32 i;
    s32 j;
    s32 k;

    if (PLAYER_IS_CPU(player) || (count > MAX_TEAM) || (needed > count) || (needed <= 0)) {
        func_80048B90(8);
        return;
    }

    sRandomState = osGetCount();

    if ((needed == 3) && (cap != 0)) {
        // One of the trios that fit, all equally likely
        s8 trios[MAX_TRIOS][3];
        s32 numTrios = 0;
        s32 pick;

        for (i = 0; i < count - 2; i++) {
            for (j = i + 1; j < count - 1; j++) {
                for (k = j + 1; k < count; k++) {
                    if (player->unk_0018[i].unk_24 + player->unk_0018[j].unk_24 + player->unk_0018[k].unk_24 <= cap) {
                        trios[numTrios][0] = i;
                        trios[numTrios][1] = j;
                        trios[numTrios][2] = k;
                        numTrios++;
                    }
                }
            }
        }
        if (numTrios == 0) {
            func_80048B90(8);
            return;
        }
        pick = Randomizer_BattleBelow(numTrios);
        for (i = 0; i < 3; i++) {
            picks[i] = trios[pick][i];
        }
    } else {
        // randomSample(team, needed)
        for (i = 0; i < count; i++) {
            picks[i] = i;
        }
        for (i = count - 1; i > 0; i--) {
            s8 tmp;

            j = Randomizer_BattleBelow(i + 1);
            tmp = picks[i];
            picks[i] = picks[j];
            picks[j] = tmp;
        }
    }

    func_84802740(player);
    for (i = 0; i < needed; i++) {
        func_84802614(player, picks[i], 0);
    }
}

// Called while the player is picking; does the auto pick once per visit to the screen.
// Returns 1 if it picked, so the frame's button presses aren't applied on top.
static s32 Randomizer_AutoBattlePick(unk_D_848037A0* player) {
    if (!RANDOMIZER_STATE_VALID() || !gRandomizerState.autoBattlePick || PLAYER_IS_CPU(player)) {
        return 0;
    }
    if (sAutoPicked[player->unk_0000 & 3] || (player->unk_000B != 0)) {
        return 0;
    }
    sAutoPicked[player->unk_0000 & 3] = 1;
    Randomizer_BattlePick(player);
    return 1;
}

// func_848027F0, plus the randomizer's button and auto pick
void Randomizer_PickInput(Controller* arg0, unk_D_848037A0* arg1) {
    s32 temp_a2 = BTN_IS_DOWN(arg0, BTN_R);

    if (Randomizer_AutoBattlePick(arg1)) {
        return;
    }

    if (BTN_IS_PRESSED(arg0, BTN_R)) {
        arg1->unk_0009 = 1;
        arg1->unk_0006 = 2;
    }

    if (arg0->unk_0A & 0x10) {
        arg1->unk_0009 = 0;
        arg1->unk_0006 = 2;
    }

    if (BTN_IS_PRESSED(arg0, BTN_L)) {
        func_84802740(arg1);
    } else if (BTN_IS_PRESSED(arg0, BTN_B)) {
        func_84802614(arg1, 0, temp_a2);
    } else if (BTN_IS_PRESSED(arg0, BTN_CLEFT)) {
        func_84802614(arg1, 1, temp_a2);
    } else if (BTN_IS_PRESSED(arg0, BTN_CUP)) {
        func_84802614(arg1, 2, temp_a2);
    } else if (BTN_IS_PRESSED(arg0, BTN_A)) {
        func_84802614(arg1, 3, temp_a2);
    } else if (BTN_IS_PRESSED(arg0, BTN_CDOWN)) {
        func_84802614(arg1, 4, temp_a2);
    } else if (BTN_IS_PRESSED(arg0, BTN_CRIGHT)) {
        func_84802614(arg1, 5, temp_a2);
    } else if (BTN_IS_PRESSED(arg0, BTN_Z)) {
        Randomizer_BattlePick(arg1);
    }
}

#endif

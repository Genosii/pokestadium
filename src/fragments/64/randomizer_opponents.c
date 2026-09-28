/*
 * Random opponents for the in-game randomizer: with "Random opponents" on (the pick
 * screen's C-Right panel), the computer trainer's team is replaced with one the random
 * team generator makes with the opponents' options, as the battle-select screen starts.
 * Part of the randomizer's battle-select fragment (randomizer_battle.h). Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * Only the trainers the game loads from its trainer data are changed (modes 1 to 8:
 * the cups, the Gym Leader Castle and the like, see func_8002C128), not a computer team
 * the player picked themselves in Free Battle. Each trainer's team comes from the run's
 * opponent seed and where the trainer stands in the run (mode, ball or gym, round), so
 * a trainer faced again after a loss has the same team, and a shared seed gives the
 * same trainers. The trainer's own levels stay, slot by slot, so the cup's level rules
 * and the game's difficulty hold, and so do its name and ID.
 */
#include "randomizer_battle.h"

#ifdef RANDOMIZER

#include "src/fragments/61/randomizer_build.h"
#include "src/randomizer_state.h"

// The modes whose computer trainers come from the trainer data (func_8002C128)
#define FIRST_TRAINER_MODE 1
#define LAST_TRAINER_MODE 8

#define IS_COMPUTER(trainer) ((trainer)->unk_000 & 2)

// A seed for one trainer: the run's seed mixed with the trainer's place in the run
static u32 Randomizer_TrainerSeed(u32 runSeed, s32 side, s32 index) {
    u32 x = (D_800AE540.unk_0000 << 24) | ((D_800AE540.unk_0002 & 0xFF) << 16) | ((D_800AE540.unk_0003 & 0xFF) << 8) |
            (side << 4) | index;

    // A few rounds of a 32-bit finalizer, so nearby places give unrelated seeds
    x = runSeed ^ (x * 0x9E3779B9);
    x ^= x >> 16;
    x *= 0x85EBCA6B;
    x ^= x >> 13;
    x *= 0xC2B2AE35;
    x ^= x >> 16;
    return x;
}

static void Randomizer_RandomizeTrainer(unk_D_800AE540_0004* trainer, u32 seed) {
    RandomizerState* state = &gRandomizerState;
    RandomizerMon mons[RANDOMIZER_TEAM_SIZE];
    RandomizerRules rules;
    unk_func_80026268_arg0 original;
    s32 count = trainer->unk_002;
    s32 i;

    if ((count <= 0) || (count > RANDOMIZER_TEAM_SIZE)) {
        return;
    }

    Randomizer_GetRules(D_800AE540.unk_0001, trainer->unk_01C[0].unk_24, &rules);
    Randomizer_Seed(seed);
    if (!Randomizer_GenerateTeam(&state->opponentSettings, &rules, mons)) {
        return; // the options leave too few Pokemon: the trainer keeps its team
    }

    for (i = 0; i < count; i++) {
        // Built over the trainer's own Pokemon, so keep what's taken from it
        original = trainer->unk_01C[i];
        mons[i].level = original.unk_24;
        Randomizer_BuildPokemon(&trainer->unk_01C[i], &mons[i], &original);
        // The copy the battle-select screen reads and battles start from (func_8002B888)
        trainer->unk_214->unk_028[i] = trainer->unk_01C[i];
    }
}

// Called as the battle-select screen starts, before it reads the teams
void Randomizer_RandomizeOpponents(void) {
    s32 side;
    s32 i;

    if (!RANDOMIZER_STATE_VALID() || !gRandomizerState.randomOpponents || (D_800AE540.unk_0000 < FIRST_TRAINER_MODE) ||
        (D_800AE540.unk_0000 > LAST_TRAINER_MODE)) {
        return;
    }

    for (side = 0; side < 2; side++) {
        unk_D_800AE540_1194* s = &D_800AE540.unk_1194[side];

        for (i = 0; (i < s->unk_01) && (i < 2); i++) {
            unk_D_800AE540_0004* trainer = s->unk_08[i];

            if ((trainer != NULL) && IS_COMPUTER(trainer) && (trainer->unk_214 != NULL)) {
                Randomizer_RandomizeTrainer(trainer, Randomizer_TrainerSeed(gRandomizerState.opponentSeed, side, i));
            }
        }
    }
}

#endif

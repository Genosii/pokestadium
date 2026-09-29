/*
 * Random opponents for the in-game randomizer: with "Random opponents" on (the pick
 * screen's C-Right panel), or in Factory or Rogue mode, the computer trainer's team is
 * replaced with one the random team generator makes with the opponents' options, as the
 * battle-select screen starts. The "Stadium" moveset gives them the moves of the mode's
 * rental Pokemon, which this loads for the purpose.
 * Part of the randomizer's battle-select fragment (randomizer_battle.h). Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * Only the trainers the game loads from its trainer data are changed (modes 1 to 8:
 * the cups, the Gym Leader Castle and the like, see func_8002C128), not a computer team
 * the player picked themselves in Free Battle. Each trainer's team comes from the run's
 * opponent seed and where the trainer stands in the run (mode, ball or gym, round), so
 * a trainer faced again after a loss has the same team, and a shared seed gives the
 * same trainers. The trainer's own levels stay, slot by slot, and so do its DVs and stat
 * exp unless the options make them random, so the cup's level rules and the game's
 * difficulty hold, and so do its name and ID.
 */
#include "randomizer_battle.h"

#ifdef RANDOMIZER

#include "src/3FB0.h"
#include "src/fragments/61/randomizer_build.h"
#include "src/memory.h"
#include "src/randomizer_state.h"

// The trainer archive, which also holds the rental Pokemon (func_8002C128, func_84203C90)
#define TRAINER_ARCHIVE ((u8*)0x898000)

// The modes whose computer trainers come from the trainer data (func_8002C128)
#define FIRST_TRAINER_MODE 1
#define LAST_TRAINER_MODE 8

#define IS_COMPUTER(trainer) ((trainer)->unk_000 & 2)

// The Gym Leader Castle's mode, its gyms (D_800AE540.unk_0002) and round of the leader
#define CASTLE_MODE 7
#define CASTLE_GYMS 8
#define CASTLE_ELITE_FOUR 8
#define CASTLE_LEADER_ROUND 4

// Gen 1's type ids, plus one as RandomizerRules.theme has them
#define THEME(type) ((type) + 1)
#define NORMAL THEME(0)
#define FIGHTING THEME(1)
#define FLYING THEME(2)
#define POISON THEME(3)
#define GROUND THEME(4)
#define ROCK THEME(5)
#define BUG THEME(7)
#define GHOST THEME(8)
#define FIRE THEME(20)
#define WATER THEME(21)
#define GRASS THEME(22)
#define ELECTRIC THEME(23)
#define PSYCHIC THEME(24)
#define ICE THEME(25)
#define DRAGON THEME(26)

/*
 * Rogue's themes: each gym's leader, in the castle's order, then the Elite Four in
 * theirs. The second type makes up six where the first has too few (Gen 1 has three
 * Ghost and three Dragon Pokemon, five Ice, and the pool options can leave fewer). It's
 * the type of the trainer's other Pokemon in the game where there are any (Blaine's
 * Clefable and Kangaskhan, Lance's Gyarados and Aerodactyl, ...).
 */
static const u8 sLeaderThemes[CASTLE_GYMS][2] = {
    { ROCK, GROUND },     // Brock
    { WATER, ICE },       // Misty
    { ELECTRIC, NORMAL }, // Lt. Surge
    { GRASS, BUG },       // Erika
    { POISON, BUG },      // Koga
    { PSYCHIC, ICE },     // Sabrina
    { FIRE, NORMAL },     // Blaine
    { GROUND, NORMAL },   // Giovanni
};
static const u8 sEliteFourThemes[4][2] = {
    { ICE, WATER },     // Lorelei
    { FIGHTING, ROCK }, // Bruno
    { GHOST, POISON },  // Agatha
    { DRAGON, FLYING }, // Lance
};

// Rogue gives the castle's leaders and the Elite Four teams of their type
static void Randomizer_ThemeRules(RandomizerRules* rules) {
    const u8* theme = NULL;
    s32 gym = D_800AE540.unk_0002;
    s32 round = D_800AE540.unk_0003;

    if ((gRandomizerState.mode != RANDOMIZER_MODE_ROGUE) || (D_800AE540.unk_0000 != CASTLE_MODE)) {
        return;
    }
    if ((gym >= 0) && (gym < CASTLE_GYMS) && (round == CASTLE_LEADER_ROUND)) {
        theme = sLeaderThemes[gym];
    } else if ((gym == CASTLE_ELITE_FOUR) && (round >= 1) && (round <= ARRAY_COUNT(sEliteFourThemes))) {
        theme = sEliteFourThemes[round - 1];
    }
    if (theme != NULL) {
        rules->theme = theme[0];
        rules->theme2 = theme[1];
    }
}

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

static void Randomizer_RandomizeTrainer(unk_D_800AE540_0004* trainer, u32 seed, const RandomizerRentalList* rentals) {
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
    Randomizer_ThemeRules(&rules);
    Randomizer_UseRentals(&rules, rentals);
    Randomizer_Seed(seed);
    if (!Randomizer_GenerateTeam(&state->opponentSettings, &rules, mons)) {
        return; // the options leave too few Pokemon: the trainer keeps its team
    }

    for (i = 0; i < count; i++) {
        // Built over the trainer's own Pokemon, so keep what's taken from it
        original = trainer->unk_01C[i];
        mons[i].level = original.unk_24;
        // "Stadium" DVs and stat exp are the trainer's own: the game's difficulty is mostly
        // in these, which grow from ball to ball and gym to gym (6000 stat exp in the Poke
        // Cup's Poke Ball, 25600 in the Castle)
        if (state->opponentSettings.dvs == RANDOMIZER_STATS_STADIUM) {
            mons[i].dvs[0] = (original.unk_1E >> 12) & 0xF;
            mons[i].dvs[1] = (original.unk_1E >> 8) & 0xF;
            mons[i].dvs[2] = (original.unk_1E >> 4) & 0xF;
            mons[i].dvs[3] = original.unk_1E & 0xF;
        }
        if (state->opponentSettings.statExp == RANDOMIZER_STATS_STADIUM) {
            mons[i].statExp[0] = original.unk_14;
            mons[i].statExp[1] = original.unk_16;
            mons[i].statExp[2] = original.unk_18;
            mons[i].statExp[3] = original.unk_1A;
            mons[i].statExp[4] = original.unk_1C;
        }
        Randomizer_BuildPokemon(&trainer->unk_01C[i], &mons[i], &original);
        // The copy the battle-select screen reads and battles start from (func_8002B888)
        trainer->unk_214->unk_028[i] = trainer->unk_01C[i];
    }
}

// Called as the battle-select screen starts, before it reads the teams
void Randomizer_RandomizeOpponents(void) {
    const RandomizerRentalList* rentals = NULL;
    s32 table = Randomizer_RentalTable();
    s32 side;
    s32 i;

    // Factory and Rogue always have them
    if (!RANDOMIZER_STATE_VALID() ||
        (!gRandomizerState.randomOpponents && (gRandomizerState.mode == RANDOMIZER_MODE_NORMAL)) ||
        (D_800AE540.unk_0000 < FIRST_TRAINER_MODE) || (D_800AE540.unk_0000 > LAST_TRAINER_MODE)) {
        return;
    }

    // The mode's rental Pokemon, for the "Stadium" moveset: loaded for this only, as
    // func_8002C128 loads the trainers
    main_pool_push_state('RNDO');
    if (table >= 0) {
        rentals = func_8000484C(func_800044F4(TRAINER_ARCHIVE, NULL, 1, 0), table);
    }

    for (side = 0; side < 2; side++) {
        unk_D_800AE540_1194* s = &D_800AE540.unk_1194[side];

        for (i = 0; (i < s->unk_01) && (i < 2); i++) {
            unk_D_800AE540_0004* trainer = s->unk_08[i];

            if ((trainer != NULL) && IS_COMPUTER(trainer) && (trainer->unk_214 != NULL)) {
                Randomizer_RandomizeTrainer(trainer, Randomizer_TrainerSeed(gRandomizerState.opponentSeed, side, i),
                                            rentals);
            }
        }
    }
    main_pool_pop_state('RNDO');
}

#endif

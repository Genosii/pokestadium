/*
 * Team generation for the in-game randomizer: a port of randomizeFullTeam() and its
 * helpers from the random team generator website (js/randomize.js), for the
 * Stadium 1 modes. Random numbers are drawn in the same order as on the website, so
 * the same seed and settings give the same team; tools/randomizer/parity checks that.
 *
 * Where the website's behaviour is noted in a comment, it's the function of the same
 * name in randomize.js.
 */
#include "randomizer_logic.h"

#ifdef RANDOMIZER

#define MAX_LEARNSET 80
#define MAX_SCRATCH 168 // at least RANDOMIZER_NUM_SPECIES and RANDOMIZER_NUM_CHAOS_MOVES

// Website type ids (Gen 1's internal ones): physical types are below 10, special ones 20 and up
#define TYPE_PHYSICAL_END 10
#define TYPE_SPECIAL_START 20
#define NO_TYPE 0xFF

static u32 sRandomState;

static u8 sPool[RANDOMIZER_NUM_SPECIES];
static u8 sScratch[MAX_SCRATCH];
static u8 sLegal[MAX_LEARNSET];
static u8 sCandidates[MAX_LEARNSET];
static u8 sStrong[MAX_LEARNSET];
static u8 sPicked[4];
static s32 sNumPicked;

void Randomizer_Seed(u32 seed) {
    sRandomState = seed;
}

// rng(): a 32-bit LCG with the Numerical Recipes constants, as a fraction of 2^32
static u32 Randomizer_Next(void) {
    sRandomState = (sRandomState * 0x19660D) + 0x3C6EF35F;
    return sRandomState;
}

// Math.floor(rng() * n)
static s32 Randomizer_Below(s32 n) {
    return ((u64)Randomizer_Next() * (u32)n) >> 32;
}

// The shuffle randomSample() does before taking the first n items
static void Randomizer_Shuffle(u8* items, s32 count) {
    s32 i;

    for (i = count - 1; i > 0; i--) {
        s32 j = Randomizer_Below(i + 1);
        u8 tmp = items[i];

        items[i] = items[j];
        items[j] = tmp;
    }
}

static void Randomizer_Copy(u8* dst, const u8* src, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

// randomSample(pool, n): a shuffle of the pool, first n
static void Randomizer_Sample(const u8* pool, s32 count, RandomizerMon team[RANDOMIZER_TEAM_SIZE]) {
    s32 i;

    Randomizer_Copy(sScratch, pool, count);
    Randomizer_Shuffle(sScratch, count);
    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        team[i].species = sScratch[i];
    }
}

// The website's SHARED_TYPE_ATTEMPTS
#define SHARED_TYPE_ATTEMPTS 10

// typesOf(): a bit for each of the species' types (type ids are under 32)
#define TYPE_BITS(species) ((1 << gRandomizerSpecies[species].type1) | (1 << gRandomizerSpecies[species].type2))

// sampleWithoutSharedTypes(pool, n): goes through a shuffle of the pool in order and skips
// any Pokemon that shares a type with one already picked; shuffles again if that runs out
// before n, and falls back to randomSample() if the pool can't do it
static void Randomizer_SampleWithoutSharedTypes(const u8* pool, s32 count, RandomizerMon team[RANDOMIZER_TEAM_SIZE]) {
    s32 attempt;
    s32 picked;
    s32 i;
    u32 used;

    for (attempt = 0; attempt < SHARED_TYPE_ATTEMPTS; attempt++) {
        Randomizer_Copy(sScratch, pool, count);
        Randomizer_Shuffle(sScratch, count);
        used = 0;
        picked = 0;
        for (i = 0; (i < count) && (picked < RANDOMIZER_TEAM_SIZE); i++) {
            if (!(TYPE_BITS(sScratch[i]) & used)) {
                used |= TYPE_BITS(sScratch[i]);
                team[picked++].species = sScratch[i];
            }
        }
        if (picked == RANDOMIZER_TEAM_SIZE) {
            return;
        }
    }
    Randomizer_Sample(pool, count, team);
}

/* Species pool */

// filterPokemonForMode()
static s32 Randomizer_InCupPool(s32 cup, s32 species) {
    switch (cup) {
        case RANDOMIZER_CUP_POKE:
            return species < 150; // no Mewtwo or Mew
        case RANDOMIZER_CUP_PETIT:
            return gRandomizerSpecies[species].flags & RANDOMIZER_SPECIES_PETIT_CUP;
        case RANDOMIZER_CUP_PIKA:
            return gRandomizerSpecies[species].flags & RANDOMIZER_SPECIES_PIKA_CUP;
        default:
            return 1;
    }
}

// CUP_MAX_LEVEL
static s32 Randomizer_CupMaxLevel(s32 cup) {
    switch (cup) {
        case RANDOMIZER_CUP_POKE:
            return 55;
        case RANDOMIZER_CUP_PETIT:
            return 30;
        case RANDOMIZER_CUP_PIKA:
            return 20;
        default:
            return 0;
    }
}

// applyPoolFilters(): drop what the settings exclude, unless that leaves fewer than six
static s32 Randomizer_FilterPool(const RandomizerSettings* settings, s32 cup, u8* pool, s32 count) {
    s32 maxLevel = Randomizer_CupMaxLevel(cup);
    s32 kept = 0;
    s32 i;

    for (i = 0; i < count; i++) {
        const RandomizerSpecies* sp = &gRandomizerSpecies[pool[i]];

        if ((maxLevel != 0) && (sp->minLevel > maxLevel)) {
            continue;
        }
        if (settings->noLegendaries && (sp->flags & RANDOMIZER_SPECIES_LEGENDARY)) {
            continue;
        }
        if (settings->finalEvosOnly && !(sp->flags & RANDOMIZER_SPECIES_FINAL_EVO)) {
            continue;
        }
        sScratch[kept++] = pool[i];
    }

    if (kept < RANDOMIZER_TEAM_SIZE) {
        return count;
    }
    Randomizer_Copy(pool, sScratch, kept);
    return kept;
}

static s32 Randomizer_HasType(s32 species, s32 type) {
    return (gRandomizerSpecies[species].type1 == type) || (gRandomizerSpecies[species].type2 == type);
}

// pickMonoType(): a type with at least six Pokemon in the pool, types in ascending id order
static s32 Randomizer_PickMonoType(u8* pool, s32 count) {
    u8 viable[32];
    s32 numViable = 0;
    s32 type;
    s32 kept;
    s32 i;

    for (type = 0; type < 32; type++) {
        s32 matches = 0;

        for (i = 0; i < count; i++) {
            matches += Randomizer_HasType(pool[i], type);
        }
        if (matches >= RANDOMIZER_TEAM_SIZE) {
            viable[numViable++] = type;
        }
    }

    if (numViable == 0) {
        return count;
    }

    type = viable[Randomizer_Below(numViable)];
    kept = 0;
    for (i = 0; i < count; i++) {
        if (Randomizer_HasType(pool[i], type)) {
            pool[kept++] = pool[i];
        }
    }
    return kept;
}

/*
 * A trainer's theme (RandomizerRules.theme; the website has no such thing): a shuffle of
 * the pool, its Pokemon of the theme's type first, then of the second type. Returns 0 if
 * the two make fewer than six.
 */
static s32 Randomizer_SampleTheme(const RandomizerRules* rules, const u8* pool, s32 count,
                                  RandomizerMon team[RANDOMIZER_TEAM_SIZE]) {
    s32 themes[2];
    s32 picked = 0;
    s32 t;
    s32 i;

    themes[0] = rules->theme - 1;
    themes[1] = rules->theme2 - 1;

    Randomizer_Copy(sScratch, pool, count);
    Randomizer_Shuffle(sScratch, count);
    for (t = 0; t < 2; t++) {
        if (themes[t] < 0) {
            continue;
        }
        for (i = 0; (i < count) && (picked < RANDOMIZER_TEAM_SIZE); i++) {
            if ((sScratch[i] != 0) && Randomizer_HasType(sScratch[i], themes[t])) {
                team[picked++].species = sScratch[i];
                sScratch[i] = 0; // taken
            }
        }
    }
    return picked == RANDOMIZER_TEAM_SIZE;
}

/* Movesets */

static const RandomizerMove* Randomizer_Move(s32 move) {
    return &gRandomizerMoves[move];
}

// rentalFor()
static s32 Randomizer_Rental(const RandomizerRules* rules, s32 species, RandomizerMon* rental) {
    return (rules->rental != 0) && rules->rental(species, rental);
}

// getMoveInheritance(): the learnset in website order, without tradeback-only moves if asked
static s32 Randomizer_LegalMoves(const RandomizerSettings* settings, s32 species) {
    const RandomizerSpecies* sp = &gRandomizerSpecies[species];
    s32 count = 0;
    s32 i;

    for (i = 0; i < sp->learnsetCount; i++) {
        const RandomizerLearnsetMove* entry = &gRandomizerLearnsets[sp->learnsetStart + i];

        if (!settings->noTradeback || entry->gen1) {
            sLegal[count++] = entry->move;
        }
    }
    return count;
}

static s32 Randomizer_IsPicked(s32 move) {
    s32 i;

    for (i = 0; i < sNumPicked; i++) {
        if (sPicked[i] == move) {
            return 1;
        }
    }
    return 0;
}

// pickFrom() in pickStrongMoves()
static void Randomizer_PickFrom(const u8* pool, s32 count) {
    sPicked[sNumPicked++] = pool[Randomizer_Below(count)];
}

// power x accuracy
static s32 Randomizer_Strength(s32 move) {
    return Randomizer_Move(move)->power * Randomizer_Move(move)->accuracy;
}

// isStrong()
static s32 Randomizer_IsStrong(s32 move) {
    return (Randomizer_Move(move)->power >= 70) && (Randomizer_Move(move)->accuracy >= 85);
}

// suitsStat(): of the attacking stat the species is better at
static s32 Randomizer_SuitsStat(s32 species, s32 move) {
    s32 type = Randomizer_Move(move)->type;

    return (gRandomizerSpecies[species].flags & RANDOMIZER_SPECIES_PHYSICAL) ? (type < TYPE_PHYSICAL_END)
                                                                             : (type >= TYPE_SPECIAL_START);
}

// In reliable: an attack that isn't on UNRELIABLE_MOVES
static s32 Randomizer_IsReliable(s32 move) {
    return (Randomizer_Move(move)->power > 0) && !(Randomizer_Move(move)->flags & RANDOMIZER_MOVE_UNRELIABLE);
}

static s32 Randomizer_TypePicked(s32 type) {
    s32 i;

    for (i = 0; i < sNumPicked; i++) {
        if (Randomizer_Move(sPicked[i])->type == type) {
            return 1;
        }
    }
    return 0;
}

// The tiers of coverage() in pickStrongMoves(), tried in order
enum {
    COVERAGE_STRONG_SUITED, // fresh.filter(m => isStrong(m) && suitsStat(m))
    COVERAGE_STRONG,        // fresh.filter(isStrong)
    COVERAGE_SUITED,        // fresh.filter(suitsStat)
    COVERAGE_FRESH,         // fresh: reliable, not picked, of a type not picked yet
    COVERAGE_ANY,           // reliable.filter(m => !picked.includes(m))
    COVERAGE_TIERS
};

// coverage(): a move from the first tier that has any; 0 if none has
static s32 Randomizer_Coverage(s32 species, s32 legalCount) {
    s32 tier;
    s32 count;
    s32 i;

    for (tier = 0; tier < COVERAGE_TIERS; tier++) {
        count = 0;
        for (i = 0; i < legalCount; i++) {
            s32 move = sLegal[i];

            if (!Randomizer_IsReliable(move) || Randomizer_IsPicked(move)) {
                continue;
            }
            if ((tier != COVERAGE_ANY) && Randomizer_TypePicked(Randomizer_Move(move)->type)) {
                continue;
            }
            if (((tier == COVERAGE_STRONG_SUITED) || (tier == COVERAGE_STRONG)) && !Randomizer_IsStrong(move)) {
                continue;
            }
            if (((tier == COVERAGE_STRONG_SUITED) || (tier == COVERAGE_SUITED)) &&
                !Randomizer_SuitsStat(species, move)) {
                continue;
            }
            sCandidates[count++] = move;
        }
        if (count != 0) {
            Randomizer_PickFrom(sCandidates, count);
            return 1;
        }
    }
    return 0;
}

// A support move, a good one if there's any: 0 if there's none at all
static s32 Randomizer_Support(s32 legalCount) {
    s32 good;
    s32 count;
    s32 i;

    for (good = 1; good >= 0; good--) {
        count = 0;
        for (i = 0; i < legalCount; i++) {
            const RandomizerMove* info = Randomizer_Move(sLegal[i]);

            if ((info->power == 0) && (!good || (info->flags & RANDOMIZER_MOVE_GOOD_SUPPORT))) {
                sCandidates[count++] = sLegal[i];
            }
        }
        if (count != 0) {
            Randomizer_PickFrom(sCandidates, count);
            return 1;
        }
    }
    return 0;
}

// A type's best move has to be this strong for a STAB slot (MIN_STAB_POWER)
#define MIN_STAB_POWER 40

/*
 * pickStrongMoves(): for each of the species' types, one of its strongest moves of
 * that type (within 70% of the best by power x accuracy); coverage up to three moves;
 * a support move; then whatever's left.
 */
static void Randomizer_StrongMoves(s32 species, s32 legalCount) {
    const RandomizerSpecies* sp = &gRandomizerSpecies[species];
    s32 types[2];
    s32 numTypes;
    s32 numLeft;
    s32 t;
    s32 i;

    sNumPicked = 0;

    // typesOf()
    types[0] = sp->type1;
    types[1] = sp->type2;
    numTypes = (sp->type1 == sp->type2) ? 1 : 2;

    for (t = 0; t < numTypes; t++) {
        s32 count = 0;
        s32 best = 0;
        s32 numStrongest = 0;

        for (i = 0; i < legalCount; i++) {
            const RandomizerMove* info = Randomizer_Move(sLegal[i]);

            if (Randomizer_IsReliable(sLegal[i]) && (info->type == types[t]) && (info->power >= MIN_STAB_POWER)) {
                sCandidates[count++] = sLegal[i];
                if (Randomizer_Strength(sLegal[i]) > best) {
                    best = Randomizer_Strength(sLegal[i]);
                }
            }
        }
        if (count == 0) {
            continue;
        }
        for (i = 0; i < count; i++) {
            if ((Randomizer_Strength(sCandidates[i]) * 10) >= (best * 7)) {
                sStrong[numStrongest++] = sCandidates[i];
            }
        }
        Randomizer_PickFrom(sStrong, numStrongest);
    }

    while ((sNumPicked < 3) && Randomizer_Coverage(species, legalCount)) {}

    if (!Randomizer_Support(legalCount)) {
        Randomizer_Coverage(species, legalCount);
    }

    // Top up from whatever's left (tiny learnsets), keeping the learnset order
    numLeft = 0;
    for (i = 0; i < legalCount; i++) {
        if (!Randomizer_IsPicked(sLegal[i])) {
            sCandidates[numLeft++] = sLegal[i];
        }
    }
    while ((sNumPicked < 4) && (numLeft > 0)) {
        s32 index = Randomizer_Below(numLeft);

        sPicked[sNumPicked++] = sCandidates[index];
        for (i = index; i < numLeft - 1; i++) {
            sCandidates[i] = sCandidates[i + 1];
        }
        numLeft--;
    }
}

// pickMovesFor(). Where the website pads with Struggle, the slot is left empty.
static void Randomizer_PickMoves(const RandomizerSettings* settings, const RandomizerRules* rules, s32 species,
                                 u8 moves[4]) {
    RandomizerMon rental;
    s32 i;

    sNumPicked = 0;

    if (settings->moveset == RANDOMIZER_MOVESET_CHAOS) {
        Randomizer_Copy(sScratch, gRandomizerChaosMoves, RANDOMIZER_NUM_CHAOS_MOVES);
        Randomizer_Shuffle(sScratch, RANDOMIZER_NUM_CHAOS_MOVES);
        Randomizer_Copy(sPicked, sScratch, 4);
        sNumPicked = 4;
    } else if ((settings->moveset == RANDOMIZER_MOVESET_STADIUM) && Randomizer_Rental(rules, species, &rental)) {
        for (i = 0; i < 4; i++) {
            if (rental.moves[i] != 0) {
                sPicked[sNumPicked++] = rental.moves[i];
            }
        }
    } else {
        s32 legalCount = Randomizer_LegalMoves(settings, species);

        if (settings->moveset == RANDOMIZER_MOVESET_LEGAL) {
            Randomizer_Shuffle(sLegal, legalCount);
            sNumPicked = (legalCount < 4) ? legalCount : 4;
            Randomizer_Copy(sPicked, sLegal, sNumPicked);
        } else {
            Randomizer_StrongMoves(species, legalCount);
        }
    }

    for (i = 0; i < 4; i++) {
        moves[i] = (i < sNumPicked) ? sPicked[i] : 0;
    }
}

/* Levels */

// levelForPokemon(): the cup's floor, raised for species that can't be that low
static s32 Randomizer_Level(const RandomizerRules* rules, s32 species) {
    s32 level;

    if (rules->level != 0) {
        return rules->level;
    }

    level = rules->levelMin;
    if (gRandomizerSpecies[species].minLevel > level) {
        level = gRandomizerSpecies[species].minLevel;
    }
    if ((rules->levelMax != 0) && (level > rules->levelMax)) {
        level = rules->levelMax;
    }
    return level;
}

// isValidLevelSum(): some three of the six fit under the cap, i.e. the three lowest do
s32 Randomizer_TeamFitsLevelSum(const RandomizerRules* rules, const RandomizerMon team[RANDOMIZER_TEAM_SIZE]) {
    s32 lowest[3];
    s32 i;
    s32 j;

    if (rules->levelSum == 0) {
        return 1;
    }

    lowest[0] = lowest[1] = lowest[2] = 0xFF;
    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        s32 level = team[i].level;

        for (j = 0; j < 3; j++) {
            if (level < lowest[j]) {
                s32 tmp = lowest[j];

                lowest[j] = level;
                level = tmp;
            }
        }
    }
    return (lowest[0] + lowest[1] + lowest[2]) <= rules->levelSum;
}

/* Team */

/*
 * randomizeFullTeam() for a Stadium 1 mode, after the seed is set. Returns 0 if the pool
 * has fewer than six Pokemon. The stat exp rolls follow the website's save export
 * (buildGen1Save), which draws a trainer ID first.
 */
s32 Randomizer_GenerateTeam(const RandomizerSettings* settings, const RandomizerRules* rules,
                            RandomizerMon team[RANDOMIZER_TEAM_SIZE]) {
    RandomizerMon rental;
    s32 count = 0;
    s32 species;
    s32 i;
    s32 j;

    for (species = 1; species <= RANDOMIZER_NUM_SPECIES; species++) {
        if (Randomizer_InCupPool(rules->cup, species)) {
            sPool[count++] = species;
        }
    }
    count = Randomizer_FilterPool(settings, rules->cup, sPool, count);
    if (settings->monoType && (rules->theme == 0)) {
        count = Randomizer_PickMonoType(sPool, count);
    }
    if (count < RANDOMIZER_TEAM_SIZE) {
        return 0;
    }

    // Species first: sScratch is reused for the Chaos move pool below. A theme the pool
    // can't make six of gives an unthemed team.
    if (rules->theme != 0) {
        if (!Randomizer_SampleTheme(rules, sPool, count, team)) {
            Randomizer_Sample(sPool, count, team);
        }
    } else if (settings->noSharedTypes && !settings->monoType) {
        Randomizer_SampleWithoutSharedTypes(sPool, count, team);
    } else {
        Randomizer_Sample(sPool, count, team);
    }

    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        RandomizerMon* mon = &team[i];

        Randomizer_PickMoves(settings, rules, mon->species, mon->moves);

        // getRandomGender(): Stadium 1 has no genders, but the website still rolls one
        if (gRandomizerSpecies[mon->species].flags & RANDOMIZER_SPECIES_GENDER_ROLL) {
            Randomizer_Next();
        }

        mon->level = Randomizer_Level(rules, mon->species);

        // pickGen1Dvs()
        if (settings->dvs == RANDOMIZER_STATS_RANDOM) {
            for (j = 0; j < 4; j++) {
                mon->dvs[j] = Randomizer_Below(16);
            }
        } else if ((settings->dvs == RANDOMIZER_STATS_STADIUM) && Randomizer_Rental(rules, mon->species, &rental)) {
            Randomizer_Copy(mon->dvs, rental.dvs, 4);
        } else {
            for (j = 0; j < 4; j++) {
                mon->dvs[j] = 15;
            }
        }
    }

    // The website's last resort when no three fit the level cap: every Pokemon at the
    // cup's floor or its minimum (so usually no change)
    if (!Randomizer_TeamFitsLevelSum(rules, team)) {
        for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
            team[i].level = rules->levelMin;
            if (gRandomizerSpecies[team[i].species].minLevel > team[i].level) {
                team[i].level = gRandomizerSpecies[team[i].species].minLevel;
            }
        }
    }

    // pickStatExp() in buildGen1Save()
    if (settings->statExp == RANDOMIZER_STATS_RANDOM) {
        Randomizer_Next(); // trainer ID
    }
    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        s32 fromRental =
            (settings->statExp == RANDOMIZER_STATS_STADIUM) && Randomizer_Rental(rules, team[i].species, &rental);

        for (j = 0; j < 5; j++) {
            if (settings->statExp == RANDOMIZER_STATS_RANDOM) {
                team[i].statExp[j] = Randomizer_Below(0x10000);
            } else {
                team[i].statExp[j] = fromRental ? rental.statExp[j] : 0xFFFF;
            }
        }
    }
    return 1;
}

#endif

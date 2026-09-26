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

// rng() < 0.75 and rng() < 0.5
#define RNG_BELOW_THREE_QUARTERS() (Randomizer_Next() < 0xC0000000)
#define RNG_BELOW_HALF() (Randomizer_Next() < 0x80000000)

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

/* Movesets */

static const RandomizerMove* Randomizer_Move(s32 move) {
    return &gRandomizerMoves[move];
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

enum {
    TAKE_ATTACK,          // attacks
    TAKE_STAB_ATTACK,     // attacks.filter(m => stabTypes.includes(typeOf(m)))
    TAKE_OTHER_TYPE,      // attacks.filter(m => typeOf(m) !== firstType)
    TAKE_SUPPORT,         // supports
    TAKE_GOOD_SUPPORT,    // supports.filter(m => GOOD_SUPPORT_MOVES.includes(moveKey(m)))
    TAKE_PHYSICAL_FILLER, // filler for a physical attacker
    TAKE_SPECIAL_FILLER   // filler for a special attacker
};

static s32 Randomizer_IsPicked(s32 move) {
    s32 i;

    for (i = 0; i < sNumPicked; i++) {
        if (sPicked[i] == move) {
            return 1;
        }
    }
    return 0;
}

static s32 Randomizer_Matches(s32 kind, s32 move, s32 species, s32 firstType) {
    const RandomizerMove* info = Randomizer_Move(move);

    switch (kind) {
        case TAKE_ATTACK:
            return info->power > 0;
        case TAKE_STAB_ATTACK:
            return (info->power > 0) && Randomizer_HasType(species, info->type);
        case TAKE_OTHER_TYPE:
            return (info->power > 0) && (info->type != firstType);
        case TAKE_SUPPORT:
            return info->power == 0;
        case TAKE_GOOD_SUPPORT:
            return (info->power == 0) && info->goodSupport;
        case TAKE_PHYSICAL_FILLER:
            return (info->power > 0) && (info->type < TYPE_PHYSICAL_END);
        case TAKE_SPECIAL_FILLER:
            return (info->power > 0) && (info->type >= TYPE_SPECIAL_START);
    }
    return 0;
}

// take() in pickStadiumStyleMoves(): a random unpicked move of the given kind, 75% of the
// time from the strong ones (power >= 70, accuracy >= 85) if there are any
static s32 Randomizer_Take(s32 kind, s32 legalCount, s32 species, s32 firstType) {
    s32 numCandidates = 0;
    s32 numStrong = 0;
    s32 i;

    for (i = 0; i < legalCount; i++) {
        if (Randomizer_Matches(kind, sLegal[i], species, firstType) && !Randomizer_IsPicked(sLegal[i])) {
            sCandidates[numCandidates++] = sLegal[i];
        }
    }
    if (numCandidates == 0) {
        return 0;
    }

    for (i = 0; i < numCandidates; i++) {
        const RandomizerMove* info = Randomizer_Move(sCandidates[i]);

        if ((info->power >= 70) && (info->accuracy >= 85)) {
            sStrong[numStrong++] = sCandidates[i];
        }
    }

    if ((numStrong != 0) && RNG_BELOW_THREE_QUARTERS()) {
        sPicked[sNumPicked++] = sStrong[Randomizer_Below(numStrong)];
    } else {
        sPicked[sNumPicked++] = sCandidates[Randomizer_Below(numCandidates)];
    }
    return 1;
}

// pickStadiumStyleMoves(): a STAB attack, coverage, a support move, then filler
static void Randomizer_StadiumMoves(s32 species, s32 legalCount) {
    s32 filler = (gRandomizerSpecies[species].flags & RANDOMIZER_SPECIES_PHYSICAL) ? TAKE_PHYSICAL_FILLER
                                                                                   : TAKE_SPECIAL_FILLER;
    s32 firstType;
    s32 numLeft;
    s32 i;

    sNumPicked = 0;

    if (!Randomizer_Take(TAKE_STAB_ATTACK, legalCount, species, NO_TYPE)) {
        Randomizer_Take(TAKE_ATTACK, legalCount, species, NO_TYPE);
    }

    firstType = (sNumPicked != 0) ? Randomizer_Move(sPicked[0])->type : NO_TYPE;
    if (!Randomizer_Take(TAKE_OTHER_TYPE, legalCount, species, firstType)) {
        Randomizer_Take(TAKE_ATTACK, legalCount, species, NO_TYPE);
    }

    if (!Randomizer_Take(TAKE_GOOD_SUPPORT, legalCount, species, NO_TYPE)) {
        Randomizer_Take(TAKE_SUPPORT, legalCount, species, NO_TYPE);
    }

    if (!(RNG_BELOW_HALF() && Randomizer_Take(filler, legalCount, species, NO_TYPE))) {
        if (!Randomizer_Take(TAKE_SUPPORT, legalCount, species, NO_TYPE) &&
            !Randomizer_Take(filler, legalCount, species, NO_TYPE)) {
            Randomizer_Take(TAKE_ATTACK, legalCount, species, NO_TYPE);
        }
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
static void Randomizer_PickMoves(const RandomizerSettings* settings, s32 species, u8 moves[4]) {
    s32 i;

    sNumPicked = 0;

    if (settings->moveset == RANDOMIZER_MOVESET_CHAOS) {
        Randomizer_Copy(sScratch, gRandomizerChaosMoves, RANDOMIZER_NUM_CHAOS_MOVES);
        Randomizer_Shuffle(sScratch, RANDOMIZER_NUM_CHAOS_MOVES);
        Randomizer_Copy(sPicked, sScratch, 4);
        sNumPicked = 4;
    } else {
        s32 legalCount = Randomizer_LegalMoves(settings, species);

        if (settings->moveset == RANDOMIZER_MOVESET_STADIUM) {
            Randomizer_StadiumMoves(species, legalCount);
        } else {
            Randomizer_Shuffle(sLegal, legalCount);
            sNumPicked = (legalCount < 4) ? legalCount : 4;
            Randomizer_Copy(sPicked, sLegal, sNumPicked);
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
    if (settings->monoType) {
        count = Randomizer_PickMonoType(sPool, count);
    }
    if (count < RANDOMIZER_TEAM_SIZE) {
        return 0;
    }

    // randomSample(pool, 6); sScratch is reused for the Chaos move pool below
    Randomizer_Copy(sScratch, sPool, count);
    Randomizer_Shuffle(sScratch, count);
    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        team[i].species = sScratch[i];
    }

    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        RandomizerMon* mon = &team[i];

        Randomizer_PickMoves(settings, mon->species, mon->moves);

        // getRandomGender(): Stadium 1 has no genders, but the website still rolls one
        if (gRandomizerSpecies[mon->species].flags & RANDOMIZER_SPECIES_GENDER_ROLL) {
            Randomizer_Next();
        }

        mon->level = Randomizer_Level(rules, mon->species);

        // pickGen1Dvs()
        for (j = 0; j < 4; j++) {
            mon->dvs[j] = settings->randomDvs ? Randomizer_Below(16) : 15;
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
    if (settings->randomStatExp) {
        Randomizer_Next(); // trainer ID
    }
    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        for (j = 0; j < 5; j++) {
            team[i].statExp[j] = settings->randomStatExp ? Randomizer_Below(0x10000) : 0xFFFF;
        }
    }
    return 1;
}

#endif

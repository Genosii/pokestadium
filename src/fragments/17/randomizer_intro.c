/*
 * Random Pokemon for the intro (randomizer_intro, see randomizer_intro.h): its first four
 * scenes, two on the ground, one in the sky and one underwater, get Pokemon from pools
 * that suit each, a different set every time the console starts. The last scene, with
 * Pikachu, Psyduck, Clefairy and Jigglypuff, is one model with its own animation and
 * stays as it is. Each boot is counted in the save file, so emulators, which start the same
 * way every time, get a new set too. Built only with RANDOMIZER=1; empty otherwise so the default build still
 * matches.
 *
 * The scenes are tables of Pokemon (D_86B0C4C8, docs/game-screens.md) that the intro reads
 * as it reaches each scene; this runs once as it starts and rewrites the species in them.
 * Every Pokemon plays its first animation, its idle one in battle, and the intro moves it
 * along, so the sky and water pools are Pokemon whose idle animation flies or swims.
 * Charizard is the exception, which the game gives its tenth, flying, animation.
 */
#include "randomizer_intro.h"

#ifdef RANDOMIZER

#include "src/randomizer_save.h"

extern unk_D_86B0C4C8* D_86B0C4C8[];

#define END_OF_SCENE 0x98 // the species that ends a scene's table
#define CHARIZARD 6
#define CHARIZARD_FLYING 10 // its animation in the sky scene

enum { SCENE_GROUND_1, SCENE_GROUND_2, SCENE_SKY, SCENE_WATER, SCENE_COUNT };

typedef struct RandomizerIntroPool {
    /* 0x0 */ const u8* species;
    /* 0x4 */ s32 count;
} RandomizerIntroPool; // size = 0x8

// About the size of the Pokemon they replace (1 to 2 metres), standing on the ground; not
// Muk or Snorlax, which fill the screen when the camera passes close
static const u8 sGround[] = {
    3,   // Venusaur
    6,   // Charizard
    9,   // Blastoise
    28,  // Sandslash
    31,  // Nidoqueen
    34,  // Nidoking
    36,  // Clefable
    38,  // Ninetales
    45,  // Vileplume
    55,  // Golduck
    57,  // Primeape
    59,  // Arcanine
    62,  // Poliwrath
    65,  // Alakazam
    67,  // Machoke
    68,  // Machamp
    71,  // Victreebel
    76,  // Golem
    78,  // Rapidash
    80,  // Slowbro
    85,  // Dodrio
    94,  // Gengar
    97,  // Hypno
    99,  // Kingler
    103, // Exeggutor
    105, // Marowak
    106, // Hitmonlee
    107, // Hitmonchan
    108, // Lickitung
    112, // Rhydon
    113, // Chansey
    115, // Kangaskhan
    122, // Mr. Mime
    123, // Scyther
    124, // Jynx
    125, // Electabuzz
    126, // Magmar
    127, // Pinsir
    128, // Tauros
    141, // Kabutops
    149, // Dragonite
    150, // Mewtwo
};

// Flying in their idle animation (Charizard with its own); not the Pokemon that only
// float, like Gastly and Koffing
static const u8 sSky[] = {
    6,   // Charizard
    12,  // Butterfree
    15,  // Beedrill
    16,  // Pidgey
    17,  // Pidgeotto
    18,  // Pidgeot
    21,  // Spearow
    22,  // Fearow
    41,  // Zubat
    42,  // Golbat
    49,  // Venomoth
    142, // Aerodactyl
    144, // Articuno
    145, // Zapdos
    146, // Moltres
    149, // Dragonite
};

// Swimming in their idle animation
static const u8 sWater[] = {
    60,  // Poliwag
    72,  // Tentacool
    73,  // Tentacruel
    86,  // Seel
    87,  // Dewgong
    90,  // Shellder
    91,  // Cloyster
    116, // Horsea
    117, // Seadra
    118, // Goldeen
    119, // Seaking
    120, // Staryu
    121, // Starmie
    129, // Magikarp
    130, // Gyarados
    131, // Lapras
    134, // Vaporeon
    138, // Omanyte
    139, // Omastar
    140, // Kabuto
    147, // Dratini
    148, // Dragonair
};

static const RandomizerIntroPool sPools[SCENE_COUNT] = {
    { sGround, ARRAY_COUNT(sGround) },
    { sGround, ARRAY_COUNT(sGround) },
    { sSky, ARRAY_COUNT(sSky) },
    { sWater, ARRAY_COUNT(sWater) },
};

#define BOOT_COUNT_MAGIC 0x524E4442 // "RNDB"

// How many times the intro has run, kept in the save file (src/randomizer_save.h)
typedef struct RandomizerBootCount {
    /* 0x0 */ u32 magic;
    /* 0x4 */ u32 count;
} RandomizerBootCount; // size = 0x8

static u32 sRandomState;

// A 32-bit LCG, the same as the team generator's; its core fragment isn't loaded here
static s32 Randomizer_IntroBelow(s32 n) {
    sRandomState = (sRandomState * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandomState * (u32)n) >> 32;
}

static s32 Randomizer_IntroUsed(const u8* used, s32 count, s32 species) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (used[i] == species) {
            return 1;
        }
    }
    return 0;
}

/*
 * The seed: the CPU's cycle count, which differs from boot to boot on a console but not in
 * an emulator, which starts the same way every time, so mixed with a count of boots kept in
 * the save file. The intro has loaded save bank 2 already (func_86B01190).
 */
static u32 Randomizer_IntroSeed(void) {
    RandomizerBootCount boots;
    u32 seed = osGetCount();

    if (!func_80028AFC(RANDOMIZER_SAVE_BANK)) {
        return seed;
    }
    bcopy(RANDOMIZER_SAVE_AT(RANDOMIZER_SAVE_BOOT_COUNT), &boots, sizeof(boots));
    if (boots.magic != BOOT_COUNT_MAGIC) {
        boots.magic = BOOT_COUNT_MAGIC;
        boots.count = 0;
    }
    seed ^= boots.count * 0x9E3779B9;
    boots.count++;
    bcopy(&boots, RANDOMIZER_SAVE_AT(RANDOMIZER_SAVE_BOOT_COUNT), sizeof(boots));
    RANDOMIZER_SAVE_WRITE();
    return seed;
}

// Run once as the intro starts (Randomizer_IntroLoad in randomizer_intro_stub.s)
s32 Randomizer_IntroEntry(UNUSED s32 arg0, UNUSED s32 arg1) {
    u8 used[SCENE_COUNT * 4];
    s32 numUsed = 0;
    s32 scene;

    sRandomState = Randomizer_IntroSeed();

    for (scene = 0; scene < SCENE_COUNT; scene++) {
        const RandomizerIntroPool* pool = &sPools[scene];
        unk_D_86B0C4C8* mon;
        s32 slot = 0;

        for (mon = D_86B0C4C8[scene]; mon->unk_0C != END_OF_SCENE; mon++, slot++) {
            s32 species;

#ifdef RANDOMIZER_INTRO_TEST_PAGE
            // For checking the pools in an emulator: each page shows the next Pokemon of
            // each pool in turn, the ground scenes sharing theirs
            species = pool->species[((RANDOMIZER_INTRO_TEST_PAGE * ((scene < SCENE_SKY) ? 8 : 4)) +
                                     ((scene == SCENE_GROUND_2) ? 4 : 0) + slot) %
                                    pool->count];
#else
            do {
                species = pool->species[Randomizer_IntroBelow(pool->count)];
            } while (Randomizer_IntroUsed(used, numUsed, species));
#endif
            used[numUsed++] = species;
            mon->unk_0C = species;
            mon->unk_10 = ((scene == SCENE_SKY) && (species == CHARIZARD)) ? CHARIZARD_FLYING : 0;
        }
    }
    return 0;
}

#endif

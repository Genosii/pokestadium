/*
 * The randomizer's cup screen fragment (randomizer_cup, see randomizer_cup.h): what the
 * opponents' "Mode" option adds between the battles of a cup or the Gym Leader Castle.
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * Factory: after each win (but the last), a panel shows the three Pokemon the
 * trainer battled with and the player's six. The player can take one of the trainer's,
 * which keeps its level, DVs, stat exp and moves, in place of one of their own, or keep
 * their team. It goes into the team the battle-select screen reads (and a "save and
 * quit" saves), healed and with the player's trainer name and ID. One swap per win: if
 * the player kept their team, the next-battle menu that follows gets a "Swap a Pokemon"
 * line that brings the panel back.
 *
 * Rogue: after each win (but the last), a reward instead: a new Pokemon, one of three made
 * with the player's options (none the player or the trainer has), in place of one of their
 * own; or a move changed, one of a Pokemon's for one of three others it can learn (any
 * move with the Chaos moveset). Built as the rental card's "Randomize" builds them
 * (randomizer_core): the "Stadium" options give Strong moves and top stats. Skipped, the
 * menu's "Pick a reward" line brings it back. A loss ends the run: a panel says so, then
 * the screen quits as its own menu's "Quit" would, so there's no continue or retry.
 * Badges and records aren't touched.
 *
 * The screen's own frames (func_84B01AA0) keep running while a panel is up, and draw it
 * through the draw hook at the end of each one.
 *
 * Factory and Rogue were the options' "Playstyle"; they're the Battle Frontier's now, and its
 * runs (gRandomizerState.frontier) don't have them yet (parts 2 and 3). A run comes here with
 * the mode of a cup the US version left out (randomizer_stadium.c), which becomes its cup's
 * as the screen starts, before the first trainer is loaded. It goes on past the eighth
 * trainer: the round back to 1, the next ball's trainers (the Poke and Prime Cups have four;
 * the Petit and Pika Cups one set of eight), and the lap counted and mixed into the
 * opponents' seed so their teams don't repeat. A loss ends it, and quitting ends it without
 * saving it to continue.
 */
#include "randomizer_cup.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/22630.h"
#include "src/232C0.h"
#include "src/26820.h"
#include "src/29BA0.h"
#include "src/2C1C0.h"
#include "src/2E110.h"
#include "src/49790.h"
#include "src/controller.h"
#include "src/hal_libc.h"

// The screen's coordinates
#define PANEL_X 8
#define PANEL_Y 8
#define PANEL_W 304
#define PANEL_H 224
#define PANEL_COLOR 0x2121     // the blue of the game's menus
#define HIGHLIGHT_COLOR 0x32B1 // the Pokemon under the cursor
#define TAKEN_COLOR 0x2411     // the trainer's Pokemon picked to take

#define TITLE_Y (PANEL_Y + 6)
#define THEIRS_Y (PANEL_Y + 26)
#define YOURS_LABEL_Y (THEIRS_Y + BLOCK_H + 5)
#define YOURS_Y (YOURS_LABEL_Y + 13)
#define FOOTER_Y (PANEL_Y + PANEL_H - 16)

// A Pokemon: name and level, then its moves; three to a row
#define COLUMNS 3
#define COLUMN_X(i) (PANEL_X + 12 + ((i) % COLUMNS) * 96)
#define COLUMN_W 88
#define LINE_H 9
#define BLOCK_H ((5 * LINE_H) + 2)
#define MOVE_INDENT 2

#define NUM_MOVES 4
#define TEAM_SIZE 6
#define NUM_OFFERS 3
#define OFFER_TRIES 8 // teams made to find three new Pokemon
#define REWARD_POKEMON 0
#define REWARD_MOVE 1
#define REWARD_Y 72
#define REWARD_LINE_H 16
#define MAX_PP_UPS (3 << 6)
#define NICKNAME_LENGTH 10

// func_8002D5AC's archive of move names
#define MOVE_NAMES_ARCHIVE 0x25

// The screen's quit flag (the loss menu's "Quit")
#define CUP_QUIT 1
#define CUP_TITLE 0x40 // with CUP_QUIT: to the title screen (func_80029BC0)

#define MAX_ROUND 8
#define POKE_CUP_MODE 3
#define PRIME_CUP_MODE 6
#define MASTER_BALL 3
// The modes of the two cups the US version left out, which a Battle Frontier run starts with
#define ROGUE_SLOT_MODE 1
#define FACTORY_SLOT_MODE 2

// The next-battle menus (fragment63_3A1C30.c): 1 in cups, 2 in the Castle, as
// func_84B022A0 opens them, and their line height and layout (func_84B0DE04)
#define MENU_CUP 1
#define MENU_CASTLE 2
#define MENU_LINE_H 0xE
#define MENU_ITEMS_X 0x22
#define MENU_ITEMS_Y 0x18
#define MENU_COUNT_X 0x4A
// func_84B0DE04 shows the cup's continues only at this height; with the swap line
// they move down and are drawn here instead
#define MENU_COUNT_HEIGHT 0x48
#define MENU_COUNT_Y (0x36 + MENU_LINE_H)
#define MENU_OPEN 4 // unk_02 once the window is fully open

extern unk_D_84B17550 D_84B17550[];
extern unk_D_84B26640 D_84B26640;

// Each menu's height without the swap line
static s16 sMenuHeights[2];
static s32 sMenuSaved;

enum {
    STEP_OFF,
    STEP_THEIRS,   // picking one of the trainer's Pokemon (or Rogue's new ones)
    STEP_YOURS,    // picking one of the player's to give up for it
    STEP_RUN_OVER, // Rogue's loss
    STEP_REWARD,   // Rogue: a new Pokemon or a move
    STEP_WHOSE,    // whose move to change,
    STEP_WHICH,    // which of its moves,
    STEP_LEARN     // and which new one
};

static s32 sStep;
static s32 sSwapped; // this win's swap is done
static s32 sCursor;
static s32 sTaken; // index into sTheirs
static s32 sTheirs[TEAM_SIZE];
static s32 sNumTheirs;
static char** sMoveNames;
static s32 sRogue; // the Pokemon on offer are sOffers, not the trainer's
static unk_func_80026268_arg0 sOffers[NUM_OFFERS];
static s32 sMon;  // the player's Pokemon whose move changes,
static s32 sMove; // which one,
static u8 sLearn[NUM_OFFERS]; // and the ones it can learn instead
static s32 sNumLearn;
static s32 sUnpacked;
static s32 sBeaten;  // trainers a Battle Frontier run has beaten, for its end
static s32 sRunKind; // and which it was
static u32 sRandom;

static unk_D_800AE540_0004* Randomizer_Player(void) {
    return D_800AE540.unk_1194[0].unk_08[0];
}

static unk_D_800AE540_0004* Randomizer_Trainer(void) {
    return D_800AE540.unk_1194[1].unk_08[0];
}

static s32 Randomizer_Mode(void) {
    return RANDOMIZER_STATE_VALID() ? gRandomizerState.mode : RANDOMIZER_MODE_NORMAL;
}

// The player's team as the battle-select screen reads it (and a save keeps it)
static unk_func_80026268_arg0* Randomizer_Yours(s32* count) {
    unk_D_800AE540_0874* team = Randomizer_Player()->unk_214;

    *count = team->unk_002;
    return team->unk_028;
}

static void Randomizer_DrawMon(unk_func_80026268_arg0* mon, s32 x, s32 y) {
    s32 i;

    func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
    func_8001F1E8(x, y, "%s", mon->unk_30);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(x + COLUMN_W - func_8001F5B0(1, 0, "L%d", mon->unk_24), y, "L%d", mon->unk_24);

    if (sMoveNames == NULL) {
        return;
    }
    for (i = 0; (i < NUM_MOVES) && (mon->unk_09[i] != 0); i++) {
        func_8001F1E8(x + MOVE_INDENT, y + ((i + 1) * LINE_H), "%s",
                      func_8002D7C0(NULL, 0, sMoveNames, mon->unk_09[i] - 1));
    }
}

// One of the Pokemon on offer: the trainer's, or Rogue's new ones
static unk_func_80026268_arg0* Randomizer_Offer(s32 i) {
    return sRogue ? &sOffers[i] : &Randomizer_Trainer()->unk_01C[sTheirs[i]];
}

// A box behind a Pokemon, for the cursor or the one picked
static void Randomizer_DrawBox(s32 i, s32 y, u16 color) {
    func_80020460(COLUMN_X(i) - 5, y - 4, COLUMN_W + 10, BLOCK_H + 5, color);
}

// The moves to learn, in the top row: name, power and accuracy
static void Randomizer_DrawLearn(void) {
    s32 i;

    for (i = 0; i < sNumLearn; i++) {
        RandomizerMove* move = &gRandomizerMoves[sLearn[i]];

        func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
        func_8001F1E8(COLUMN_X(i), THEIRS_Y, "%s", func_8002D7C0(NULL, 0, sMoveNames, sLearn[i] - 1));
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        if (move->power > 1) {
            func_8001F1E8(COLUMN_X(i) + MOVE_INDENT, THEIRS_Y + LINE_H, "Power %d", move->power);
        } else {
            func_8001F1E8(COLUMN_X(i) + MOVE_INDENT, THEIRS_Y + LINE_H, "Power -");
        }
        func_8001F1E8(COLUMN_X(i) + MOVE_INDENT, THEIRS_Y + (2 * LINE_H), "Accuracy %d", move->accuracy);
    }
}

static void Randomizer_DrawSwap(void) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);
    switch (sStep) {
        case STEP_THEIRS:
        case STEP_LEARN:
            Randomizer_DrawBox(sCursor, THEIRS_Y, HIGHLIGHT_COLOR);
            break;
        case STEP_YOURS:
            Randomizer_DrawBox(sTaken, THEIRS_Y, TAKEN_COLOR);
            /* fallthrough */
        case STEP_WHOSE:
            Randomizer_DrawBox(sCursor, YOURS_Y + ((sCursor / COLUMNS) * BLOCK_H), HIGHLIGHT_COLOR);
            break;
        case STEP_WHICH:
            Randomizer_DrawBox(sMon, YOURS_Y + ((sMon / COLUMNS) * BLOCK_H), TAKEN_COLOR);
            func_80020460(COLUMN_X(sMon) - 2, YOURS_Y + ((sMon / COLUMNS) * BLOCK_H) + ((sCursor + 1) * LINE_H) - 1,
                          COLUMN_W + 4, LINE_H, HIGHLIGHT_COLOR);
            break;
    }

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    switch (sStep) {
        case STEP_THEIRS:
            if (sRogue) {
                func_8001F1E8(PANEL_X + 8, TITLE_Y, "Take a new Pok\xE9mon?");
            } else {
                func_8001F1E8(PANEL_X + 8, TITLE_Y, "Take one of %s's?", trainer->unk_008);
            }
            break;
        case STEP_YOURS:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Swap %s for?", Randomizer_Offer(sTaken)->unk_30);
            break;
        case STEP_WHOSE:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Change whose move?");
            break;
        case STEP_WHICH:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Change which move?");
            break;
        case STEP_LEARN:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Learn which move?");
            break;
    }

    func_8001EBE0(1, 0);
    if (sStep == STEP_LEARN) {
        Randomizer_DrawLearn();
    } else if ((sStep == STEP_THEIRS) || (sStep == STEP_YOURS)) {
        for (i = 0; i < sNumTheirs; i++) {
            Randomizer_DrawMon(Randomizer_Offer(i), COLUMN_X(i), THEIRS_Y);
        }
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 8, YOURS_LABEL_Y, "Your team");
    for (i = 0; i < count; i++) {
        Randomizer_DrawMon(&yours[i], COLUMN_X(i), YOURS_Y + ((i / COLUMNS) * BLOCK_H));
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    switch (sStep) {
        case STEP_THEIRS:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, sRogue ? "A: take it   B: back" : "A: take it   B: keep your team");
            break;
        case STEP_YOURS:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: swap   B: back");
            break;
        case STEP_LEARN:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: learn it   B: back");
            break;
        default:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: choose   B: back");
            break;
    }
    func_8001F444();
}

// Rogue's choice of reward
static void Randomizer_DrawReward(void) {
    func_80020460(PANEL_X + 40, REWARD_Y, PANEL_W - 80, 88, PANEL_COLOR);
    func_80020460(PANEL_X + 50, REWARD_Y + 28 + (sCursor * REWARD_LINE_H) - 3, PANEL_W - 100, REWARD_LINE_H - 2,
                  HIGHLIGHT_COLOR);

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 56, REWARD_Y + 8, "Pick a reward");
    func_8001EBE0(1, 0);
    func_8001F1E8(PANEL_X + 60, REWARD_Y + 28, "A new Pok\xE9mon");
    func_8001F1E8(PANEL_X + 60, REWARD_Y + 28 + REWARD_LINE_H, "Change a move");
    func_8001F1E8(PANEL_X + 56, REWARD_Y + 68, "A: choose   B: not now");
    func_8001F444();
}

static void Randomizer_DrawRunOver(void) {
    s32 frontier = sRunKind;

    func_80020460(PANEL_X + 40, 72, PANEL_W - 80, 88, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 56, 84, "Your run is over");
    func_8001EBE0(1, 0);
    if (frontier == FRONTIER_NONE) {
        func_8001F1E8(PANEL_X + 56, 108, "Rogue mode: no retries.");
    } else {
        func_8001F1E8(PANEL_X + 56, 104, "%s: a loss ends the run.",
                      (frontier == FRONTIER_ROGUE) ? "Rogue" : "Factory");
        func_8001F1E8(PANEL_X + 56, 118, "Trainers beaten: %d", sBeaten);
    }
    func_8001F1E8(PANEL_X + 56, 136, "A: back to the menu");
    func_8001F444();
}

// The "Swap a Pokemon" line, and the cup's continues below it, on the open menu
static void Randomizer_DrawMenuLine(void) {
    unk_D_84B2665C* menu = &D_84B26640.unk_1C;
    unk_D_84B17550* def;

    if ((menu->unk_00 == 0) || ((menu->unk_01 != MENU_CUP) && (menu->unk_01 != MENU_CASTLE)) ||
        (menu->unk_02 < MENU_OPEN)) {
        return;
    }
    def = &D_84B17550[menu->unk_01 - 1];
    if (def->unk_08 <= RANDOMIZER_CUP_SWAP_ITEM) {
        return;
    }

    func_8001F3F4();
    func_8001EBE0(2, 0);
    if (menu->unk_04 == RANDOMIZER_CUP_SWAP_ITEM) {
        func_8001F324(0xFF, 0xFF, 0, 0xFF);
    } else {
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    }
    func_8001F1E8(def->unk_00 + MENU_ITEMS_X, def->unk_02 + MENU_ITEMS_Y + (RANDOMIZER_CUP_SWAP_ITEM * MENU_LINE_H),
                  (Randomizer_Mode() == RANDOMIZER_MODE_ROGUE) ? "Pick a reward" : "Swap a Pok\xE9mon");
    if (menu->unk_01 == MENU_CUP) {
        func_8001EBE0(1, 0);
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F1E8(def->unk_00 + MENU_COUNT_X, def->unk_02 + MENU_COUNT_Y, "%s%d", func_84B0037C(0x2C),
                      D_800AE540.unk_11F3);
    }
    func_8001F444();
}

// Gives the next-battle menus the swap line, or takes it away
static void Randomizer_SetMenuLine(s32 on) {
    s32 i;

    for (i = 0; i < 2; i++) {
        unk_D_84B17550* def = &D_84B17550[i];

        if (!sMenuSaved) {
            sMenuHeights[i] = def->unk_06;
        }
        def->unk_08 = on ? (RANDOMIZER_CUP_SWAP_ITEM + 1) : RANDOMIZER_CUP_SWAP_ITEM;
        // Taller by a line, and never MENU_COUNT_HEIGHT, which would draw the
        // continues where the line is
        def->unk_06 = sMenuHeights[i] + (on ? (MENU_LINE_H + 2) : 0);
    }
    sMenuSaved = 1;
}

// At the end of every frame's drawing (func_84B014DC)
static void Randomizer_CupDraw(void) {
    Randomizer_DrawMenuLine();

    switch (sStep) {
        case STEP_THEIRS:
        case STEP_YOURS:
        case STEP_WHOSE:
        case STEP_WHICH:
        case STEP_LEARN:
            Randomizer_DrawSwap();
            break;

        case STEP_REWARD:
            Randomizer_DrawReward();
            break;

        case STEP_RUN_OVER:
            Randomizer_DrawRunOver();
            break;
    }
}

// The trainer's Pokemon becomes the player's, in slot
static void Randomizer_Swap(unk_func_80026268_arg0* theirs, unk_func_80026268_arg0* slot) {
    unk_func_80026268_arg0 mon = *theirs;
    char name[0x20];
    s32 i;

    // The player's trainer name and ID, and the species' name, as rentals have
    mon.unk_0E = slot->unk_0E;
    _bcopy(slot->unk_3B, mon.unk_3B, sizeof(mon.unk_3B));
    // Where it came from, as the team's other Pokemon
    mon.unk_52 = slot->unk_52;
    mon.unk_53 = slot->unk_53;
    bzero(mon.unk_30, sizeof(mon.unk_30));
    func_80021CA4(name, mon.unk_00.unk_00);
    for (i = 0; (i < NICKNAME_LENGTH) && (name[i] != '\0'); i++) {
        mon.unk_30[i] = name[i];
    }
    // Stats worked out again, with full HP and PP and no status after its battle
    func_80022734(&mon);
    func_800228B0(&mon);
    *slot = mon;
}

// The same LCG as the team generator's, for the moves on offer
static s32 Randomizer_CupBelow(s32 n) {
    sRandom = (sRandom * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandom * (u32)n) >> 32;
}

static void Randomizer_Unpack(void) {
    if (!sUnpacked) {
        // The generator's tables, in randomizer_core, loaded with this fragment
        Randomizer_UnpackData();
        sUnpacked = 1;
    }
}

// Whether the player, the trainer or another of the offers has the species
static s32 Randomizer_Seen(s32 species) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);
    for (i = 0; i < count; i++) {
        if (yours[i].unk_00.unk_00 == species) {
            return 1;
        }
    }
    for (i = 0; i < trainer->unk_002; i++) {
        if (trainer->unk_01C[i].unk_00.unk_00 == species) {
            return 1;
        }
    }
    for (i = 0; i < sNumTheirs; i++) {
        if (sOffers[i].unk_00.unk_00 == species) {
            return 1;
        }
    }
    return 0;
}

/*
 * Rogue's three new Pokemon: teams made with the player's options and the cup's rules (no
 * rentals: the "Stadium" options give Strong moves and top stats, as the rental card's
 * "Randomize" does), the first three not seen yet taken
 */
static void Randomizer_MakeOffers(void) {
    unk_func_80026268_arg0* yours;
    RandomizerRules rules;
    RandomizerMon team[RANDOMIZER_TEAM_SIZE];
    s32 count;
    s32 tries;
    s32 i;

    Randomizer_Unpack();
    yours = Randomizer_Yours(&count);
    Randomizer_GetRules(D_800AE540.unk_0001, yours[0].unk_24, &rules);
    rules.rental = NULL;
    Randomizer_Seed(osGetCount());

    sNumTheirs = 0;
    for (tries = 0; (tries < OFFER_TRIES) && (sNumTheirs < NUM_OFFERS); tries++) {
        if (!Randomizer_GenerateTeam(&gRandomizerState.settings, &rules, team)) {
            continue;
        }
        for (i = 0; (i < RANDOMIZER_TEAM_SIZE) && (sNumTheirs < NUM_OFFERS); i++) {
            if (!Randomizer_Seen(team[i].species)) {
                Randomizer_BuildPokemon(&sOffers[sNumTheirs], &team[i], &yours[0]);
                sNumTheirs++;
            }
        }
    }
}

/*
 * Three moves the Pokemon can learn and doesn't know: from its learnset (Gen 1's own moves
 * only with "No tradebacks"), or any with the Chaos moveset
 */
static void Randomizer_MakeLearn(unk_func_80026268_arg0* mon) {
    RandomizerSettings* settings = &gRandomizerState.settings;
    RandomizerSpecies* species;
    u8 moves[RANDOMIZER_NUM_MOVES];
    s32 count = 0;
    s32 known;
    s32 move;
    s32 i;
    s32 j;

    Randomizer_Unpack();
    species = &gRandomizerSpecies[mon->unk_00.unk_00];
    for (i = 0; i < ((settings->moveset == RANDOMIZER_MOVESET_CHAOS) ? RANDOMIZER_NUM_CHAOS_MOVES
                                                                     : species->learnsetCount);
         i++) {
        if (settings->moveset == RANDOMIZER_MOVESET_CHAOS) {
            move = gRandomizerChaosMoves[i];
        } else {
            RandomizerLearnsetMove* learn = &gRandomizerLearnsets[species->learnsetStart + i];

            if (settings->noTradeback && !learn->gen1) {
                continue;
            }
            move = learn->move;
        }
        known = (move == 0);
        for (j = 0; j < NUM_MOVES; j++) {
            known |= (mon->unk_09[j] == move);
        }
        for (j = 0; j < count; j++) {
            known |= (moves[j] == move);
        }
        if (!known && (count < RANDOMIZER_NUM_MOVES)) {
            moves[count++] = move;
        }
    }

    // Three of them, each picked from those not picked yet
    sRandom = osGetCount();
    for (sNumLearn = 0; (sNumLearn < NUM_OFFERS) && (count > 0); sNumLearn++) {
        i = Randomizer_CupBelow(count);
        sLearn[sNumLearn] = moves[i];
        moves[i] = moves[--count];
    }
}

// Stats worked out again, with full HP and PP and no status after its battle
static void Randomizer_Heal(unk_func_80026268_arg0* mon) {
    func_80022734(mon);
    func_800228B0(mon);
}

// How many lines the cursor goes over in a step
static s32 Randomizer_StepLines(void) {
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);
    switch (sStep) {
        case STEP_REWARD:
            return 2;
        case STEP_THEIRS:
            return sNumTheirs;
        case STEP_LEARN:
            return sNumLearn;
        case STEP_WHICH:
            // Its moves, and the first empty slot if it has one
            for (i = 0; (i < NUM_MOVES) && (yours[sMon].unk_09[i] != 0); i++) {}
            return MIN(i + 1, NUM_MOVES);
        default:
            return count;
    }
}

static void Randomizer_PanelA(void) {
    unk_func_80026268_arg0* yours;
    unk_func_80026268_arg0* mon;
    s32 count;

    yours = Randomizer_Yours(&count);
    switch (sStep) {
        case STEP_REWARD:
            if (sCursor == REWARD_POKEMON) {
                if (sNumTheirs == 0) {
                    Randomizer_MakeOffers();
                }
                if (sNumTheirs == 0) {
                    return;
                }
                sStep = STEP_THEIRS;
            } else {
                sStep = STEP_WHOSE;
            }
            sCursor = 0;
            break;

        case STEP_THEIRS:
            sTaken = sCursor;
            sCursor = 0;
            sStep = STEP_YOURS;
            break;

        case STEP_YOURS:
            Randomizer_Swap(Randomizer_Offer(sTaken), &yours[sCursor]);
            sSwapped = 1;
            sStep = STEP_OFF;
            break;

        case STEP_WHOSE:
            if (sMon != sCursor) {
                sNumLearn = 0;
            }
            sMon = sCursor;
            sCursor = 0;
            sStep = STEP_WHICH;
            break;

        case STEP_WHICH:
            // The moves on offer stay the same for the Pokemon, whichever is replaced
            if (sNumLearn == 0) {
                Randomizer_MakeLearn(&yours[sMon]);
            }
            if (sNumLearn == 0) {
                return;
            }
            sMove = sCursor;
            sCursor = 0;
            sStep = STEP_LEARN;
            break;

        case STEP_LEARN:
            mon = &yours[sMon];
            mon->unk_09[sMove] = sLearn[sCursor];
            mon->unk_20[sMove] = MAX_PP_UPS;
            Randomizer_Heal(mon);
            sSwapped = 1;
            sStep = STEP_OFF;
            break;
    }
    func_80048B90(2);
}

static void Randomizer_PanelB(void) {
    switch (sStep) {
        case STEP_THEIRS:
            if (sRogue) {
                sCursor = REWARD_POKEMON;
                sStep = STEP_REWARD;
            } else {
                sStep = STEP_OFF;
            }
            break;
        case STEP_YOURS:
            sCursor = sTaken;
            sStep = STEP_THEIRS;
            break;
        case STEP_WHOSE:
            sCursor = REWARD_MOVE;
            sStep = STEP_REWARD;
            break;
        case STEP_WHICH:
            sCursor = sMon;
            sStep = STEP_WHOSE;
            break;
        case STEP_LEARN:
            sCursor = sMove;
            sStep = STEP_WHICH;
            break;
        default:
            sStep = STEP_OFF;
            break;
    }
    func_80048B90(3);
}

static void Randomizer_PanelInput(Controller* cont) {
    s32 max = Randomizer_StepLines();
    s32 grid = (sStep == STEP_YOURS) || (sStep == STEP_WHOSE);

    if (BTN_IS_PRESSED(cont, BTN_DLEFT) || (!grid && BTN_IS_PRESSED(cont, BTN_DUP))) {
        sCursor = (sCursor + max - 1) % max;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT) || (!grid && BTN_IS_PRESSED(cont, BTN_DDOWN))) {
        sCursor = (sCursor + 1) % max;
        func_80048B90(1);
    } else if (grid && BTN_IS_PRESSED(cont, BTN_DUP | BTN_DDOWN)) {
        if (max > COLUMNS) {
            sCursor = (sCursor + COLUMNS) % (COLUMNS * 2);
            if (sCursor >= max) {
                sCursor = max - 1;
            }
            func_80048B90(1);
        }
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        Randomizer_PanelA();
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        Randomizer_PanelB();
    }
}

// After a win, before the menu for the next battle, and again when that menu's swap
// line is picked (func_84B022A0)
static void Randomizer_AfterWin(void) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    s32 i;

    if ((Randomizer_Mode() == RANDOMIZER_MODE_NORMAL) || (trainer == NULL) || !(trainer->unk_000 & 2) || sSwapped) {
        Randomizer_SetMenuLine(0);
        return;
    }

    sRogue = (Randomizer_Mode() == RANDOMIZER_MODE_ROGUE);
    if (sRogue) {
        // A reward: the Pokemon and the moves on offer are made the first time they're
        // looked at, and stay the same for this win
        sStep = STEP_REWARD;
    } else {
        // The three it battled with: having lost, they're the ones that fainted. The rest
        // weren't sent out.
        sNumTheirs = 0;
        for (i = 0; i < trainer->unk_002; i++) {
            if (trainer->unk_01C[i].unk_02 == 0) {
                sTheirs[sNumTheirs++] = i;
            }
        }
        if (sNumTheirs == 0) {
            Randomizer_SetMenuLine(0);
            return;
        }
        sStep = STEP_THEIRS;
    }

    // Freed with the rest of this part of the screen
    sMoveNames = func_8002D5AC(MOVE_NAMES_ARCHIVE);
    sCursor = 0;
    func_80048B90(4);

    while (sStep != STEP_OFF) {
        func_84B01AA0();
        Randomizer_PanelInput(gPlayer1Controller);
    }
    sMoveNames = NULL;

    // Kept their team: the menu offers the swap again
    Randomizer_SetMenuLine(!sSwapped);
}

// After a loss, before the menu for what to do next
static s32 Randomizer_RunOver(void) {
    s32 frontier = RANDOMIZER_FRONTIER();

    if ((Randomizer_Mode() != RANDOMIZER_MODE_ROGUE) && (frontier == FRONTIER_NONE)) {
        return 0;
    }
    sRunKind = frontier;
    if (frontier != FRONTIER_NONE) {
        sBeaten = (FRONTIER_LAP(gRandomizerState.frontier) * MAX_ROUND) + D_800AE540.unk_0003 - 1;
        // Over: the player's own options back as the next screen with them starts
        gRandomizerState.frontier = FRONTIER_RESTORE;
    }

    sStep = STEP_RUN_OVER;
    do {
        func_84B01AA0();
    } while (!BTN_IS_PRESSED(gPlayer1Controller, BTN_A));
    sStep = STEP_OFF;
    func_80048B90(2);

    D_800AE540.unk_11F6 |= CUP_QUIT;
    return 1;
}

// func_84B01994, unchanged: after the Prime Cup's Master Ball in Round 2, a Pikachu on the
// team that knows Surf (and came from a Game Boy game) unlocks Surfing Pikachu
static void Randomizer_PikachuCheck(void) {
    s32 i;
    s32 j;
    unk_D_800AE540_0004* var_a1;
    unk_D_800AE540_0874* temp_v1;

    if (!(D_800AE540.unk_11F5 & 2) && (D_800AE540.unk_11F2 == 1) && (D_800AE540.unk_0000 == 6) &&
        (D_800AE540.unk_0002 == 3)) {
        var_a1 = D_800AE540.unk_1194[0].unk_08[0];
        temp_v1 = var_a1->unk_214;

        for (i = 0; i < temp_v1->unk_002; i++) {
            s32 tmp = (temp_v1->unk_028[i].unk_52 & 0x70) >> 4;
            s32 tmp2 = temp_v1->unk_028[i].unk_52 & 0xF;

            if ((tmp >= 4) || (tmp2 >= 13)) {
                return;
            }
        }

        for (i = 0; i < var_a1->unk_002; i++) {
            unk_func_80026268_arg0* ptr = &var_a1->unk_01C[i];

            if (ptr->unk_00.unk_00 == 0x19) {
                for (j = 0; j < 4; j++) {
                    if (ptr->unk_09[j] == 0) {
                        break;
                    } else if (ptr->unk_09[j] == 0x39) {
                        return;
                    }
                }
                D_800AE540.unk_11F6 |= 0x1000;
                break;
            }
        }
    }
}

/*
 * func_84B022A0, the "Keep battling?" menu after a win: 0 to battle on, 1 to quit (saving the
 * cup to continue). Factory and Rogue first swap one of the team for one the trainer battled
 * with, and again when the menu's "Swap a Pokemon" line is picked; a Battle Frontier run
 * isn't saved to continue: quitting ends it.
 */
static s32 Randomizer_KeepBattling(void) {
    s32 var_s2;
    s32 var_v0_2;

    do {
        Randomizer_AfterWin();

        if (D_800AE540.unk_0000 == 7) {
            func_84B0F950(&D_84B26640.unk_1C, 2);
        } else {
            func_84B0F950(&D_84B26640.unk_1C, 1);
        }

        var_s2 = func_84B0FADC(&D_84B26640.unk_1C);
        while (var_s2 == -1) {
            func_84B01AA0();
            var_s2 = func_84B0FADC(&D_84B26640.unk_1C);
        }
    } while (var_s2 == RANDOMIZER_CUP_SWAP_ITEM);

    if ((var_s2 == 1) && (RANDOMIZER_FRONTIER() == FRONTIER_NONE)) {
        if (func_8002847C() != 0) {
            func_84B0F950(&D_84B26640.unk_1C, 0xA);

            var_v0_2 = func_84B0FADC(&D_84B26640.unk_1C);
            while (var_v0_2 == -1) {
                func_84B01AA0();
                var_v0_2 = func_84B0FADC(&D_84B26640.unk_1C);
            }

            if (var_v0_2 == 1) {
                var_s2 = 0;
            }
        }
        if (var_s2 == 1) {
            D_800AE540.unk_0003 = D_800AE540.unk_0003 + 1;
            func_8002C538();
            func_800284B4(2);
        }
    }
    return var_s2;
}

// "Keep battling?" answered no: the run saved to continue and the title screen, as the game
// does; a Battle Frontier run, which isn't saved, ends and goes back to the menu
static u16 Randomizer_Quit(void) {
    if (RANDOMIZER_FRONTIER() == FRONTIER_NONE) {
        return CUP_QUIT | CUP_TITLE;
    }
    gRandomizerState.frontier = FRONTIER_RESTORE;
    return CUP_QUIT;
}

// A 32-bit finalizer, so each lap's seed is unrelated to the last
static u32 Randomizer_Mix(u32 x) {
    x ^= x >> 16;
    x *= 0x85EBCA6B;
    x ^= x >> 13;
    x *= 0xC2B2AE35;
    x ^= x >> 16;
    return x;
}

// After a win, the round counted up: a Battle Frontier run's goes back to 1 after the
// eighth trainer (func_8002BFC4 keeps which trainer each round had in 8 bytes, by round)
static void Randomizer_NextRound(void) {
    RandomizerState* state = &gRandomizerState;
    s32 lap;

    if ((RANDOMIZER_FRONTIER() == FRONTIER_NONE) || (D_800AE540.unk_0003 <= MAX_ROUND)) {
        return;
    }
    D_800AE540.unk_0003 = 1;

    // The next ball's trainers, up to the Master Ball's, in the cups that have four
    if (((D_800AE540.unk_0000 == POKE_CUP_MODE) || (D_800AE540.unk_0000 == PRIME_CUP_MODE)) &&
        (D_800AE540.unk_0002 < MASTER_BALL)) {
        D_800AE540.unk_0002++;
    }

    // The run's counter, and the opponents' seed mixed with it
    lap = FRONTIER_LAP(state->frontier);
    if (lap < (FRONTIER_LAP_MASK >> FRONTIER_LAP_SHIFT)) {
        lap++;
    }
    state->frontier = (state->frontier & ~FRONTIER_LAP_MASK) | (lap << FRONTIER_LAP_SHIFT);
    state->opponentSeed = Randomizer_Mix(state->opponentSeed + (lap * 0x9E3779B9));
}

static RandomizerCupHooks sHooks = {
    Randomizer_CupDraw,      Randomizer_AfterWin, Randomizer_RunOver,   Randomizer_PikachuCheck,
    Randomizer_KeepBattling, Randomizer_Quit,     Randomizer_NextRound,
};

// Run by the cup screen once it has loaded this fragment (func_84B03194)
void Randomizer_CupEntry(void) {
    sStep = STEP_OFF;
    sSwapped = 0;
    sMenuSaved = 0;
    sMoveNames = NULL;
    sRogue = 0;
    sNumTheirs = 0;
    sNumLearn = 0;
    sMon = -1;
    sUnpacked = 0;
    sBeaten = 0;
    sRunKind = FRONTIER_NONE;
    // Even before the pick screen has set the rest up (a cup resumed after power-on):
    // it's only this, and the hooks check the rest is valid
    gRandomizerState.cupHooks = &sHooks;

    // A Battle Frontier run, its first time here: the mode of the cup left out it came with
    // (which skipped the ball select and showed Rogue's or Factory's picture) becomes its
    // cup's, whose trainers it battles; for these cups the mode is the rule set
    if ((RANDOMIZER_FRONTIER() != FRONTIER_NONE) &&
        ((D_800AE540.unk_0000 == ROGUE_SLOT_MODE) || (D_800AE540.unk_0000 == FACTORY_SLOT_MODE))) {
        D_800AE540.unk_0000 = D_800AE540.unk_0001;
    }
}

#endif

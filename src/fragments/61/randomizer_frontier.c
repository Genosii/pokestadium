/*
 * The Battle Frontier's team screens, in place of the pick screen's "Choose from where?"
 * and rental list during a run (Randomizer_FrontierPick, called by func_84203E6C). The run's
 * mode is picked on the cup screen (randomizer_stadium.c), and Battle comes here:
 *   - "Choose your rules" first (randomizer_panel.c), over the mode's picture. B goes back to
 *     Battle, Rules and Registration; Start offers the Pokemon, made with those rules;
 *   - Factory: six Pokemon in a row of portraits, the one highlighted's moves and stats under
 *     them. A asks to select it (or return it), and once three are, "Rent these 3?" shows the
 *     three with their moves and stats;
 *   - Rogue: three teams of three, one at a time between the others (Left and Right), the
 *     moves and stats of its three under them. A asks to choose it.
 * B goes back to the rules, and Start offers new ones. The portraits are the game's own,
 * rendered from the Pokemon's models (func_8001A024), as the screens around a cup's battles
 * have them. The team goes in as the game enters a picked team (func_84202EB0), as rentals:
 * registered Pokemon can't be used.
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "randomizer_build.h"
#include "src/19840.h"
#include "src/1AB70.h"
#include "src/1CF30.h"
#include "src/20470.h"
#include "src/232C0.h"
#include "src/2C1C0.h"
#include "src/49790.h"
#include "src/6BC0.h"
#include "src/DDC0.h"
#include "src/controller.h"
#include "src/hal_libc.h"
#include "src/memory.h"
#include "src/stage_loader.h"

// In fragment61
extern void* D_84211B34;                 // the background: the mode's picture (D_84210D4C)
extern unk_D_842168A0_0013C* D_84211B38; // the rentals of the run's rule set, the Poke Cup's
extern unk_D_8423D3D8 D_8423D3D8[4];     // the players picking a team (func_84202C20)

typedef struct unk_D_842115F0 {
    /* 0x00 */ Color_RGB8 unk_00[4];
    /* 0x0C */ u8* unk_0C;
} unk_D_842115F0; // size = 0x10
extern unk_D_842115F0 D_842115F0[];

#define FACTORY_OFFERS 6
#define ROGUE_TEAMS 3
#define TEAM_SIZE 3
#define MAX_OFFERS (ROGUE_TEAMS * TEAM_SIZE)

// As func_84209DB8 marks a rental the player picks: picked from the rental list (source 4)
#define RENTAL_LIST 0xD
#define RENTALS_SOURCE 4
#define NOT_A_RENTAL 0xFF

// The title's strip at the top
#define TITLE_X 40
#define TITLE_Y 8
#define TITLE_W 560
#define TITLE_H 40

// The portraits, tall as those after a cup's battle, each in a window 4 wider on each side.
// Rogue's team in the middle, and the other two at half size at its sides; Factory's six in a
// row.
#define FRAME 4
#define PORTRAITS_Y 56
#define ROGUE_PORTRAIT_W 96
#define ROGUE_PORTRAIT_H 144
#define ROGUE_PORTRAIT_X(i) (166 + (i) * 104)
#define ROGUE_SIDE_X(side, i) (((side) == 0 ? 8 : 480) + (i) * 52)
#define ROGUE_SIDE_Y (PORTRAITS_Y + ROGUE_PORTRAIT_H / 4)
#define FACTORY_PORTRAIT_W 88
#define FACTORY_PORTRAIT_H 132
#define FACTORY_PORTRAIT_X(i) (44 + (i) * 94)
// Frames from a portrait's drawing to the next model's loading
#define PORTRAIT_WAIT 2
// Rows of a portrait loaded at a time: texture memory holds 4 KB (and even, for half size)
#define STRIP_ROWS 20

// A Pokemon's wide box, one under another: its name and level, its stats, and its moves
// under them
#define BOX_X 40
#define BOX_W 560
#define BOX_H 70
#define BOX_Y(top, i) ((top) + (i) * 74)
#define ROGUE_BOXES_Y 208
#define FACTORY_BOXES_Y 200
#define BOX_NAME_X (BOX_X + 16)
#define BOX_LEVEL_X (BOX_X + 144)
#define BOX_STATS_X (BOX_X + 190)
#define BOX_STAT_W 72
#define BOX_STAT_VALUE_X 32
#define BOX_LINE_Y 12
#define BOX_MOVES_Y 42
#define BOX_MOVE_X(j) (BOX_X + 12 + (j) * 134)

// Factory's box for the Pokemon highlighted, in the big font: moves on the left, stats on the
// right; and its question beside it
#define INFO_X 40
#define INFO_Y 200
#define INFO_W 370
#define INFO_H 196
#define INFO_LINE 28
#define INFO_MOVES_Y 44
#define INFO_STATS_X (INFO_X + 240)
#define INFO_STATS_RIGHT (INFO_X + 350)
#define ASK_X 420
#define ASK_Y 200
#define ASK_W 180
#define ASK_H 144

// The question under the wide boxes, "Rent these 3?" and "Choose this team?"
#define BAR_X 120
#define BAR_Y 430
#define BAR_W 400
#define BAR_H 40

#define ICON_SIZE 0x14
#define MOVE_TEXT_X (ICON_SIZE + 6)
#define NUM_MOVES 4
#define NUM_STATS 5

#define BIG_FONT 0x10
#define SMALL_FONT 8

// The windows' colours, as the pick screen's (0x2121), lighter for the cursor and green for
// the Pokemon selected
#define WINDOW_COLOR 0x2121
#define CURSOR_COLOR 0x42B1
#define MARKED_COLOR 0x2391

#define MOVE_TYPE(move) (D_80072338[(move) - 1].unk_01)

enum { SCREEN_RULES, SCREEN_TEAMS, SCREEN_CONFIRM };
enum { ANSWER_YES, ANSWER_NO };

static const char* sStatNames[NUM_STATS] = { "HP", "Atk", "Def", "Spe", "Spc" };

static unk_func_80026268_arg0 sOffers[MAX_OFFERS];
static s32 sNumOffers; // 0 when no team fits the rules
static s32 sCursor;    // Factory: the Pokemon highlighted; Rogue: the team shown
static u8 sMarked[TEAM_SIZE]; // Factory: the Pokemon selected, in order
static s32 sNumMarked;
static s32 sAsking; // a Yes/No question is open
static s32 sAnswer;
static unk_func_8001A024* sPortraits;

static s32 Randomizer_RentalIndex(RandomizerRentalList* rentals, s32 species) {
    u32 i;

    for (i = 0; i < rentals->count; i++) {
        if (rentals->mons[i].unk_00.unk_00 == species) {
            return i;
        }
    }
    return NOT_A_RENTAL;
}

static s32 Randomizer_HasSpecies(RandomizerMon* team, s32 count, s32 species) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (team[i].species == species) {
            return 1;
        }
    }
    return 0;
}

/*
 * The Pokemon offered, made with the run's rules (Randomizer_FrontierBegin put them in the
 * settings) and the Poke Cup's, from rentals as Z's random team is (Randomizer_FillTeam):
 * Factory's six are one team; Rogue's nine are one team and three of a second, those not in
 * the first where it has them. Their portraits are rendered over the next frames.
 */
static void Randomizer_FrontierOffer(s32 kind) {
    RandomizerRentalList* rentals = (RandomizerRentalList*)D_84211B38;
    RandomizerState* state = Randomizer_State();
    RandomizerMon mons[2][RANDOMIZER_TEAM_SIZE];
    RandomizerMon offers[MAX_OFFERS];
    RandomizerRules rules;
    s32 count = 0;
    s32 i;
    s32 pass;

    func_8001A258(sPortraits);
    sNumOffers = 0;
    sCursor = 0;
    sNumMarked = 0;
    sAsking = 0;
    if ((rentals == NULL) || (rentals->count == 0)) {
        return;
    }
    Randomizer_GetRules(D_800AE540.unk_0001, rentals->mons[0].unk_24, &rules);
    Randomizer_UseRentals(&rules, rentals);
    Randomizer_Seed(osGetCount());

    if (!Randomizer_GenerateTeam(&state->settings, &rules, mons[0])) {
        return;
    }
    for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
        offers[count++] = mons[0][i];
    }
    if (kind == FRONTIER_ROGUE) {
        if (!Randomizer_GenerateTeam(&state->settings, &rules, mons[1])) {
            return;
        }
        // Those the first team doesn't have, then the others if too few
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; (i < RANDOMIZER_TEAM_SIZE) && (count < MAX_OFFERS); i++) {
                if (Randomizer_HasSpecies(offers, RANDOMIZER_TEAM_SIZE, mons[1][i].species) == pass) {
                    offers[count++] = mons[1][i];
                }
            }
        }
    }

    for (i = 0; i < count; i++) {
        unk_func_80026268_arg0* mon = &sOffers[i];

        // The website can go over the cup's top level when its filters leave too few
        // Pokemon; the game wouldn't accept that
        if ((rules.levelMax != 0) && (offers[i].level > rules.levelMax)) {
            offers[i].level = rules.levelMax;
        }
        Randomizer_BuildPokemon(mon, &offers[i], &rentals->mons[0]);
        mon->unk_53 = Randomizer_RentalIndex(rentals, offers[i].species);
        mon->unk_52 = (RENTALS_SOURCE << 4) | RENTAL_LIST;
        func_8001A46C(sPortraits, i, mon, 0);
    }
    sNumOffers = count;
}

// The team, entered as func_84202EB0 enters one from the rentals: no trainer ID, the
// rental-only trainer's name
static void Randomizer_FrontierEnter(unk_D_8423D3D8* player, unk_func_80026268_arg0** team) {
    unk_D_800AE540_0004* entry = player->unk_04;
    s32 i;

    entry->unk_002 = 0;
    entry->unk_018 = 0;
    if (D_800AE540.unk_0000 != 0) {
        HAL_Strcpy(entry->unk_008, func_8002311C(1));
    }
    HAL_Strcpy(entry->unk_214->unk_014, func_8002311C(1));
    for (i = 0; i < TEAM_SIZE; i++) {
        func_8002B888(entry, team[i]);
    }
}

// Factory: the place of a Pokemon among those selected, or -1
static s32 Randomizer_MarkIndex(s32 offer) {
    s32 i;

    for (i = 0; i < sNumMarked; i++) {
        if (sMarked[i] == offer) {
            return i;
        }
    }
    return -1;
}

static void Randomizer_ToggleMark(s32 offer) {
    s32 mark = Randomizer_MarkIndex(offer);
    s32 i;

    if (mark >= 0) {
        // Returned, and those selected after it move up
        for (i = mark; i < sNumMarked - 1; i++) {
            sMarked[i] = sMarked[i + 1];
        }
        sNumMarked--;
    } else if (sNumMarked < TEAM_SIZE) {
        sMarked[sNumMarked++] = offer;
    }
}

// The team picked: Factory's three selected, or Rogue's team shown
static void Randomizer_PickedTeam(s32 kind, unk_func_80026268_arg0** team) {
    s32 i;

    for (i = 0; i < TEAM_SIZE; i++) {
        team[i] = &sOffers[(kind == FRONTIER_ROGUE) ? (sCursor * TEAM_SIZE + i) : sMarked[i]];
    }
}

// A Yes/No question's input: Yes or No (A, or B for No) once answered, -1 until then
static s32 Randomizer_AskInput(Controller* cont, u16 dpad) {
    if (dpad & RANDOMIZER_DPAD) {
        sAnswer ^= 1;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        func_80048B90((sAnswer == ANSWER_YES) ? 2 : 3);
        return sAnswer;
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        func_80048B90(3);
        return ANSWER_NO;
    }
    return -1;
}

static void Randomizer_Ask(void) {
    sAsking = 1;
    sAnswer = ANSWER_YES;
}

/*
 * The team screens' input: 1 once a team is picked (into team), -1 for B back to the rules,
 * 0 otherwise. *screen goes between the team screen and Factory's "Rent these 3?".
 */
static s32 Randomizer_FrontierTeamsInput(s32 kind, Controller* cont, s32* screen,
                                         unk_func_80026268_arg0** team) {
    u16 dpad = Randomizer_Repeat(cont, RANDOMIZER_DPAD);
    s32 count = (kind == FRONTIER_ROGUE) ? ROGUE_TEAMS : FACTORY_OFFERS;
    s32 answer;

    if (sAsking) {
        answer = Randomizer_AskInput(cont, dpad);
        if (answer < 0) {
            return 0;
        }
        sAsking = 0;
        if (*screen == SCREEN_CONFIRM) {
            if (answer == ANSWER_YES) {
                Randomizer_PickedTeam(kind, team);
                return 1;
            }
            *screen = SCREEN_TEAMS;
        } else if (answer == ANSWER_YES) {
            if (kind == FRONTIER_ROGUE) {
                Randomizer_PickedTeam(kind, team);
                return 1;
            }
            Randomizer_ToggleMark(sCursor);
            if (sNumMarked == TEAM_SIZE) {
                *screen = SCREEN_CONFIRM;
                Randomizer_Ask();
            }
        }
        return 0;
    }

    if (BTN_IS_PRESSED(cont, BTN_B)) {
        func_80048B90(3);
        return -1;
    }
    if (sNumOffers == 0) {
        return 0;
    }
    if (dpad & (BTN_DLEFT | BTN_DRIGHT)) {
        sCursor = (sCursor + ((dpad & BTN_DLEFT) ? count - 1 : 1)) % count;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        if ((kind == FRONTIER_FACTORY) && (sNumMarked == TEAM_SIZE) && (Randomizer_MarkIndex(sCursor) < 0)) {
            // Three are selected already
            func_80048B90(8);
        } else {
            func_80048B90(2);
            Randomizer_Ask();
        }
    } else if (BTN_IS_PRESSED(cont, BTN_START) && (kind == FRONTIER_FACTORY) && (sNumMarked == TEAM_SIZE)) {
        func_80048B90(2);
        *screen = SCREEN_CONFIRM;
        Randomizer_Ask();
    }
    return 0;
}

/*
 * A portrait, once rendered, a strip at a time as texture memory holds 4 KB; at half size
 * when small.
 */
static void Randomizer_PortraitDraw(s32 offer, s32 x, s32 y, s32 w, s32 h, s32 small) {
    u16* image = (u16*)func_8001AFD8(sPortraits, offer);
    s32 step = small ? 0x800 : 0x400;
    s32 shift = small ? 1 : 0;
    s32 rows;
    s32 i;

    for (i = 0; i < h; i += STRIP_ROWS) {
        rows = ((h - i) < STRIP_ROWS) ? (h - i) : STRIP_ROWS;
        gDPLoadTextureBlock(gDisplayListHead++, image + (i * w), G_IM_FMT_RGBA, G_IM_SIZ_16b, w, rows, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        func_8001C330(x, y + (i >> shift), w >> shift, rows >> shift, 0, 0, step, step, 0);
    }
}

// A see-through dark box over what's not picked
static void Randomizer_Dim(s32 x, s32 y, s32 w, s32 h) {
    gSPDisplayList(gDisplayListHead++, D_8006F498);
    func_8001C604(x, y, w, h, 0, 0, 0, 0x90);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

static void Randomizer_IconsDraw(unk_func_80026268_arg0* mon, s32 x, s32 y, s32 dx, s32 dy) {
    s32 j;

    for (j = 0; (j < NUM_MOVES) && (mon->unk_09[j] != 0); j++) {
        func_8001C6AC(x + (j * dx), y + (j * dy), ICON_SIZE, ICON_SIZE, D_842115F0[MOVE_TYPE(mon->unk_09[j])].unk_0C,
                      ICON_SIZE, 0);
    }
}

static void Randomizer_MoveColor(u8 move) {
    Color_RGB8* color = &D_842115F0[MOVE_TYPE(move)].unk_00[2];

    func_8001F324(color->r, color->g, color->b, 0xFF);
}

// A wide box's text: the name in the big font, the level and stats beside it, the moves under
static void Randomizer_BoxText(unk_func_80026268_arg0* mon, s32 y) {
    s32 j;

    func_8001EBE0(BIG_FONT, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(BOX_NAME_X, y + 8, "%s", mon->unk_30);
    func_8001EBE0(SMALL_FONT, 0);
    func_8001F1E8(BOX_LEVEL_X, y + BOX_LINE_Y, "%s %d", func_84200160(0x15), mon->unk_24);

    // Max HP, Attack, Defense, Speed and Special follow one another (unk_26 to unk_2E)
    for (j = 0; j < NUM_STATS; j++) {
        func_8001F324(0x9C, 0x9C, 0x9C, 0xFF);
        func_8001F1E8(BOX_STATS_X + (j * BOX_STAT_W), y + BOX_LINE_Y, "%s", sStatNames[j]);
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F1E8(BOX_STATS_X + (j * BOX_STAT_W) + BOX_STAT_VALUE_X, y + BOX_LINE_Y, "%d", (&mon->unk_26)[j]);
    }

    for (j = 0; (j < NUM_MOVES) && (mon->unk_09[j] != 0); j++) {
        Randomizer_MoveColor(mon->unk_09[j]);
        func_8001F1E8(BOX_MOVE_X(j) + MOVE_TEXT_X, y + BOX_MOVES_Y, "%s", func_842000C0(mon->unk_09[j]));
    }
}

// Factory's box for the Pokemon highlighted, all in the big font
static void Randomizer_InfoText(unk_func_80026268_arg0* mon) {
    s32 y = INFO_Y + INFO_MOVES_Y;
    s32 j;

    func_8001EBE0(BIG_FONT, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(INFO_X + 16, INFO_Y + 10, "%s", mon->unk_30);
    func_8001F1E8(INFO_STATS_RIGHT - func_8001F5B0(0, 0, "%s %d", func_84200160(0x15), mon->unk_24), INFO_Y + 10,
                  "%s %d", func_84200160(0x15), mon->unk_24);

    for (j = 0; (j < NUM_MOVES) && (mon->unk_09[j] != 0); j++) {
        Randomizer_MoveColor(mon->unk_09[j]);
        func_8001F1E8(INFO_X + 16 + MOVE_TEXT_X, y + (j * INFO_LINE), "%s", func_842000C0(mon->unk_09[j]));
    }
    for (j = 0; j < NUM_STATS; j++) {
        func_8001F324(0x9C, 0x9C, 0x9C, 0xFF);
        func_8001F1E8(INFO_STATS_X, y + (j * INFO_LINE), "%s", sStatNames[j]);
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F1E8(INFO_STATS_RIGHT - func_8001F5B0(0, 0, "%d", (&mon->unk_26)[j]), y + (j * INFO_LINE), "%d",
                      (&mon->unk_26)[j]);
    }
}

// Yes and No, the answer on the cursor in yellow
static void Randomizer_AnswersText(s32 x, s32 y, s32 dx, s32 dy) {
    s32 i;

    for (i = ANSWER_YES; i <= ANSWER_NO; i++) {
        if (i == sAnswer) {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        } else {
            func_8001F324(0x9C, 0x9C, 0x9C, 0xFF);
        }
        func_8001F1E8(x + (i * dx), y + (i * dy), "%s", (i == ANSWER_YES) ? "Yes" : "No");
    }
}

static void Randomizer_TitleText(const char* title, const char* hint) {
    func_8001EBE0(BIG_FONT, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(TITLE_X + 16, TITLE_Y + 10, "%s", title);
    func_8001EBE0(SMALL_FONT, 0);
    func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
    func_8001F1E8(TITLE_X + TITLE_W - 16 - func_8001F5B0(0, 0, "%s", hint), TITLE_Y + 14, "%s", hint);
}

// The question under the wide boxes
static void Randomizer_BarText(const char* question) {
    func_8001EBE0(BIG_FONT, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(BAR_X + 16, BAR_Y + 10, "%s", question);
    Randomizer_AnswersText(BAR_X + BAR_W - 120, BAR_Y + 10, 64, 0);
}

static void Randomizer_RogueDraw(void) {
    char title[0x20];
    s32 side;
    s32 team;
    s32 i;

    // The windows first, which would cover what's drawn before them
    func_80020460(TITLE_X, TITLE_Y, TITLE_W, TITLE_H, WINDOW_COLOR);
    if (sNumOffers != 0) {
        for (side = 0; side < 2; side++) {
            for (i = 0; i < TEAM_SIZE; i++) {
                func_80020460(ROGUE_SIDE_X(side, i) - FRAME, ROGUE_SIDE_Y - FRAME, (ROGUE_PORTRAIT_W / 2) + 2 * FRAME,
                              (ROGUE_PORTRAIT_H / 2) + 2 * FRAME, WINDOW_COLOR);
            }
        }
        for (i = 0; i < TEAM_SIZE; i++) {
            func_80020460(ROGUE_PORTRAIT_X(i) - FRAME, PORTRAITS_Y - FRAME, ROGUE_PORTRAIT_W + 2 * FRAME,
                          ROGUE_PORTRAIT_H + 2 * FRAME, CURSOR_COLOR);
            func_80020460(BOX_X, BOX_Y(ROGUE_BOXES_Y, i), BOX_W, BOX_H, WINDOW_COLOR);
        }
        if (sAsking) {
            func_80020460(BAR_X, BAR_Y, BAR_W, BAR_H, CURSOR_COLOR);
        }

        // The team shown, and the one before it at the left and the one after at the right,
        // half size and darker
        gSPDisplayList(gDisplayListHead++, D_8006F518);
        for (side = 0; side < 2; side++) {
            team = (sCursor + ((side == 0) ? ROGUE_TEAMS - 1 : 1)) % ROGUE_TEAMS;
            for (i = 0; i < TEAM_SIZE; i++) {
                Randomizer_PortraitDraw(team * TEAM_SIZE + i, ROGUE_SIDE_X(side, i), ROGUE_SIDE_Y, ROGUE_PORTRAIT_W,
                                        ROGUE_PORTRAIT_H, 1);
            }
        }
        for (i = 0; i < TEAM_SIZE; i++) {
            Randomizer_PortraitDraw(sCursor * TEAM_SIZE + i, ROGUE_PORTRAIT_X(i), PORTRAITS_Y, ROGUE_PORTRAIT_W,
                                    ROGUE_PORTRAIT_H, 0);
        }
        gSPDisplayList(gDisplayListHead++, D_8006F630);
        for (side = 0; side < 2; side++) {
            Randomizer_Dim(ROGUE_SIDE_X(side, 0) - FRAME, ROGUE_SIDE_Y - FRAME,
                           ROGUE_SIDE_X(side, TEAM_SIZE - 1) - ROGUE_SIDE_X(side, 0) + (ROGUE_PORTRAIT_W / 2) +
                               2 * FRAME,
                           (ROGUE_PORTRAIT_H / 2) + 2 * FRAME);
        }
    }

    func_8001F3F4();
    sprintf(title, "Team %d of %d", sCursor + 1, ROGUE_TEAMS);
    Randomizer_TitleText((sNumOffers != 0) ? title : "No team fits these rules",
                         "Left/Right: Teams   A: Choose   B: Rules");
    for (i = 0; (i < TEAM_SIZE) && (sNumOffers != 0); i++) {
        Randomizer_BoxText(&sOffers[sCursor * TEAM_SIZE + i], BOX_Y(ROGUE_BOXES_Y, i));
    }
    if (sAsking) {
        Randomizer_BarText("Choose this team?");
    }
    func_8001F444();

    // The type icons, as the summary draws them next to the moves
    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; (i < TEAM_SIZE) && (sNumOffers != 0); i++) {
        Randomizer_IconsDraw(&sOffers[sCursor * TEAM_SIZE + i], BOX_MOVE_X(0), BOX_Y(ROGUE_BOXES_Y, i) + BOX_MOVES_Y - 2,
                             BOX_MOVE_X(1) - BOX_MOVE_X(0), 0);
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

static void Randomizer_FactoryDraw(s32 screen) {
    s32 confirm = screen == SCREEN_CONFIRM;
    unk_func_80026268_arg0* mon;
    s32 color;
    s32 mark;
    s32 i;

    // The windows first, which would cover what's drawn before them
    func_80020460(TITLE_X, TITLE_Y, TITLE_W, TITLE_H, WINDOW_COLOR);
    for (i = 0; i < sNumOffers; i++) {
        if (Randomizer_MarkIndex(i) >= 0) {
            color = MARKED_COLOR;
        } else if ((i == sCursor) && !confirm) {
            color = CURSOR_COLOR;
        } else {
            color = WINDOW_COLOR;
        }
        func_80020460(FACTORY_PORTRAIT_X(i) - FRAME, PORTRAITS_Y - FRAME, FACTORY_PORTRAIT_W + 2 * FRAME,
                      FACTORY_PORTRAIT_H + 2 * FRAME, color);
    }
    if (confirm) {
        for (i = 0; i < TEAM_SIZE; i++) {
            func_80020460(BOX_X, BOX_Y(FACTORY_BOXES_Y, i), BOX_W, BOX_H, WINDOW_COLOR);
        }
        func_80020460(BAR_X, BAR_Y, BAR_W, BAR_H, CURSOR_COLOR);
    } else if (sNumOffers != 0) {
        func_80020460(INFO_X, INFO_Y, INFO_W, INFO_H, WINDOW_COLOR);
        if (sAsking) {
            func_80020460(ASK_X, ASK_Y, ASK_W, ASK_H, CURSOR_COLOR);
        }
    }

    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; i < sNumOffers; i++) {
        Randomizer_PortraitDraw(i, FACTORY_PORTRAIT_X(i), PORTRAITS_Y, FACTORY_PORTRAIT_W, FACTORY_PORTRAIT_H, 0);
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);
    // "Rent these 3?": the others darker
    for (i = 0; (i < sNumOffers) && confirm; i++) {
        if (Randomizer_MarkIndex(i) < 0) {
            Randomizer_Dim(FACTORY_PORTRAIT_X(i) - FRAME, PORTRAITS_Y - FRAME, FACTORY_PORTRAIT_W + 2 * FRAME,
                           FACTORY_PORTRAIT_H + 2 * FRAME);
        }
    }

    func_8001F3F4();
    if (sNumOffers == 0) {
        Randomizer_TitleText("No team fits these rules", "B: Rules");
    } else if (confirm) {
        Randomizer_TitleText("Your rental team", "A: Answer   B: Back");
    } else {
        Randomizer_TitleText("Rent 3 of these 6", "Left/Right: Pok\xE9mon   A: Select   B: Rules");
    }
    // The order they were selected in, in yellow, on their portraits
    func_8001EBE0(BIG_FONT, 0);
    func_8001F324(0xFF, 0xFF, 0, 0xFF);
    for (i = 0; i < sNumOffers; i++) {
        mark = Randomizer_MarkIndex(i);
        if (mark >= 0) {
            func_8001F1E8(FACTORY_PORTRAIT_X(i) + 2, PORTRAITS_Y + 2, "%d", mark + 1);
        }
    }
    if (confirm) {
        for (i = 0; i < TEAM_SIZE; i++) {
            Randomizer_BoxText(&sOffers[sMarked[i]], BOX_Y(FACTORY_BOXES_Y, i));
        }
        Randomizer_BarText("Rent these 3?");
    } else if (sNumOffers != 0) {
        mon = &sOffers[sCursor];
        Randomizer_InfoText(mon);
        if (sAsking) {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
            func_8001F1E8(ASK_X + 16, ASK_Y + 12, "%s", (Randomizer_MarkIndex(sCursor) >= 0) ? "Return" : "Select");
            func_8001F1E8(ASK_X + 16, ASK_Y + 12 + INFO_LINE, "%s?", mon->unk_30);
            Randomizer_AnswersText(ASK_X + 16, ASK_Y + 12 + 2 * INFO_LINE + 8, 0, INFO_LINE);
        }
    }
    func_8001F444();

    gSPDisplayList(gDisplayListHead++, D_8006F518);
    if (confirm) {
        for (i = 0; i < TEAM_SIZE; i++) {
            Randomizer_IconsDraw(&sOffers[sMarked[i]], BOX_MOVE_X(0), BOX_Y(FACTORY_BOXES_Y, i) + BOX_MOVES_Y - 2,
                                 BOX_MOVE_X(1) - BOX_MOVE_X(0), 0);
        }
    } else if (sNumOffers != 0) {
        Randomizer_IconsDraw(&sOffers[sCursor], INFO_X + 16, INFO_Y + INFO_MOVES_Y + 2, 0, INFO_LINE);
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// The mode's picture, faded in as func_84203100 fades the pick screen in
static void Randomizer_FrontierFadeIn(void) {
    s32 i;

    if (func_80007604() == 1) {
        func_80006C6C(8);
        for (i = 0; i < 10; i++) {
            func_800290B4();
            func_800079C4();
            func_8001D924(D_84211B34);
            func_80007778();
        }
    }
}

// A run's team, for the first player picking one: 1 once it's entered, 0 for B on the rules
static s32 Randomizer_FrontierTeams(s32 kind) {
    unk_D_8423D3D8* player = NULL;
    unk_func_80026268_arg0* team[TEAM_SIZE];
    Controller* cont;
    s32 screen = SCREEN_RULES;
    s32 wait = 0;
    s32 result;
    s32 i;

    func_8000D1F0(0x15);
    func_84202C20();
    for (i = 0; i < 4; i++) {
        if (D_8423D3D8[i].unk_00 != 0) {
            player = &D_8423D3D8[i];
            break;
        }
    }
    if (player == NULL) {
        return 0;
    }
    cont = &gControllers[player->unk_02];

    // The portraits and the model they're rendered from, freed as the screen ends (the models'
    // loader is the pick screen's, set up by func_84203E6C). Room for one model only: two, as
    // the screen before a cup's battle has (func_84B07428), don't fit with the screen's own.
    main_pool_push_state('FRNT');
    if (kind == FRONTIER_ROGUE) {
        sPortraits = func_8001A024(func_80019760(1), MAX_OFFERS, ROGUE_PORTRAIT_W, ROGUE_PORTRAIT_H);
    } else {
        sPortraits = func_8001A024(func_80019760(1), FACTORY_OFFERS, FACTORY_PORTRAIT_W, FACTORY_PORTRAIT_H);
    }

    Randomizer_FrontierFadeIn();
    Randomizer_FrontierPanelOpen(kind);

    while (TRUE) {
        func_800290B4();
        if (screen == SCREEN_RULES) {
            result = Randomizer_FrontierPanelInput(cont);
            if (result < 0) {
                break;
            }
            if (result > 0) {
                Randomizer_FrontierBegin(kind);
                Randomizer_FrontierOffer(kind);
                screen = SCREEN_TEAMS;
            }
        } else {
            result = Randomizer_FrontierTeamsInput(kind, cont, &screen, team);
            if (result < 0) {
                Randomizer_FrontierPanelOpen(kind);
                screen = SCREEN_RULES;
            } else if (result > 0) {
                break;
            }
        }
        // The portraits waiting, as the screen before a cup's battle renders its own: a
        // model loaded, then drawn into its portrait, without waiting for either. The next
        // model loads where the last one was, so not before that frame has been drawn.
        if (wait > 0) {
            wait--;
        } else if (func_8001AA18(sPortraits, 0) >= 0) {
            wait = PORTRAIT_WAIT;
        }

        func_800079C4();
        // Drawing to the screen again, after a portrait (func_84B014DC)
        func_8000699C(&gDisplayListHead, 1);
        func_8001D924(D_84211B34);
        if (screen == SCREEN_RULES) {
            Randomizer_PanelDraw();
        } else if (kind == FRONTIER_ROGUE) {
            Randomizer_RogueDraw();
        } else {
            Randomizer_FactoryDraw(screen);
        }
        func_80007778();
    }

    // Back to Battle, Rules and Registration for B, as B on "Choose from where?" goes
    func_8420305C(0);
    if (result > 0) {
        Randomizer_FrontierEnter(player, team);
    }
    main_pool_pop_state('FRNT');
    if (result < 0) {
        return 0;
    }
    func_8002B6BC();
    return 1;
}

/*
 * In place of func_84203210 in func_84203E6C, with the line before it (moved here to make
 * room for this call): a run's own team screens, else the pick screen's.
 */
s32 Randomizer_FrontierPick(void) {
    s32 kind = RANDOMIZER_FRONTIER();

    D_800AE540.unk_11F5 &= 0xFFFD;
    if (kind == FRONTIER_NONE) {
        return func_84203210();
    }
    return Randomizer_FrontierTeams(kind);
}

#endif

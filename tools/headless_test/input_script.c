/*
 * mupen64plus input plugin that plays a scripted sequence of controller inputs,
 * keyed by VI frame number, for headless testing.
 *
 * Script (path in env M64_INPUT_SCRIPT), one command per line:
 *   <frame> press <buttons> [frames]   hold buttons for [frames] frames (default 4)
 *   <frame> shot                       take a screenshot
 *   <frame> save <path>                write a savestate
 *   <frame> quit                       stop emulation
 * Buttons are joined with '+': A B Z START L R DU DD DL DR CU CD CL CR
 *
 * If M64_SHOT_DIR is set, "shot" grabs the X display into that directory with
 * ImageMagick instead of asking the video plugin (whose screenshots can come out
 * black under software OpenGL).
 */
#define M64P_PLUGIN_PROTOTYPES 1
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mupen64plus/m64p_common.h>
#include <mupen64plus/m64p_plugin.h>
#include <mupen64plus/m64p_types.h>

typedef m64p_error (*ptr_CoreDoCommand)(m64p_command, int, void *);

enum { CMD_PRESS, CMD_SHOT, CMD_SAVE, CMD_QUIT };

typedef struct {
    int frame;
    int kind;
    int duration;
    unsigned int buttons;
    char path[512];
} Cmd;

static Cmd cmds[4096];
static int ncmds;
static int next_cmd;
static int frame;
static unsigned int held;
static int held_until = -1;
static ptr_CoreDoCommand CoreDoCommand;
static CONTROL *controls;

static unsigned int parse_buttons(char *s) {
    unsigned int v = 0;
    BUTTONS b;
    char *tok = strtok(s, "+");
    while (tok) {
        b.Value = 0;
        if (!strcmp(tok, "A")) b.A_BUTTON = 1;
        else if (!strcmp(tok, "B")) b.B_BUTTON = 1;
        else if (!strcmp(tok, "Z")) b.Z_TRIG = 1;
        else if (!strcmp(tok, "START")) b.START_BUTTON = 1;
        else if (!strcmp(tok, "L")) b.L_TRIG = 1;
        else if (!strcmp(tok, "R")) b.R_TRIG = 1;
        else if (!strcmp(tok, "DU")) b.U_DPAD = 1;
        else if (!strcmp(tok, "DD")) b.D_DPAD = 1;
        else if (!strcmp(tok, "DL")) b.L_DPAD = 1;
        else if (!strcmp(tok, "DR")) b.R_DPAD = 1;
        else if (!strcmp(tok, "CU")) b.U_CBUTTON = 1;
        else if (!strcmp(tok, "CD")) b.D_CBUTTON = 1;
        else if (!strcmp(tok, "CL")) b.L_CBUTTON = 1;
        else if (!strcmp(tok, "CR")) b.R_CBUTTON = 1;
        else fprintf(stderr, "input-script: unknown button %s\n", tok);
        v |= b.Value;
        tok = strtok(NULL, "+");
    }
    return v;
}

static void load_script(void) {
    const char *path = getenv("M64_INPUT_SCRIPT");
    char line[1024];
    FILE *f;

    ncmds = 0;
    if (!path || !(f = fopen(path, "r"))) {
        fprintf(stderr, "input-script: no script (M64_INPUT_SCRIPT)\n");
        return;
    }
    while (fgets(line, sizeof(line), f) && ncmds < 4096) {
        Cmd *c = &cmds[ncmds];
        char verb[32] = "", arg[512] = "";
        int dur = 4;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        if (sscanf(line, "%d %31s %511s %d", &c->frame, verb, arg, &dur) < 2) continue;
        c->duration = dur;
        if (!strcmp(verb, "press")) { c->kind = CMD_PRESS; c->buttons = parse_buttons(arg); }
        else if (!strcmp(verb, "shot")) c->kind = CMD_SHOT;
        else if (!strcmp(verb, "save")) { c->kind = CMD_SAVE; strcpy(c->path, arg); }
        else if (!strcmp(verb, "quit")) c->kind = CMD_QUIT;
        else continue;
        ncmds++;
    }
    fclose(f);
    fprintf(stderr, "input-script: %d commands from %s\n", ncmds, path);
}

static void frame_callback(unsigned int index) {
    (void)index;
    frame++;
    if (frame > held_until) held = 0;
    while (next_cmd < ncmds && cmds[next_cmd].frame <= frame) {
        Cmd *c = &cmds[next_cmd++];
        switch (c->kind) {
            case CMD_PRESS:
                held = c->buttons;
                held_until = frame + c->duration - 1;
                break;
            case CMD_SHOT: {
                /* M64_SHOT_DIR set: grab the X display instead (for video plugins without screenshots) */
                const char *dir = getenv("M64_SHOT_DIR");
                if (dir) {
                    char cmd[1024];
                    snprintf(cmd, sizeof(cmd), "import -window root %s/f%06d.png", dir, frame);
                    if (system(cmd) != 0) fprintf(stderr, "input-script: capture failed\n");
                } else {
                    CoreDoCommand(M64CMD_TAKE_NEXT_SCREENSHOT, 0, NULL);
                }
                fprintf(stderr, "input-script: screenshot at frame %d\n", frame);
                break;
            }
            case CMD_SAVE:
                CoreDoCommand(M64CMD_STATE_SAVE, 1, c->path);
                fprintf(stderr, "input-script: savestate %s at frame %d\n", c->path, frame);
                break;
            case CMD_QUIT:
                fprintf(stderr, "input-script: quit at frame %d\n", frame);
                CoreDoCommand(M64CMD_STOP, 0, NULL);
                break;
        }
    }
}

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle CoreLibHandle, void *Context, void (*DebugCallback)(void *, int, const char *)) {
    (void)Context; (void)DebugCallback;
    CoreDoCommand = (ptr_CoreDoCommand)dlsym(CoreLibHandle, "CoreDoCommand");
    return CoreDoCommand ? M64ERR_SUCCESS : M64ERR_INCOMPATIBLE;
}

EXPORT m64p_error CALL PluginShutdown(void) { return M64ERR_SUCCESS; }

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities) {
    if (PluginType) *PluginType = M64PLUGIN_INPUT;
    if (PluginVersion) *PluginVersion = 0x010000;
    if (APIVersion) *APIVersion = 0x020100;
    if (PluginNamePtr) *PluginNamePtr = "Scripted input";
    if (Capabilities) *Capabilities = 0;
    return M64ERR_SUCCESS;
}

EXPORT void CALL InitiateControllers(CONTROL_INFO ControlInfo) {
    int i;
    controls = ControlInfo.Controls;
    for (i = 0; i < 4; i++) {
        controls[i].Present = (i == 0);
        controls[i].RawData = 0;
        controls[i].Plugin = PLUGIN_NONE;
        controls[i].Type = CONT_TYPE_STANDARD;
    }
}

EXPORT int CALL RomOpen(void) {
    frame = 0;
    next_cmd = 0;
    held = 0;
    held_until = -1;
    load_script();
    CoreDoCommand(M64CMD_SET_FRAME_CALLBACK, 0, (void *)frame_callback);
    return 1;
}

EXPORT void CALL RomClosed(void) {}

EXPORT void CALL GetKeys(int Control, BUTTONS *Keys) {
    Keys->Value = (Control == 0) ? held : 0;
}

EXPORT void CALL ControllerCommand(int Control, unsigned char *Command) { (void)Control; (void)Command; }
EXPORT void CALL ReadController(int Control, unsigned char *Command) { (void)Control; (void)Command; }
EXPORT void CALL SDL_KeyDown(int keymod, int keysym) { (void)keymod; (void)keysym; }
EXPORT void CALL SDL_KeyUp(int keymod, int keysym) { (void)keymod; (void)keysym; }

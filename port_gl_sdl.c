#include <kos.h>
#include "ref_gl/gl_local.h"
#include "client/keys.h"
#include "quake2.h"
#include <kos/thread.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#define MAIN_STACK_SIZE (32 * 1024)  // 64KB for main thread
#define RAM_UPDATE_INTERVAL 5000      // 5 seconds in milliseconds

static int _width = 640;
static int _height = 480;
int _old_button_state = 0;
static unsigned long systemRam = 0x00000000;
static unsigned long elfOffset = 0x8c000000;  // Dreamcast specific
static unsigned long stackSize = 0x00000000;
static unsigned long lastRamCheck = 0;

extern unsigned long end;
extern unsigned long start;

static void init_thread_stack(void) {
    kthread_t *current = thd_get_current();
    if (current) {
        void *new_stack = malloc(MAIN_STACK_SIZE);
        if (new_stack) {
            current->stack = new_stack;
            current->stack_size = MAIN_STACK_SIZE;
            current->flags |= THD_OWNS_STACK;
        }
    }
}

unsigned long getFreeRam(void) {
    struct mallinfo mi = mallinfo();
    return systemRam - (mi.usmblks + stackSize);
}

void setSystemRam(void) {
    // Dreamcast has 16MB RAM
    systemRam = 0x8d000000 - 0x8c000000;
    stackSize = (int)&end - (int)&start + ((int)&start - elfOffset);
}

unsigned long getSystemRam(void) {
    return systemRam;
}

unsigned long getUsedRam(void) {
    return (systemRam - getFreeRam());
}

void checkAndDisplayRamStatus(void) {
    unsigned long currentTime = timer_ms_gettime64();
    if (currentTime - lastRamCheck >= RAM_UPDATE_INTERVAL) {
        printf("\nRAM Status:\n");
        printf("Total: %.2f MB, Free: %.2f MB, Used: %.2f MB\n",
               getSystemRam() / (1024.0 * 1024.0),
               getFreeRam() / (1024.0 * 1024.0),
               getUsedRam() / (1024.0 * 1024.0));
        lastRamCheck = currentTime;
    }
}
// Quake 2 interface functions remain the same
qboolean GLimp_InitGL(void) { return true; }

static void setupWindow(qboolean fullscreen) { }

int GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen) {
    int width = 0;
    int height = 0;

    ri.Con_Printf(PRINT_ALL, "Initializing OpenGL display\n");
    ri.Con_Printf(PRINT_ALL, "...setting mode %d:", mode);

    if (!ri.Vid_GetModeInfo(&width, &height, mode)) {
        ri.Con_Printf(PRINT_ALL, " invalid mode\n");
        return rserr_invalid_mode;
    }

    ri.Con_Printf(PRINT_ALL, " %d %d\n", width, height);

    _width = width;
    _height = height;
    *pwidth = width;
    *pheight = height;
    ri.Vid_NewWindow(width, height);
    return rserr_ok;
}

void GLimp_Shutdown(void) { }

int GLimp_Init(void *hinstance, void *wndproc) {
    setupWindow(false);
    return true;
}

void GLimp_BeginFrame(float camera_seperation) { }

void GLimp_EndFrame(void) {
    glFlush();
    glKosSwapBuffers();
}

void GLimp_AppActivate(qboolean active) { }

int QG_Milliseconds(void) {
    return timer_ms_gettime64();
}

void QG_CaptureMouse(void) { }
void QG_ReleaseMouse(void) { }

void HandleInput(void) {
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, state)
        int i;
        int button_state = 0;
        
        if (state->buttons & CONT_DPAD_UP)
            Quake2_SendKey(K_UPARROW, true);
        else
            Quake2_SendKey(K_UPARROW, false);
            
        if (state->buttons & CONT_DPAD_DOWN)
            Quake2_SendKey(K_DOWNARROW, true);
        else
            Quake2_SendKey(K_DOWNARROW, false);
            
        if (state->buttons & CONT_DPAD_LEFT)
            Quake2_SendKey(K_LEFTARROW, true);
        else
            Quake2_SendKey(K_LEFTARROW, false);
            
        if (state->buttons & CONT_DPAD_RIGHT)
            Quake2_SendKey(K_RIGHTARROW, true);
        else
            Quake2_SendKey(K_RIGHTARROW, false);

        if (state->buttons & CONT_A)
            button_state |= (1 << 0);
            
        if (state->buttons & CONT_B)
            button_state |= (1 << 1);
            
        if (state->buttons & CONT_X)
            Quake2_SendKey(K_ENTER, true);
        else
            Quake2_SendKey(K_ENTER, false);
            
        if (state->buttons & CONT_Y)
            Quake2_SendKey(K_ESCAPE, true);
        else
            Quake2_SendKey(K_ESCAPE, false);
            
        if (state->buttons & CONT_START)
            ri.Cmd_ExecuteText(EXEC_NOW, "quit");

        for (i = 0; i < 2; i++) {
            if ((button_state & (1<<i)) && !(_old_button_state & (1<<i)))
                Quake2_SendKey(K_MOUSE1 + i, true);
                
            if (!(button_state & (1<<i)) && (_old_button_state & (1<<i)))
                Quake2_SendKey(K_MOUSE1 + i, false);
        }
        
        _old_button_state = button_state;
    MAPLE_FOREACH_END()
}

void QG_GetMouseDiff(int* dx, int* dy) {
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, state)
        *dx = state->joyx / 4;
        *dy = state->joyy / 4;
    MAPLE_FOREACH_END()
}

int main(int argc, char **argv) {
    init_thread_stack();
    setSystemRam();  // Initialize RAM tracking

    // PowerVR config
    GLdcConfig config;
    glKosInitConfig(&config);
    config.autosort_enabled = GL_TRUE;
    config.fsaa_enabled = GL_FALSE;
    config.internal_palette_format = GL_RGBA8;
    config.initial_op_capacity = 4096 * 3;
    config.initial_pt_capacity = 256 * 3;
    config.initial_tr_capacity = 1024 * 3;
    config.initial_immediate_capacity = 256 * 3;
    glKosInitEx(&config);

    printf("Initial RAM Status:\n");
    printf("Total: %lu KB, Free: %lu KB, Used: %lu KB\n",
           getSystemRam() / 1024,
           getFreeRam() / 1024,
           getUsedRam() / 1024);

    int time, oldtime, newtime;
    Quake2_Init(argc, argv);

    oldtime = QG_Milliseconds();
    while (1) {
        HandleInput();
        
        do {
            newtime = QG_Milliseconds();
            time = newtime - oldtime;
        } while (time < 1);

        Quake2_Frame(time);
        checkAndDisplayRamStatus();  // Check RAM every 5 seconds
        
        oldtime = newtime;
        thd_pass();
    }

    return 0;
}
#include <stdio.h>
#include <kos.h>
#include "ref_soft/r_local.h"
#include "client/keys.h"
#include "quake2.h"
#include <SDL/SDL.h>
#include <kos/thread.h>

#define MAIN_STACK_SIZE (64 * 1024)  // 64KB for main thread

static SDL_Color colors[256];
static SDL_Surface *surface = NULL;
int _old_button_state = 0;
static int _width = 640;
static int _height = 480;

static void init_thread_stack(void) {
    kthread_t *current = thd_get_current();
    if (current) {
        void *new_stack = malloc(MAIN_STACK_SIZE);
        if (new_stack) {
            /* Set the stack and its size */
            current->stack = new_stack;
            current->stack_size = MAIN_STACK_SIZE;
            current->flags |= THD_OWNS_STACK;  /* Indicate that we own this stack */
        }
    }
}




static void setupWindow(qboolean fullscreen) {
    int flags = SDL_HWSURFACE;
    if (fullscreen)
        flags |= SDL_FULLSCREEN;
        
    surface = SDL_SetVideoMode(_width, _height, 8, flags);
    if (!surface)
        Sys_Error("VID: Couldn't set video mode: %s\n", SDL_GetError());

    vid.rowbytes = surface->pitch;
    vid.buffer = surface->pixels;
}

rserr_t SWimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen) {
    int width = 0, height = 0;

    ri.Con_Printf(PRINT_ALL, "Initializing display\n");
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

    setupWindow(fullscreen);
    ri.Vid_NewWindow(width, height);

    return rserr_ok;
}

void SWimp_Shutdown(void) {
    if (surface)
        SDL_FreeSurface(surface);
    SDL_Quit();
}

int SWimp_Init(void *hInstance, void *wndProc) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        Sys_Error("VID: Couldn't initialize SDL: %s\n", SDL_GetError());
        return false;
    }
    SDL_ShowCursor(SDL_DISABLE);  
    SDL_WM_SetCaption("Quake II", "Quake II");
    setupWindow(false);
    return true;
}

static qboolean SWimp_InitGraphics(qboolean fullscreen) {
    vid.rowbytes = surface->pitch;
    vid.buffer = surface->pixels;
    return true;
}

void SWimp_SetPalette(const unsigned char *palette) {
    int i;
    for (i = 0; i < 256; i++) {
        colors[i].r = *palette++;
        colors[i].g = *palette++;
        colors[i].b = *palette++;
        palette++;
    }
    SDL_SetColors(surface, colors, 0, 256);
}

void SWimp_BeginFrame(float camera_seperation) {}

void SWimp_EndFrame(void) {
    SDL_Flip(surface);
}

void SWimp_AppActivate(qboolean active) {}

static void HandleControllerInput(void) {
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, state)
        int i; 
        int button_state = 0;
        
        // D-pad movement
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

        // Action buttons
        if (state->buttons & CONT_A)
            button_state |= (1 << 0); // Primary fire
            
        if (state->buttons & CONT_B)
            button_state |= (1 << 1); // Secondary fire
            
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

        // Handle mouse button emulation
        for (i = 0; i < 2; i++) {
            if ((button_state & (1<<i)) && !(_old_button_state & (1<<i)))
                Quake2_SendKey(K_MOUSE1 + i, true);
                
            if (!(button_state & (1<<i)) && (_old_button_state & (1<<i)))
                Quake2_SendKey(K_MOUSE1 + i, false);
        }
        
        _old_button_state = button_state;
        
    MAPLE_FOREACH_END()
}

int QG_Milliseconds(void) {
    return timer_ms_gettime64();
}

void QG_GetMouseDiff(int* dx, int* dy) {
    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, state)
        *dx = state->joyx / 4;
        *dy = state->joyy / 4;
    MAPLE_FOREACH_END()
}

void QG_CaptureMouse(void) {
}

void QG_ReleaseMouse(void) {
}

int main(int argc, char **argv) {
    init_thread_stack();
    
    
    int time, oldtime, newtime;
    Quake2_Init(argc, argv);
    
    oldtime = Quake2_Milliseconds();
    while (1) {
        HandleControllerInput();
        
        do {
            newtime = Quake2_Milliseconds();
            time = newtime - oldtime;
        } while (time < 1);
        
        Quake2_Frame(time);
        oldtime = newtime;
        
        /* Give other threads a chance to run */
        thd_pass();
    }
    
    return 0;
}
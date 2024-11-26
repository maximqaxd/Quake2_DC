#include <kos.h>
#include "ref_gl/gl_local.h"
#include "client/keys.h"
#include "quake2.h"
#include <kos/thread.h>

#define MAIN_STACK_SIZE (64 * 1024)  // 64KB for main thread

static int _width = 640;
static int _height = 480;
int _old_button_state = 0;

typedef float dc_vec3_t[3];

typedef struct dc_poly_s {
    struct dc_poly_s *next;
    int numverts;
    int flags;
    float verts[4][3];    // Variable sized
} dc_poly_t;

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

void DC_DrawPoly(dc_poly_t *p) {
    int i;
    float *v;

    glBegin(GL_POLYGON);
    v = p->verts[0];
    for (i = 0; i < p->numverts; i++, v += 3) {
        glVertex3fv(v);
    }
    glEnd();
}

// Test room setup and rendering function
void Test_DrawRoom(void) {
    static float rot_y = 0.0f;
    static float cam_x = 0.0f;
    static float cam_y = 0.0f;
    static float cam_z = 0.0f;
    
    static dc_poly_t floor_poly = {
        .numverts = 4,
        .verts = {
            {-10.0f, -10.0f, -20.0f},
            {10.0f, -10.0f, -20.0f},
            {10.0f, 10.0f, -20.0f},
            {-10.0f, 10.0f, -20.0f}
        }
    };

    static dc_poly_t ceiling_poly = {
        .numverts = 4,
        .verts = {
            {-10.0f, -10.0f, -10.0f},
            {10.0f, -10.0f, -10.0f},
            {10.0f, 10.0f, -10.0f},
            {-10.0f, 10.0f, -10.0f}
        }
    };

    static dc_poly_t wall1_poly = {
        .numverts = 4,
        .verts = {
            {-10.0f, -10.0f, -20.0f},
            {-10.0f, -10.0f, -10.0f},
            {10.0f, -10.0f, -10.0f},
            {10.0f, -10.0f, -20.0f}
        }
    };

    static dc_poly_t wall2_poly = {
        .numverts = 4,
        .verts = {
            {10.0f, -10.0f, -20.0f},
            {10.0f, -10.0f, -10.0f},
            {10.0f, 10.0f, -10.0f},
            {10.0f, 10.0f, -20.0f}
        }
    };

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -50.0f);
    glRotatef(-90, 1, 0, 0);    
    glRotatef(90, 0, 0, 1);     
    glTranslatef(-cam_x, -cam_y, -cam_z);
    glRotatef(rot_y, 0, 1, 0);

    glColor3f(1.0f, 1.0f, 1.0f);  // White
    DC_DrawPoly(&floor_poly);

    glColor3f(1.0f, 0.0f, 0.0f);  // Red
    DC_DrawPoly(&ceiling_poly);

    glColor3f(0.0f, 1.0f, 0.0f);  // Green
    DC_DrawPoly(&wall1_poly);

    glColor3f(0.0f, 0.0f, 1.0f);  // Blue
    DC_DrawPoly(&wall2_poly);

    MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, state)
        if(state->buttons & CONT_DPAD_UP)
            cam_z -= 0.1f;
        if(state->buttons & CONT_DPAD_DOWN)
            cam_z += 0.1f;
        if(state->buttons & CONT_DPAD_LEFT)
            cam_x -= 0.1f;
        if(state->buttons & CONT_DPAD_RIGHT)
            cam_x += 0.1f;
        
        rot_y += state->joyx * 0.1f;
    MAPLE_FOREACH_END()
    
    glKosSwapBuffers();
}

// Quake 2 interface functions
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

// Main entry point
int main(int argc, char **argv) {
    init_thread_stack();

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

    // Let Q2 handle all GL state setup
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
        
        oldtime = newtime;
        thd_pass();
    }

    return 0;
}
#include <stdio.h>
#include <ctype.h>
#include "../qcommon/qcommon.h"

//===============================================================================

// Dreamcast-friendly memory limits
#define DC_TOTAL_MEMORY (32 * 1024 * 1024)  // 32MB total
#define DC_HUNK_MAX_SIZE (16 * 1024 * 1024) // 26MB max hunk

static byte *membase;
static int maxhunksize;
static int curhunksize;
static int hunk_count;
static int hunk_peak;

void *Hunk_Begin(int maxsize)
{
    Com_Printf("Hunk_Begin: Requested size: %d\n", maxsize);

    // Enforce maximum size for Dreamcast
    if (maxsize > DC_HUNK_MAX_SIZE) {
        Com_Printf("WARNING: Requested hunk size %d reduced to %d\n", maxsize, DC_HUNK_MAX_SIZE);
        maxsize = DC_HUNK_MAX_SIZE;
    }

    maxhunksize = maxsize;
    curhunksize = 0;
    hunk_peak = 0;

   // Com_Printf("Hunk_Begin: Attempting malloc of %d bytes\n", maxsize);
    membase = malloc(maxsize);
    
    if (!membase) {
        Sys_Error("Hunk_Begin: Failed to allocate %d bytes", maxsize);
        return NULL;
    }

    memset(membase, 0, maxsize);
  //  Com_Printf("Hunk_Begin: Successfully allocated at %p\n", membase);
    
    return membase;
	Hunk_Stats_f();
}

void *Hunk_Alloc(int size)
{
    byte *buf;

    // Round to 4 byte alignment
    size = (size+3)&~3;
    
  //  Com_Printf("Hunk_Alloc: Requesting %d bytes\n", size);

    if (curhunksize + size > maxhunksize) {
        Sys_Error("Hunk_Alloc: overflow - requested %d bytes, only %d remaining", 
                 size, maxhunksize - curhunksize);
    }

    buf = membase + curhunksize;
    curhunksize += size;

    // Track peak usage
    if (curhunksize > hunk_peak)
        hunk_peak = curhunksize;

    return buf;
}

int Hunk_End(void)
{
    void *newmem;

 //   Com_Printf("Hunk_End: Total=%d bytes, Peak=%d bytes (%.1f%% of max)\n", 
    //          curhunksize, hunk_peak, (hunk_peak * 100.0f) / maxhunksize);

    // Try to shrink the allocation to actual size
    newmem = realloc(membase, curhunksize);
    if (newmem != NULL) {
        membase = newmem;
    }

    hunk_count++;
    return curhunksize;
}

void Hunk_Free(void *base)
{
    if (base) {
        Com_Printf("Hunk_Free: Freeing hunk at %p\n", base);
        free(base);
        if (base == membase) {
            membase = NULL;
            curhunksize = 0;
        }
    }
    hunk_count--;
}

void Hunk_Stats_f(void)
{
    Com_Printf("\nHunk Statistics:\n");
    Com_Printf("----------------\n");
    Com_Printf("Current Size: %d bytes (%.2f MB)\n", curhunksize, curhunksize/(1024.0f*1024.0f));
    Com_Printf("Peak Usage:   %d bytes (%.2f MB)\n", hunk_peak, hunk_peak/(1024.0f*1024.0f));
    Com_Printf("Maximum Size: %d bytes (%.2f MB)\n", maxhunksize, maxhunksize/(1024.0f*1024.0f));
    Com_Printf("Usage:        %.1f%% of maximum\n", (curhunksize * 100.0f) / maxhunksize);
    Com_Printf("Peak Usage:   %.1f%% of maximum\n", (hunk_peak * 100.0f) / maxhunksize);
    Com_Printf("Hunk Count:   %d\n", hunk_count);
    Com_Printf("\n");
}

//============================================
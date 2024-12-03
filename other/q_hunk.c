#include <stdio.h>
#include <ctype.h>
#include "../qcommon/qcommon.h"

//===============================================================================

// Dreamcast memory limits
#define DC_TOTAL_MEMORY (16 * 1024 * 1024)  // 16MB total
#define DC_HUNK_MAX_SIZE (16 * 1024 * 1024) // 16MB max hunk

static byte *membase;
static int maxhunksize;
static int curhunksize;
static int hunk_count;
static int hunk_peak;

void *Hunk_Begin(int maxsize)
{
    static int last_allocation = 0;  // Track last successful allocation
    
    // Only log if this is a new allocation, not a retry
    if (maxsize != last_allocation) {
        Com_Printf("Hunk_Begin: New allocation of %d bytes\n", maxsize);
        last_allocation = maxsize;
    }

    maxhunksize = maxsize;
    curhunksize = 0;
    hunk_peak = 0;

    membase = malloc(maxsize);
    
    if (!membase) {
        Sys_Error("Hunk_Begin: Failed to allocate %d bytes", maxsize);
        return NULL;
    }

    memset(membase, 0, maxsize);
    
    return membase;
}
void *Hunk_Alloc(int size)
{
    byte *buf;

    // Round to 4 byte alignment
    size = (size+3)&~3;

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
        // Use snprintf to ensure proper string formatting
        char addrbuf[32];
        snprintf(addrbuf, sizeof(addrbuf), "%p", base);
        Com_Printf("Hunk_Free: Freeing hunk at %s\n", addrbuf);
        
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
    float current_mb = curhunksize / (1024.0f * 1024.0f);
    float peak_mb = hunk_peak / (1024.0f * 1024.0f);
    float max_mb = maxhunksize / (1024.0f * 1024.0f);
    
    Com_Printf("\nHunk Statistics:\n");
    Com_Printf("----------------\n");
    Com_Printf("Current Size: %.2f MB (%d bytes)\n", current_mb, curhunksize);
    Com_Printf("Peak Usage:   %.2f MB (%d bytes)\n", peak_mb, hunk_peak);
    Com_Printf("Maximum Size: %.2f MB (%d bytes)\n", max_mb, maxhunksize);
    Com_Printf("Usage:        %.1f%%\n", (curhunksize * 100.0f) / maxhunksize);
    Com_Printf("Peak Usage:   %.1f%%\n", (hunk_peak * 100.0f) / maxhunksize);
    Com_Printf("Hunk Count:   %d\n", hunk_count);
    Com_Printf("\n");
}
Looking at the formats, for best quality-to-memory ratio in your case:

1. For textures that need high fidelity (walls, important sprites):
- RGB-VQ (Row 5 in chart)
- Good compression (13-19%)
- Good quality
- Works well with 64x64 and 128x128 textures

2. For UI elements and less important textures:
- 256 colors look up - VQ Tiny (Row 8)
- Better compression (13-19%)
- Still decent quality
- Perfect for Quake 2's palette-based textures

And yes, you could massively simplify this code if everything is PVR! You could:
1. Remove all the format conversion code
2. Remove resizing logic since PVRs are pre-padded
3. Remove all the mipmap generation since PVRs can include mipmaps
4. Remove the texture format detection/handling
5. Remove palette conversion since PVRs handle that
6. Just have one simple loader that reads PVR files directly into VRAM

The whole texture loading system could potentially be reduced to just loading pre-processed PVR files with the right dimensions and format already set.








In that case, I'd recommend sticking with RGB-VQ (Row 5) for everything. Here's why:

1. It provides good quality (better than paletted formats)
2. Consistent 13-14% compression ratio at your target sizes
3. Works well for both detailed textures and UI elements
4. No need to juggle different formats
5. Still gets decent compression (3kb at 64x64, 18kb at 256x256)

While you could save some memory with 256 colors lookup formats, having a single format (RGB-VQ) would make the code much simpler and more maintainable. You'd still be getting good compression while maintaining quality, and the complexity reduction in the code might be worth the small memory tradeoff.










Here's a simplified version of gl_image.c using PVR only:
cCopy#include "gl_local.h"

image_t    gltextures[MAX_GLTEXTURES];
int        numgltextures;

// Simple structure for PVR texture header
typedef struct {
    uint32_t header_size;
    uint32_t width;
    uint32_t height;
    uint32_t format;  // RGB-VQ format
    uint32_t has_mipmaps;
    // ... other PVR header fields
} pvr_header_t;

/*
================
GL_LoadPVRFile
================
*/
byte *GL_LoadPVRFile(char *name, pvr_header_t *header)
{
    byte *buffer;
    int len = ri.FS_LoadFile(name, (void **)&buffer);
    
    if (!buffer || len < sizeof(pvr_header_t)) {
        return NULL;
    }
    
    // Copy header info
    memcpy(header, buffer, sizeof(pvr_header_t));
    
    return buffer;
}

/*
================
GL_LoadPVRTexture
================
*/
image_t *GL_LoadPVRTexture(char *name, imagetype_t type)
{
    image_t *image;
    pvr_header_t header;
    byte *buffer;
    
    // See if already loaded
    for (int i = 0; i < numgltextures; i++) {
        if (!strcmp(name, gltextures[i].name)) {
            gltextures[i].registration_sequence = registration_sequence;
            return &gltextures[i];
        }
    }
    
    // Load the PVR file
    buffer = GL_LoadPVRFile(name, &header);
    if (!buffer) {
        return NULL;
    }
    
    // Find a free image slot
    image = NULL;
    for (int i = 0; i < numgltextures; i++) {
        if (!gltextures[i].texnum) {
            image = &gltextures[i];
            break;
        }
    }
    if (!image) {
        if (numgltextures == MAX_GLTEXTURES)
            ri.Sys_Error(ERR_DROP, "MAX_GLTEXTURES");
        image = &gltextures[numgltextures++];
    }
    
    // Fill in the image info
    strcpy(image->name, name);
    image->width = header.width;
    image->height = header.height;
    image->type = type;
    image->registration_sequence = registration_sequence;
    
    // Generate texture and upload directly to VRAM
    GL_Bind(image->texnum);
    // Upload PVR data directly to GPU (actual function would depend on your PVR implementation)
    GL_UploadPVRToVRAM(buffer + header.header_size, &header);
    
    ri.FS_FreeFile(buffer);
    return image;
}

/*
================
GL_FindImage
================
*/
image_t *GL_FindImage(char *name, imagetype_t type)
{
    char pvr_name[MAX_QPATH];
    
    if (!name)
        return NULL;
        
    // Convert requested filename to .pvr
    COM_StripExtension(name, pvr_name);
    strcat(pvr_name, ".pvr");
    
    return GL_LoadPVRTexture(pvr_name, type);
}

/*
================
GL_FreeUnusedImages
================
*/
void GL_FreeUnusedImages(void)
{
    // Keep permanent textures
    r_notexture->registration_sequence = registration_sequence;
    r_particletexture->registration_sequence = registration_sequence;
    
    for (int i = 0; i < numgltextures; i++) {
        image_t *image = &gltextures[i];
        if (image->registration_sequence == registration_sequence)
            continue;
        if (!image->registration_sequence)
            continue;
        if (image->type == it_pic)
            continue;
        
        // Free it
        qglDeleteTextures(1, &image->texnum);
        memset(image, 0, sizeof(*image));
    }
}

/*
================
GL_InitImages
================
*/
void GL_InitImages(void)
{
    registration_sequence = 1;
    numgltextures = 0;
    
    // Load essential textures
    r_notexture = GL_FindImage("pics/notexture.pvr", it_wall);
    r_particletexture = GL_FindImage("pics/particle.pvr", it_sprite);
}

/*
================
GL_ShutdownImages
================
*/
void GL_ShutdownImages(void)
{
    for (int i = 0; i < numgltextures; i++) {
        if (!gltextures[i].registration_sequence)
            continue;
        qglDeleteTextures(1, &gltextures[i].texnum);
        memset(&gltextures[i], 0, sizeof(image_t));
    }
}
This simplified version:

Only handles PVR format
No format conversion needed
No resizing/resampling
Direct upload to VRAM
Removed all the PCX/WAL/TGA loading code
No palette management
No manual mipmap generation

You'd need to:

Pre-process all textures to PVR format
Use RGB-VQ format for all textures
Implement the actual PVR-to-VRAM upload function based on your hardware interface
Decide if you want mipmaps in the PVR files

The actual implementation might need some tweaking based on your exact PVR format and hardware interface, but this shows the basic structure.









1. Memory Issues & Initial Problem
- Running out of VRAM with current texture handling
- Need to optimize texture memory usage
- Dreamcast has 8MB VRAM, but only ~4.8MB available for textures (rest used for framebuffer)

2. Current Texture Situation
- About 240 textures being loaded
- Currently using too much memory (15360 KB reported)
- Mix of different texture sizes from UI elements to wall textures

3. PowerVR VQ Format Discussion
- Different VQ compression options available:
  * RGB-VQ: 13-19% of original size
  * 256 colors lookup VQ
  * VQ Tiny variants
- Decided on RGB-VQ as best single format solution because:
  * Good quality-to-size ratio
  * Works well for all texture types
  * 13-14% compression ratio
  * Simpler to use one format

4. Texture Size Considerations
- Current approach forces textures to specific sizes
- PVR requires square, power-of-2 textures
- Can use padding for non-square textures
- UV coordinates still work with padded textures (unused areas never seen)

5. Memory Calculations with VQ
Example distribution for 300 textures:
- 100 small (32x32): 51.2KB
- 100 medium (128x128): 1MB
- 100 large (256x256): 1.8MB
- Total: ~2.85MB with VQ compression

6. Proposed Solutions
- Convert all textures to PVR format
- Use RGB-VQ compression
- Pre-pad textures to appropriate square sizes
- Optional mipmaps for world textures
- Simplified texture loading code

7. Code Simplification
- Can remove multiple format support
- No need for runtime texture conversion
- No need for manual mipmap generation
- Direct VRAM loading
- Simpler memory management

8. Mipmaps
- Useful for:
  * Performance with distant textures
  * Visual quality at angles
  * Reducing shimmer
- Trade-off: Uses about 33% more memory
- Could be selective about which textures use mipmaps

9. Next Steps
- Convert all textures to PVR format
- Implement simplified texture loading system
- Test memory usage with new format
- Consider selective mipmap usage
- Verify visual quality

This provides a good balance between visual quality, memory usage, and code simplicity for the Dreamcast port.
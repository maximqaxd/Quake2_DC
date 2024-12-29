

#include "gl_local.h"

image_t		gltextures[MAX_GLTEXTURES];
int			numgltextures;
int			base_textureid;		// gltextures[i] = base_textureid+i

static byte			 intensitytable[256];
static unsigned char gammatable[256];

cvar_t		*intensity;

unsigned	d_8to24table[256];

qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean is_sky );
qboolean GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap);


int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_tex_solid_format = 3;
int		gl_tex_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

void GL_SetTexturePalette(unsigned palette[256])
{
    if(!qglColorTableEXT || !gl_ext_palettedtexture->value) {
        ri.Con_Printf(PRINT_ALL, "ERROR: Paletted textures not supported!\n");
        return;
    }

    ri.Con_Printf(PRINT_ALL, "Setting shared texture palette...\n");
    int i;
    unsigned char temptable[768];
    for( i = 0; i < 256; i++) {
        temptable[i*3+0] = (palette[i] >> 0) & 0xff;
        temptable[i*3+1] = (palette[i] >> 8) & 0xff;
        temptable[i*3+2] = (palette[i] >> 16) & 0xff;
        
        // Debug first few palette entries
        if(i < 4) {
            ri.Con_Printf(PRINT_ALL, "Palette[%d]: R=%d G=%d B=%d\n", 
                i, temptable[i*3], temptable[i*3+1], temptable[i*3+2]);
        }
    }

    glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
    qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT,
                   GL_RGB8,
                   256,
                   GL_RGB,
                   GL_UNSIGNED_BYTE,
                   temptable);

    GLenum err = glGetError();
    if(err != GL_NO_ERROR) {
        ri.Con_Printf(PRINT_ALL, "GL ERROR %d after setting palette\n", err);
    } else {
        ri.Con_Printf(PRINT_ALL, "Palette set successfully!\n");
    }
}
void GL_EnableMultitexture( qboolean enable )
{
	if ( !qglSelectTextureSGIS )
		return;

	if ( enable )
	{
		GL_SelectTexture( GL_TEXTURE1_SGIS );
		qglEnable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	else
	{
		GL_SelectTexture( GL_TEXTURE1_SGIS );
		qglDisable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	GL_SelectTexture( GL_TEXTURE0_SGIS );
	GL_TexEnv( GL_REPLACE );
}

void GL_SelectTexture( GLenum texture )
{
	int tmu;

	if ( !qglSelectTextureSGIS )
		return;

	if ( texture == GL_TEXTURE0_SGIS )
		tmu = 0;
	else
		tmu = 1;

	if ( tmu == gl_state.currenttmu )
		return;

	gl_state.currenttmu = tmu;

	if ( tmu == 0 )
		qglSelectTextureSGIS( GL_TEXTURE0_SGIS );
	else
		qglSelectTextureSGIS( GL_TEXTURE1_SGIS );
}

void GL_TexEnv( GLenum mode )
{
	static int lastmodes[2] = { -1, -1 };

	if ( mode != lastmodes[gl_state.currenttmu] )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		lastmodes[gl_state.currenttmu] = mode;
	}
}

void GL_Bind(int texnum) 
{ 
    extern image_t *draw_chars;
    static GLuint base_texture = 0;
    static int initialized = 0;
    static int texture_count = 0;
    static int total_texture_memory = 0;
    #define MAX_DC_TEXTURES 400
    static GLuint texture_map[MAX_DC_TEXTURES] = {0};
    
    if(!initialized) {
        glGenTextures(1, &base_texture);
        initialized = 1;
    }
    
    if(texnum == 0) {
        return;
    }
    
    int map_index = (texnum % MAX_DC_TEXTURES);
    if(!texture_map[map_index]) {
        GLuint new_tex;
        glGenTextures(1, &new_tex);
        texture_map[map_index] = new_tex;
        texture_count++;
        
        // Assuming 128x128 textures, 4 bytes per pixel
        int tex_size = 128 * 128 * 4;
        total_texture_memory += tex_size;
        
    
    }
    
    GLuint mapped_texnum = texture_map[map_index];
    
    if (gl_nobind->value && draw_chars)
        mapped_texnum = draw_chars->texnum;
    
    if (gl_state.currenttextures[gl_state.currenttmu] == mapped_texnum)
        return;
    
    gl_state.currenttextures[gl_state.currenttmu] = mapped_texnum;
    glBindTexture(GL_TEXTURE_2D, mapped_texnum);
}
void GL_MBind(GLenum target, int texnum)
{
    extern image_t *draw_chars;
    static GLuint texture_map[MAX_DC_TEXTURES] = {0};
    
    GL_SelectTexture(target);
    
    if(texnum == 0)
        return;
        
    int map_index = (texnum % MAX_DC_TEXTURES);
    if(!texture_map[map_index]) {
        GLuint new_tex;
        glGenTextures(1, &new_tex);
        texture_map[map_index] = new_tex;
    }
    
    GLuint mapped_texnum = texture_map[map_index];
    
    if(target == GL_TEXTURE0_SGIS) {
        if(gl_state.currenttextures[0] == mapped_texnum)
            return;
        gl_state.currenttextures[0] = mapped_texnum;
    } else {
        if(gl_state.currenttextures[1] == mapped_texnum)
            return;
        gl_state.currenttextures[1] = mapped_texnum;
    }
    
    glBindTexture(GL_TEXTURE_2D, mapped_texnum);
}
typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( char *string )
{
	int		i;
	image_t	*glt;

	for (i=0 ; i< NUM_GL_MODES ; i++)
	{
		if ( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky )
		{
			GL_Bind (glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
GL_TextureAlphaMode
===============
*/
void GL_TextureAlphaMode( char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_ALPHA_MODES ; i++)
	{
		if ( !Q_stricmp( gl_alpha_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_ALPHA_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

/*
===============
GL_TextureSolidMode
===============
*/
void GL_TextureSolidMode( char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_SOLID_MODES ; i++)
	{
		if ( !Q_stricmp( gl_solid_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_SOLID_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}

/*
===============
GL_ImageList_f
===============
*/
void	GL_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;
	const char *palstrings[2] =
	{
		"RGB",
		"PAL"
	};

	ri.Con_Printf (PRINT_ALL, "------------------\n");
	texels = 0;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (image->texnum <= 0)
			continue;
		texels += image->upload_width*image->upload_height;
		switch (image->type)
		{
		case it_skin:
			ri.Con_Printf (PRINT_ALL, "M");
			break;
		case it_sprite:
			ri.Con_Printf (PRINT_ALL, "S");
			break;
		case it_wall:
			ri.Con_Printf (PRINT_ALL, "W");
			break;
		case it_pic:
			ri.Con_Printf (PRINT_ALL, "P");
			break;
		default:
			ri.Con_Printf (PRINT_ALL, " ");
			break;
		}

		ri.Con_Printf (PRINT_ALL,  " %3i %3i %s: %s\n",
			image->upload_width, image->upload_height, palstrings[image->paletted], image->name);
	}
	ri.Con_Printf (PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up inefficient hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		1
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
qboolean	scrap_dirty;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
//	Sys_Error ("Scrap_AllocBlock: full");
}

int	scrap_uploads;

void Scrap_Upload(void)
{
    unsigned trans[BLOCK_WIDTH * BLOCK_HEIGHT];
    int i;

    scrap_uploads++;
    GL_Bind(TEXNUM_SCRAPS);
    
    /* Force nearest neighbor filtering for scrap textures since they contain HUD elements */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    /* Convert 8-bit indices to 32-bit RGBA */
    for (i = 0; i < BLOCK_WIDTH * BLOCK_HEIGHT; i++)
    {
        byte p = scrap_texels[0][i];
        if (p == 255)  // Transparent index
        {
            trans[i] = 0;  // Fully transparent
        }
        else
        {
            trans[i] = d_8to24table[p];
            trans[i] |= 0xff000000;  // Fully opaque
        }
    }

    /* Upload as RGBA */
    glTexImage2D(GL_TEXTURE_2D, 
                 0, 
                 GL_RGBA,  // Changed from GL_COLOR_INDEX8_EXT
                 BLOCK_WIDTH, 
                 BLOCK_HEIGHT,
                 0,
                 GL_RGBA,  // Changed from GL_COLOR_INDEX
                 GL_UNSIGNED_BYTE,
                 trans);
                 
    scrap_dirty = false;
}


/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	len = ri.FS_LoadFile (filename, (void **)&raw);
	if (!raw)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

    pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		ri.Con_Printf (PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	out = malloc ( (pcx->ymax+1) * (pcx->xmax+1) );

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = malloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free (*pic);
		*pic = NULL;
	}

	ri.FS_FreeFile (pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/
void LoadTGA (char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	int		length;
	TargaHeader		targa_header;
	byte			*targa_rgba;
	byte tmp[2];

	*pic = NULL;

	//
	// load the file
	//
	length = ri.FS_LoadFile (name, (void **)&buffer);
	if (!buffer)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad tga file %s\n", name);
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.y_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.width = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.height = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type!=2 
		&& targa_header.image_type!=10) 
		ri.Sys_Error (ERR_DROP, "LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type !=0 
		|| (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		ri.Sys_Error (ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = malloc (numPixels*4);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment
	
	if (targa_header.image_type==2) {  // Uncompressed, RGB images
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) {
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) {
					case 24:
							
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
					case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
						}
						column++;
						if (column==columns) { // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}

	ri.FS_FreeFile (buffer);
}


/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void R_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================


/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	byte		*pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i=0 ; i<outwidth ; i++)
	{
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for (i=0 ; i<outwidth ; i++)
	{
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

/*
================
GL_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
void GL_LightScaleTexture (unsigned *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( only_gamma )
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=4)
		{
			p[0] = gammatable[intensitytable[p[0]]];
			p[1] = gammatable[intensitytable[p[1]]];
			p[2] = gammatable[intensitytable[p[2]]];
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
    // If dimensions aren't equal, make them equal by using the larger dimension
    if (width != height) {
        int max_dim = (width > height) ? width : height;
        width = height = max_dim;
    }

    int     i, j;
    byte    *out;

    width <<=2;
    height >>= 1;
    out = in;
    for (i=0 ; i<height ; i++, in+=width)
    {
        for (j=0 ; j<width ; j+=8, out+=4, in+=8)
        {
            out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
            out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
            out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
            out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
        }
    }
}
/*
===============
GL_Upload32

Returns has_alpha
===============
*/
 

int		upload_width, upload_height;
qboolean uploaded_paletted;

 
/*
===============
GL_Upload8

Returns has_alpha
===============
*/
/*
static qboolean IsPowerOf2( int value )
{
	int i = 1;


	while ( 1 )
	{
		if ( value == i )
			return true;
		if ( i > value )
			return false;
		i <<= 1;
	}
}
*/

 
qboolean GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean is_sky)
{
    int scaled_width = 1;
    int scaled_height = 1;
    int new_size;
    static byte *scaled = NULL;
    static int scaled_size = 0;
    static byte *rgba = NULL;
    static int rgba_size = 0;
    qboolean is_hud;
    int miplevel;
    int mipsize;
    int x, y;
    
    /* Debug output */
    ri.Con_Printf(PRINT_ALL, "GL_Upload8: %dx%d mipmap=%d sky=%d\n", 
                 width, height, mipmap, is_sky);
    
    /* Detect HUD elements - any non-mipmapped, non-sky texture */
    is_hud = (!mipmap && !is_sky);
    
    /* Calculate power-of-2 dimensions */
    while(scaled_width < width) 
        scaled_width <<= 1;
    while(scaled_height < height) 
        scaled_height <<= 1;
        
    if (!is_sky && !is_hud) {
        if (scaled_width > 128) 
            scaled_width = 128;
        if (scaled_height > 128) 
            scaled_height = 128;
    }
    
    if (is_hud) {
        ri.Con_Printf(PRINT_ALL, "Processing as HUD element %dx%d -> %dx%d\n", 
                     width, height, scaled_width, scaled_height);
    }
    
    /* Allocate or reuse scaling buffer */
    new_size = scaled_width * scaled_height;
    if (scaled_size < new_size) {
        if (scaled) 
            free(scaled);
        scaled = malloc(new_size);
        if (!scaled) {
            ri.Con_Printf(PRINT_ALL, "GL_Upload8: out of memory\n");
            return false;
        }
        scaled_size = new_size;
    }

    /* Allocate or reuse RGBA buffer */
    if (rgba_size < new_size * 4) {
        if (rgba) 
            free(rgba);
        rgba = malloc(new_size * 4);
        if (!rgba) {
            ri.Con_Printf(PRINT_ALL, "GL_Upload8: out of memory for RGBA\n");
            return false;
        }
        rgba_size = new_size * 4;
    }
    
    /* Scale the texture to power-of-2 dimensions */
    for(y = 0; y < scaled_height; y++) {
        int src_y = (y * height) / scaled_height;
        for(x = 0; x < scaled_width; x++) {
            int src_x = (x * width) / scaled_width;
            scaled[y * scaled_width + x] = data[src_y * width + src_x];
        }
    }

    if (is_hud) {
        /* Convert to RGBA with proper alpha */
		int i;

        for (i = 0; i < new_size; i++) {
            byte p = scaled[i];
            unsigned color = d_8to24table[p];
            
            if (p == 255) {  // Transparent pixel
                rgba[i*4+0] = 0;    // R
                rgba[i*4+1] = 0;    // G
                rgba[i*4+2] = 0;    // B
                rgba[i*4+3] = 0;    // A (transparent)
            } else {
                rgba[i*4+0] = (color >> 0)  & 0xFF;  // R
                rgba[i*4+1] = (color >> 8)  & 0xFF;  // G
                rgba[i*4+2] = (color >> 16) & 0xFF;  // B
                rgba[i*4+3] = 0xFF;                  // A (opaque)
            }
        }

        /* Force nearest neighbor filtering for crisp HUD */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        /* Enable blending */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        /* Upload as RGBA */
        glTexImage2D(GL_TEXTURE_2D, 
                    0, 
                    4,           // Changed to 4 for RGBA
                    scaled_width, 
                    scaled_height,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    rgba);
    } else {
        /* Use specified filtering for world textures */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
                       mipmap ? gl_filter_min : gl_filter_max);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

        /* Upload as color indexed */
        glTexImage2D(GL_TEXTURE_2D, 
                    0, 
                    GL_COLOR_INDEX8_EXT,
                    scaled_width, 
                    scaled_height,
                    0,
                    GL_COLOR_INDEX,
                    GL_UNSIGNED_BYTE,
                    scaled);
    
        /* Only generate mipmaps for world textures */
        if(mipmap && scaled_width == scaled_height) {
            miplevel = 0;
            mipsize = scaled_width;
            
            while(mipsize > 1) {
                for(y = 0; y < mipsize/2; y++) {
                    for(x = 0; x < mipsize/2; x++) {
                        byte p1 = scaled[y*2*mipsize + x*2];
                        byte p2 = scaled[y*2*mipsize + x*2 + 1];
                        byte p3 = scaled[(y*2+1)*mipsize + x*2];
                        byte p4 = scaled[(y*2+1)*mipsize + x*2 + 1];
                        scaled[y*(mipsize/2) + x] = (p1 + p2 + p3 + p4) >> 2;
                    }
                }
                
                mipsize >>= 1;
                miplevel++;
                
                glTexImage2D(GL_TEXTURE_2D,
                            miplevel,
                            GL_COLOR_INDEX8_EXT, 
                            mipsize,
                            mipsize,
                            0,
                            GL_COLOR_INDEX,
                            GL_UNSIGNED_BYTE,
                            scaled);
            }
        }
    }
    
    upload_width = scaled_width;
    upload_height = scaled_height;
    
    return is_hud;
}
/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic(char *name, byte *pic, int width, int height, imagetype_t type, int bits)
{
    image_t *image;
    int i;
    
    /* Find free image slot */
    for(i = 0; i < numgltextures; i++) {
        if(!gltextures[i].texnum)
            break;
    }
    
    if(i == numgltextures) {
        if(numgltextures == MAX_GLTEXTURES) {
            ri.Con_Printf(PRINT_ALL, "GL_LoadPic: MAX_GLTEXTURES exceeded\n");
            return NULL;
        }
        numgltextures++;
    }
    
    image = &gltextures[i];
    
    if (strlen(name) >= sizeof(image->name)) {
        ri.Con_Printf(PRINT_ALL, "GL_LoadPic: name too long\n");
        return NULL;
    }
    
    strcpy(image->name, name);
    image->registration_sequence = registration_sequence;
    image->width = width;
    image->height = height;
    image->type = type;
    
    if(type == it_skin && bits == 8)
        R_FloodFillSkin(pic, width, height);
    
    /* Always put small HUD/pic elements into scrap */
    if(type == it_pic && bits == 8 && width <= 64 && height <= 64) {
        int x, y;
        int texnum = Scrap_AllocBlock(width, height, &x, &y);
        if(texnum != -1) {
            scrap_dirty = true;
            int k = 0;
            int j;
            
            /* Copy texture data into scrap */
            for(i = 0; i < height; i++) {
                for(j = 0; j < width; j++, k++)
                    scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = pic[k];
            }
            
            image->texnum = TEXNUM_SCRAPS + texnum;
            image->scrap = true;
            image->sl = (x+0.01)/(float)BLOCK_WIDTH;
            image->sh = (x+width-0.01)/(float)BLOCK_WIDTH;
            image->tl = (y+0.01)/(float)BLOCK_WIDTH;
            image->th = (y+height-0.01)/(float)BLOCK_WIDTH;
            
            /* Force an immediate upload if this is the first HUD element */
            if(scrap_uploads == 0) {
                Scrap_Upload();
            }
            
            return image;
        }
    }
    
    /* Handle non-scrap textures */
    image->scrap = false;
    image->texnum = TEXNUM_IMAGES + (image - gltextures);
    GL_Bind(image->texnum);
    
    image->has_alpha = GL_Upload8(pic, width, height,
                                 (type != it_pic && type != it_sky),
                                 type == it_sky);
    
    image->upload_width = upload_width;
    image->upload_height = upload_height;
    image->paletted = true;
    image->sl = 0;
    image->sh = 1;
    image->tl = 0;
    image->th = 1;
    
    return image;
}

/*
================
GL_LoadWal
================
*/
image_t *GL_LoadWal (char *name)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		ri.Con_Printf (PRINT_ALL, "GL_FindImage: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	image = GL_LoadPic (name, (byte *)mt + ofs, width, height, it_wall, 8);

	ri.FS_FreeFile ((void *)mt);

	return image;
}
/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
image_t	*GL_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len;
	byte	*pic, *palette;
	int		width, height;

	if (!name)
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: NULL name");
	len = strlen(name);
	if (len<5)
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad name: %s", name);

	// look for it
	for (i=0, image=gltextures ; i<numgltextures ; i++,image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	pic = NULL;
	palette = NULL;
	if (!strcmp(name+len-4, ".pcx"))
	{
		LoadPCX (name, &pic, &palette, &width, &height);
		if (!pic)
			return NULL; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);
		image = GL_LoadPic (name, pic, width, height, type, 8);
	}
	else if (!strcmp(name+len-4, ".wal"))
	{
		image = GL_LoadWal (name);
	}
	else if (!strcmp(name+len-4, ".tga"))
	{
		LoadTGA (name, &pic, &width, &height);
		if (!pic)
			return NULL; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);
		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad extension on: %s", name);


	if (pic)
		free(pic);
	if (palette)
		free(palette);

	return image;
}



/*
===============
R_RegisterSkin
===============
*/
struct image_s *R_RegisterSkin (char *name)
{
	return GL_FindImage (name, it_skin);
}


/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void GL_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		if (image->type == it_pic)
			continue;		// don't free pics
		// free it
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette(void)
{
    int i;
    int r, g, b;
    unsigned v;
    byte *pic, *pal;
    int width, height;

    /* Load the palette from colormap.pcx */
    LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);
    if (!pal)
        ri.Sys_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");

    for(i = 0; i < 256; i++)
    {
        r = pal[i*3+0];
        g = pal[i*3+1];
        b = pal[i*3+2];
        
        /* Key difference: Make everything fully opaque except index 255 */
        if(i == 255) {
            /* Make index 255 fully transparent */
            v = (0<<24) + (r<<0) + (g<<8) + (b<<16);
        } else {
            /* Make all other colors fully opaque */
            v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
        }
        
        d_8to24table[i] = LittleLong(v);
    }

    /* No need to modify alpha separately as we handled it above */
    
    free(pic);
    free(pal);

    return 0;
}

/*
===============
GL_InitImages
===============
*/
void GL_InitImages(void)
{
    registration_sequence = 1;

    // Get the palette from colormap.pcx
    Draw_GetPalette();

    // Load 16-to-8 color conversion table
    if (qglColorTableEXT)
    {
        ri.FS_LoadFile("pics/16to8.dat", &gl_state.d_16to8table);
        if (!gl_state.d_16to8table)
            ri.Sys_Error(ERR_FATAL, "Couldn't load pics/16to8.pcx");
            
        // Set up the shared texture palette immediately
        GL_SetTexturePalette(d_8to24table);
    }
    else
    {
        ri.Con_Printf(PRINT_ALL, "Warning: Paletted textures not supported\n");
    }

    // Initialize but simplify gamma correction
    int i;
    float g = vid_gamma->value;
    
    // Simple linear gamma for 8-bit textures
    for (i = 0; i < 256; i++)
    {
        gammatable[i] = i;
    }
    
    // Initialize intensity but keep it simple
    intensity = ri.Cvar_Get("intensity", "1", 0);
    gl_state.inverse_intensity = 1.0f;
    
    // Simple 1:1 intensity mapping
    for (i = 0; i < 256; i++)
    {
        intensitytable[i] = i;
    }
}

/*
===============
GL_ShutdownImages
===============
*/
void	GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}
}


/*----------------------------------------------------------------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#------  This File is Part Of : ----------------------------------------------------------------------------------------#
#------- _  -------------------  ______   _   --------------------------------------------------------------------------#
#------ | | ------------------- (_____ \ | |  --------------------------------------------------------------------------#
#------ | | ---  _   _   ____    _____) )| |  ____  _   _   ____   ____   ----------------------------------------------#
#------ | | --- | | | | / _  |  |  ____/ | | / _  || | | | / _  ) / ___)  ----------------------------------------------#
#------ | |_____| |_| |( ( | |  | |      | |( ( | || |_| |( (/ / | |  --------------------------------------------------#
#------ |_______)\____| \_||_|  |_|      |_| \_||_| \__  | \____)|_|  --------------------------------------------------#
#------------------------------------------------- (____/  -------------------------------------------------------------#
#------------------------   ______   _   -------------------------------------------------------------------------------#
#------------------------  (_____ \ | |  -------------------------------------------------------------------------------#
#------------------------   _____) )| | _   _   ___   ------------------------------------------------------------------#
#------------------------  |  ____/ | || | | | /___)  ------------------------------------------------------------------#
#------------------------  | |      | || |_| ||___ |  ------------------------------------------------------------------#
#------------------------  |_|      |_| \____|(___/   ------------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#- Licensed under the GPL License --------------------------------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#- Copyright (c) Nanni <lpp.nanni@gmail.com> ---------------------------------------------------------------------------#
#- Copyright (c) Rinnegatamante <rinnegatamante@gmail.com> -------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#- Credits : -----------------------------------------------------------------------------------------------------------#
#-----------------------------------------------------------------------------------------------------------------------#
#- Smealum for ctrulib and ftpony src ----------------------------------------------------------------------------------#
#- StapleButter for debug font -----------------------------------------------------------------------------------------#
#- Lode Vandevenne for lodepng -----------------------------------------------------------------------------------------#
#- Jean-loup Gailly and Mark Adler for zlib ----------------------------------------------------------------------------#
#- xerpi for sf2dlib ---------------------------------------------------------------------------------------------------#
#- Special thanks to Aurelio for testing, bug-fixing and various help with codes and implementations -------------------#
#-----------------------------------------------------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <3ds.h>
#include "Graphics.h"
#include "lodepng.h"
#include "sf2d.h"

typedef unsigned short u16;
u8* TopLFB;
u8* TopRFB;
u8* BottomFB;

void RefreshScreen(){
	TopLFB = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	BottomFB = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
}

void ClearScreen(int screen){
	if (screen==1) memset(BottomFB,0x00,230400);
	else memset(TopLFB,0x00,288000);
}

void initScene(){
	sf2d_init();
	sf2d_set_clear_color(RGBA8(0x48, 0xB9, 0xFF, 0xFF));
}

void initBlend(int screen){
	gfxScreen_t my_screen;
	if (screen == 0) my_screen = GFX_TOP;
	else my_screen = GFX_BOTTOM;
	sf2d_start_frame(my_screen, GFX_LEFT);
}

void fillRect(float x1, float y1, float x2, float y2, u32 color){
	sf2d_draw_rectangle(x1, y1, x2, y2, RGBA8((color >> 16) & 0xFF, (color >> 8) & 0xFF, (color) & 0xFF, (color >> 24) & 0xFF));
}

void drawLine(float x1, float y1, float x2, float y2, u32 color){
	sf2d_draw_line(x1, y1, x2, y2, RGBA8((color >> 16) & 0xFF, (color >> 8) & 0xFF, (color) & 0xFF, (color >> 24) & 0xFF));
}

void drawAsset(float x, float y, gpu_text* asset){
	sf2d_draw_texture(asset->tex, x, y);
}

void termBlend(){
	sf2d_end_frame();
}

void endScene(){
	sf2d_fini();
}

Bitmap* decodePNGfile(const char* filename)
{
	Handle fileHandle;
	Bitmap* result;
	u64 size;
	u32 bytesRead;
	unsigned char* out;
	unsigned char* in;
	unsigned int w, h;
	
	FS_path filePath = FS_makePath(PATH_CHAR, filename);
	FS_archive archive = (FS_archive) { ARCH_SDMC, (FS_path) { PATH_EMPTY, 1, (u8*)"" }};
	FSUSER_OpenFileDirectly(NULL, &fileHandle, archive, filePath, FS_OPEN_READ, FS_ATTRIBUTE_NONE);
	
	FSFILE_GetSize(fileHandle, &size);
	
	in = (unsigned char*)malloc(size);
	
	if(!in) {
		FSFILE_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return 0;
	}
	
	FSFILE_Read(fileHandle, &bytesRead, 0x00, in, size);
	FSFILE_Close(fileHandle);
	svcCloseHandle(fileHandle);
		
		if(lodepng_decode32(&out, &w, &h, in, size) != 0) {
			free(in);
			return 0;
		}
	
	free(in);
	
	result = (Bitmap*)malloc(sizeof(Bitmap));
	if(!result) {
		free(out);
	}
	
	result->pixels = out;
	result->width = w;
	result->height = h;
	result->bitperpixel = 32;
	return result;
}


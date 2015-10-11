#include "Font.hpp"
extern "C"{
	#include "sf2d.h"
}

struct ttf{
	u32 magic;
	Font f;
	unsigned char* buffer;
};
struct Bitmap{
	u32 magic;
	u8* pixels;
	int width;
	int height;
	u16 bitperpixel;
};
struct gpu_text{
	u32 magic;
	u16 width;
	u16 height;
	sf2d_texture* tex;
};

extern u8* TopLFB;
extern u8* TopRFB;
extern u8* BottomFB;
void RefreshScreen();
void ClearScreen(int screen);
void initScene();
void endScene();
void initBlend(int screen);
void termBlend();
void fillRect(float x1, float y1, float x2, float y2, u32 color);
void drawLine(float x1, float y1, float x2, float y2, u32 color);
void drawAsset(float x, float y, gpu_text* asset);
gpu_text* getLogo();
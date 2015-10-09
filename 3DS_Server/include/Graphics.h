extern "C"{
	//#include "../sf2d/sf2d.h"
}
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
	//sf2d_texture* tex;
};

extern u8* TopLFB;
extern u8* TopRFB;
extern u8* BottomFB;
void RefreshScreen();
void ClearScreen(int screen);
Bitmap* decodePNGfile(const char* filename);
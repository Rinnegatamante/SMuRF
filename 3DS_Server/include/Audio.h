#include "Network.h"

void My_CSND_playsound(u32 channel, u32 looping, u32 encoding, u32 samplerate, u32 *vaddr0, u32 *vaddr1, u32 totalbytesize, u32 l_vol, u32 r_vol);
void CSND_SetAdpcmState(u32 channel, int block, int sample, int index);
void CSND_ChnSetAdpcmReload(u32 channel, bool reload);

// Additional encoding
typedef enum{
	CSND_ENCODING_VORBIS = 4
}CSND_EXTRA_ENCODING;

struct Music{
	u32 magic;
	u32 samplerate;
	u16 bytepersample;
	u8* audiobuf;
	u8* audiobuf2;
	u32 size;
	u32 mem_size;
	Handle sourceFile;
	u32 startRead;
	char author[256];
	char title[256];
	u32 moltiplier;
	u64 tick;
	bool isPlaying;
	u32 ch;
	u32 ch2;
	bool streamLoop;
	bool big_endian;
	u8 encoding;
	u32* thread;
	u32 audio_pointer;
	u32 package_size;
	u32 total_packages_size;
	u32 stream_packages_size;
};

Music* prepareSong(Socket* Client, u32 idx);
void startMusic(Socket* sock, Music* src);
void streamSong();
void closeMusic(Music* src);
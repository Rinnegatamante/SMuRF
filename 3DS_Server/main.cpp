#include <3ds.h>
#include "Network.h"
#include "Graphics.h"
#include "main_font.h"
#include "khax.h"
#define TOP_SCREEN true
#define BOTTOM_SCREEN false
#define UNDER 1
#define UPPER 0

// Globals
bool isNinjhax2;
u32 white = 0xFFFFFFFF;
u32 black = 0xFF000000;
u32 red = 0xFF0000FF;
u32 light_cyan = 0xFF48B9FF;
u32 dark_cyan = 0xFF00B9FF;
gpu_text* logo;
int idx;
Font F;
u32 total;

// Supported formats list
enum{
	WAV_PCM16 = 0,
	OGG_VORBIS = 1,
	AIFF_PCM16 = 2,
};

// Structures definition
typedef struct{
	char name[256];
	u8 format;
	void* next;
} Songlist;

// fetchSonglist: Gets songlist from a client
Songlist* fetchSonglist(Socket* Client, u32* totalSongs){
	socketSend(Client, "exec0:0000");
	Packet* pkg = NULL;
	Songlist* head = NULL;
	Songlist* list = NULL;
	Songlist* old = NULL;
	u32 idx = 0;
	*totalSongs = 0;
	bool notReceived = true;
	while (((pkg = socketRecv(Client, 128)) != NULL) || notReceived){	
		if (pkg == NULL){
			if (idx == 0) continue;
			else{
				old->next = NULL;
				free(list);
				break;
			}
		}
		notReceived = false;
		if (pkg->size == 1){
			free(pkg->message);
			free(pkg);
			return NULL;
		}else{
			if (idx == 0){
				list = (Songlist*)malloc(sizeof(Songlist));
				head = list;
			}
			char tmp[2];
			tmp[0] = pkg->message[0];
			tmp[1] = 0;
			list->format = atoi(tmp);
			strcpy(list->name, (char*)&pkg->message[1]);
			list->next = (Songlist*)malloc(sizeof(Songlist));
			old = list;
			list = (Songlist*)list->next;
			free(pkg->message);
			free(pkg);
			idx++;
		}
	}
	*totalSongs = idx;
	return head;
}

// drawBottomUI: Draws bottom screen UI
void drawBottomUI(){
	initBlend(UNDER);
	fillRect(5, 5, 310, 70, black);
	fillRect(7, 7, 306, 66, white);
	fillRect(5, 80, 310, 155, black);
	fillRect(7, 82, 306, 151, white);
	termBlend();
}

// drawTopUI: Draws top screen UI
void drawTopUI(){
	initBlend(UPPER);
	drawAsset(0, 0, logo);
	fillRect(0, 400, 220, 20, black);
	termBlend();
}

// drawSongList: Draws SongList
void drawSongList(Songlist* list){
	int y = 92;
	u32 tmp_idx = 1;
	Songlist* pointer = list;
	while (tmp_idx < idx){
		pointer = (Songlist*)pointer->next;
		tmp_idx++;
	}
	u32 st_idx = tmp_idx;
	tmp_idx = 0;
	u32 color;
	while (tmp_idx < 8){
		if (tmp_idx == 0) color = red;
		else color = black;
		F.drawString(10, y, pointer->name, Color((color >> 16) & 0xFF, (color >> 8) & 0xFF, (color) & 0xFF), BOTTOM_SCREEN, true);
		tmp_idx++;
		if (st_idx + tmp_idx > total) break;
		else{
			pointer = (Songlist*)pointer->next;
			y = y + 17;
		}
	}
}

int main(){

	// Init services
	srvInit();	
	aptInit();
	gfxInitDefault();
	hidInit(NULL);
	aptOpenSession();
	Result ret=APT_SetAppCpuTimeLimit(NULL, 30);
	aptCloseSession();
	CSND_initialize(NULL);
	isNinjhax2 = false;
	if (!hbInit()){
		khaxInit;
		hbExit();
	}else isNinjhax2 = true;
	initScene();
	
	// Variables definition
	Socket* Server = NULL;
	Socket* Client = NULL;
	bool socketingStatus = false;
	char IP[64];
	char alert[64];
	Songlist* pc_songs = NULL;
	bool welcomeSent = false;
	
	// Initializing resources
    unsigned char* buffer = F.loadFromMemory(main_ttf,size_main_ttf);
	F.setSize(16);
	logo = getLogo();
	idx = 1;
	
	// Main loop
	while(aptMainLoop()){
		hidScanInput();
		u32 pad = hidKeysDown();
		RefreshScreen();
		drawBottomUI();
		drawTopUI();
		if (isWifiOn()){		
			if (socketingStatus){
				if (Server == NULL) Server = createServerSocket(5000);
				else if (Client == NULL) Client = serverAccept(Server);
				else if (!welcomeSent){
					socketSend(Client, "WELCOME SMURF");
					welcomeSent = true;
				}else if (pc_songs == NULL){
					pc_songs = fetchSonglist(Client, &total);
					sprintf(alert, "Songs detected: %i", total);
				}else{
					drawSongList(pc_songs);				
					F.drawString(7, 222, alert, Color((white >> 16) & 0xFF, (white >> 8) & 0xFF, (white) & 0xFF), TOP_SCREEN, true);
					if ((pad & KEY_DUP) == KEY_DUP){
						idx--;
						if (idx < 1) idx = total;
					}else if ((pad & KEY_DDOWN) == KEY_DDOWN){
						idx++;
						if (idx > total) idx = 1;
					}else if ((pad & KEY_DRIGHT) == KEY_DRIGHT){
						idx = idx + 8;
						if (idx > total) idx = total;
					}else if ((pad & KEY_DLEFT) == KEY_DLEFT){
						idx = idx - 8;
						if (idx < 1) idx = 1;
					}
				}
			}else{
				initSocketing();
				socketingStatus = true;
			}
		}else{
		}
		getIPAddress(IP);
		char ip_addr[32];
		sprintf(ip_addr, "IP: %s", IP);
		F.drawString(257, 222, ip_addr, Color((white >> 16) & 0xFF, (white >> 8) & 0xFF, (white) & 0xFF), TOP_SCREEN, true);
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
		if ((pad & KEY_START) == KEY_START){
			free(logo->tex);
			free(logo);
			if (Client != NULL){
				socketSend(Client, "exec3:0000");
				socketClose(Client);
			}
			if (Server != NULL) socketClose(Server);
			break;
		}
	}
	
	// Term services
	if (!isNinjhax2) khaxExit();
	endScene();
	CSND_shutdown();
	if (socketingStatus) termSocketing();
	hidExit();
	gfxExit();
	aptExit();
	srvExit();

}
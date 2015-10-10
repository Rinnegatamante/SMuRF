#include <3ds.h>
#include "Network.h"
#include "Graphics.h"
#include "main_font.h"
#define TOP_SCREEN true
#define BOTTOM_SCREEN false
#define UNDER 1
#define UPPER 0

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
Songlist* fetchSonglist(Socket* Client){
	socketSend(Client, "exec0:0000");
	Packet* pkg = NULL;
	Songlist* head = NULL;
	Songlist* list = NULL;
	u32 idx = 1;
	bool notReceived = true;
	while (((pkg = socketRecv(Client, 128)) != NULL) || notReceived){	
		if (pkg == NULL) continue;
		notReceived = false;
		if (strncmp((char*)pkg->message, ":", 1) == 0){
			free(pkg->message);
			free(pkg);
			return NULL;
		}else{
			if (idx == 1){
				list = (Songlist*)malloc(sizeof(Songlist));
				head = list;
				idx++;
			}
			char tmp[2];
			tmp[0] = pkg->message[0];
			tmp[1] = 0;
			list->format = atoi(tmp);
			strcpy(list->name, (char*)&pkg->message[1]);
			if (pkg->message[127] == ':'){
				list->next = (Songlist*)malloc(sizeof(Songlist));
				list = (Songlist*)list->next;
			}else{
				list->next = NULL;
				free(pkg->message);
				free(pkg);
				break;
			}
			free(pkg->message);
			free(pkg);
		}
	}
	socketSend(Client, "OK");
	return head;
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
	
	// Variables definition
	Socket* Server = NULL;
	Socket* Client = NULL;
	bool socketingStatus = false;
	u32 white = 0xFFFFFFFF;
	char IP[64];
	Songlist* pc_songs = NULL;
	bool welcomeSent = false;
	u32 oldpad;
	
	// Initializing resources
	Font F;
    unsigned char* buffer = F.loadFromMemory(main_ttf,size_main_ttf);
	F.setSize(16);
	
	// Main loop
	while(aptMainLoop()){
		hidScanInput();
		u32 pad = hidKeysHeld();
		RefreshScreen();
		ClearScreen(UPPER);
		if (isWifiOn()){
			if (socketingStatus){
				if (Server == NULL) Server = createServerSocket(5000);
				else if (Client == NULL) Client = serverAccept(Server);
				else if (!welcomeSent){
					socketSend(Client, "WELCOME SMURF");
					welcomeSent = true;
				}else if (pc_songs == NULL) pc_songs = fetchSonglist(Client);
			}else{
				initSocketing();
				socketingStatus = true;
			}
		}else{
		
		}
		getIPAddress(IP);
		F.drawString(10, 50, IP, Color((white >> 16) & 0xFF, (white >> 8) & 0xFF, (white) & 0xFF), TOP_SCREEN, true);
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
		if ((pad & KEY_START) == KEY_START){
			socketSend(Client, "exec3:0000");
			socketClose(Client);
			socketClose(Server);
			break;
		}
		oldpad = pad;
	}
	
	// Term services
	CSND_shutdown();
	if (socketingStatus) termSocketing();
	hidExit();
	gfxExit();
	aptExit();
	srvExit();

}
#include <3ds.h>
#include "Network.h"
#include "Graphics.h"
#include "main_font.h"
#define TOP_SCREEN true
#define BOTTOM_SCREEN false
#define UNDER 1
#define UPPER 0

int main(){

	// Init services
	srvInit();	
	aptInit();
	gfxInitDefault();
	hidInit(NULL);
	aptOpenSession();
	Result ret=APT_SetAppCpuTimeLimit(NULL, 30);
	aptCloseSession();
	
	// Variables definition
	Socket* Server = NULL;
	Socket* Client = NULL;
	bool socketingStatus = false;
	u32 white = 0xFFFFFFFF;
	char IP[64];
	
	// Initializing resources
	Font F;
    unsigned char* buffer = F.loadFromMemory(main_ttf,size_main_ttf);
	F.setSize(16);
	
	// Main loop
	while(aptMainLoop()){
		RefreshScreen();
		ClearScreen(UPPER);
		if (isWifiOn()){
			if (socketingStatus){
				if (Server == NULL) Server = createServerSocket(5000);
				else if (Client == NULL) Client = serverAccept(Server);
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
	}
	
	// Term services
	if (socketingStatus) termSocketing();
	hidExit();
	gfxExit();
	aptExit();
	srvExit();

}
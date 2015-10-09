#include <3ds.h>
#include "Network.h"
#include "Graphics.h"

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
	
	// Main loop
	while(aptMainLoop()){
		RefreshScreen();
		if (isWifiOn()){
			if (Server == NULL) Server = createServerSocket(5000);
			else if (Client == NULL) Client = serverAccept(Server);
		}else{
		
		}
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	
	// Term services
	termSocketing();
	hidExit();
	gfxExit();
	aptExit();
	srvExit();

}
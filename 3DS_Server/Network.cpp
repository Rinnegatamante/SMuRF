#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <3ds.h>
#include <fcntl.h>
#include "Network.h"
u32* sockmem;

bool isWifiOn(){
	u32 wifiStatus;
	if (ACU_GetWifiStatus(NULL, &wifiStatus) ==  0xE0A09D2E) return false;
	else return wifiStatus;
	return 1;
}

char* getIPAddress(){
	u32 ip=(u32)gethostid();
	char ip_address[64];
	sprintf(ip_address,"%lu.%lu.%lu.%lu", ip & 0xFF, (ip>>8)&0xFF, (ip>>16)&0xFF, (ip>>24)&0xFF);
	return ip_address;
}

u8 getWifiLevel(){
	u8* wifi_link = (u8*)0x1FF81066;
	return *wifi_link;
}

void initSocketing(){
	sockmem = (u32*)memalign(0x1000, 0x100000);
	Result ret = SOC_Initialize(sockmem, 0x100000);
	if(ret != 0){
		SOC_Shutdown();
		free(sockmem);
	}
}

Socket* createServerSocket(int port){
	Socket* my_socket = (Socket*) malloc(sizeof(Socket));
	my_socket->serverSocket = true;

	my_socket->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (my_socket->sock <= 0) {
		return NULL;
	}

	my_socket->addrTo.sin_family = AF_INET;
	my_socket->addrTo.sin_port = htons(port);
	my_socket->addrTo.sin_addr.s_addr = 0;

	int err = bind(my_socket->sock, (struct sockaddr*)&my_socket->addrTo, sizeof(my_socket->addrTo));
	if (err != 0) {
		return NULL;
	}

	fcntl(my_socket->sock, F_SETFL, O_NONBLOCK);

	err = listen(my_socket->sock, 1);
	if (err != 0) {
		return NULL; 
	}
	
	return my_socket;
}

void termSocketing()
{
	SOC_Shutdown();
	free(sockmem);
}

Packet* socketRecv(Socket* my_socket, u32 size){

	if (my_socket->serverSocket) return NULL;

	u8* data = (u8*)malloc(size);
	int count = recv(my_socket->sock, (char*)data, size, 0);
	
	if (count == 0) return NULL;
	
	Packet* pkg = (Packet*)malloc(sizeof(Packet));
	pkg->message = data;
	pkg->size = count;
	return pkg;
}

void socketSend(Socket* my_socket, char* text){
	size_t size = strlen(text);
	if (my_socket->serverSocket) return;
	if (!text) return;
	send(my_socket->sock, text, size, 0);
}

Socket* serverAccept(Socket* my_socket)
{
	if (!my_socket->serverSocket) return NULL;

	struct sockaddr_in addrAccept;
	socklen_t cbAddrAccept = sizeof(addrAccept);
	u32 sockClient = accept(my_socket->sock, (struct sockaddr*)&addrAccept, &cbAddrAccept);
	if (sockClient <= 0) return NULL;

	Socket* incomingSocket = (Socket*) malloc(sizeof(Socket));
	incomingSocket->serverSocket = 0;
	incomingSocket->sock = sockClient;
	incomingSocket->addrTo = addrAccept;
	
	return incomingSocket;
}

void socketClose(Socket* my_socket)
{
	closesocket(my_socket->sock);
	free(my_socket);
}
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

// Structures definition
typedef struct{
	u32 magic;
	u32 sock;
	struct sockaddr_in addrTo;
	bool serverSocket;
} Socket;
typedef struct{
	u8* message;
	u32 size;
} Packet;

bool isWifiOn();
u8 getWifiLevel();
void getIPAddress(char* ip_address);
void initSocketing();
void termSocketing();
Socket* createServerSocket(int port);
Packet* socketRecv(Socket* my_socket, u32 size);
void socketSend(Socket* my_socket, char* text);
Socket* serverAccept(Socket* my_socket);
void socketClose(Socket* my_socket);
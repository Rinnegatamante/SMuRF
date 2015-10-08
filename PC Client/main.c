#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define u32 uint32_t
#define MAX_BUFFER_SIZE 524288

enum{
	SMURF_GET_SONG_LIST = 0,
	SMURF_GET_SONG,
	SMURF_UPDATE_CACHE,
};

typedef struct{
	u32 sock;
	struct sockaddr_in addrTo;
} Socket;

typedef struct{
	char name[256];
	void* next;
} songlist;

FILE* currentSong = NULL;
songlist* songList = NULL;

char* getSongFilename(songlist* list, int idx){
	int i = 0;
	songlist* song = list;
	if (song == NULL) return "";
	while (i < idx){
		if (song->next == NULL) return "";
		else{
			song = song->next;
			i++;
		}
	}
	return song->name;
}

int main(int argc,char** argv){

	// Getting IP
	char* host = (char*)(argv[1]);
	
	// Writing info on the screen
	printf("-------------------------\n");
	printf("SMuRF Command-Line Client\n");
	printf("Server: ");
	printf(host);
	printf("\n-------------------------\n\n");

	// Creating client socket
	Socket* my_socket = (Socket*) malloc(sizeof(Socket));
	memset(&my_socket->addrTo, '0', sizeof(my_socket->addrTo)); 
	my_socket->addrTo.sin_family = AF_INET;
	my_socket->addrTo.sin_port = htons(5000);
	my_socket->addrTo.sin_addr.s_addr = inet_addr(host);
	my_socket->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (my_socket->sock < 0){
		printf("\nFailed creating socket.");	
		return -1;
	}else printf("\nClient socket created on port 5000");
	
	// Connecting to server
	int err = connect(my_socket->sock, (struct sockaddr*)&my_socket->addrTo, sizeof(my_socket->addrTo));
	if (err < 0 ){ 
		printf("\nFailed connecting server.");
		close(my_socket->sock);
		return -1;
	}else printf("\nConnection estabilished, waiting for server response...");
	
	// Waiting for magic
	char data[13];
	int count = recv(my_socket->sock, &data, 13, 0);
	while (count < 0){
		count = recv(my_socket->sock, &data, 13, 0);
	}
	printf(data);
	if (strncmp(data,"WELCOME SMURF",13) == 0){
		printf("\n\n\n\n\nWelcome to SMuRF!\n\nWaiting for commands...\n\nDebug Console:");	
	}else{
		printf("\nWrong magic received, connection aborted.");
		close(my_socket->sock);
		return -1;
	}
	
	//SMuRF command listener
	char cmd[10];
	char cmd_id[2];
	count = recv(my_socket->sock, &cmd, 10, 0);
	while (1){
		count = recv(my_socket->sock, &cmd, 10, 0);
		if (count > 0){
			if (strstr(cmd, "exec")){
				cmd_id[0] = cmd[4];
				cmd_id[1] = 0;
				switch (atoi(cmd_id)){
					case SMURF_GET_SONG_LIST: // TODO
						break;
					case SMURF_GET_SONG:
						printf("\nGET_SONG: Opening song to stream...");
						if (currentSong != NULL) fclose(currentSong);
						char song_id[4];
						strncpy(song_id, &cmd[6], 3);
						song_id[3] = 0;
						char* filename = getSongFilename(songList, atoi(song_id));
						char file[512];
						sprintf(file,"./songs/%s",filename);
						currentSong = fopen(file, "r");
						if (currentSong < 0) printf(" File not found.");
						else{
							printf(" Done!");
							fseek(currentSong, 0, SEEK_END);
							int size = ftell(currentSong);
							int stream_size = size;
							while (stream_size > MAX_BUFFER_SIZE){
								stream_size = stream_size / 2;
							}
							printf("\nGET_SONG: Sending buffer size info...");
							char info[64];
							memset(&info, 0, 64);
							sprintf(info, "%i", stream_size);
							send(my_socket->sock, info, 64, 0);
							while (recv(my_socket->sock, NULL, 2, 0) < 1){}
							printf(" Done!");
							fseek(currentSong, 0, SEEK_SET);
							char* buffer = malloc(stream_size);
							fread(buffer, stream_size, 1, currentSong);
							printf("\nGET_SONG: Sending first block...");
							send(my_socket->sock, buffer, size, 0);
							while (recv(my_socket->sock, NULL, 2, 0) < 1){}
							printf(" Done!");
						}
						break;
					case SMURF_UPDATE_CACHE: // TODO
						if (currentSong == NULL) printf("\nERROR: No opened song!");
						break;
				}
			}else printf("ERROR: Invalid command! (%s)\n",cmd);
			memset(&cmd, 0, 10);
		}
	}
	
}
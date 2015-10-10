#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define u32 uint32_t

// Supported formats list
enum{
	WAV_PCM16 = 0,
	OGG_VORBIS = 1,
	AIFF_PCM16 = 2,
};

// Structures definition
typedef struct{
	u32 sock;
	struct sockaddr_in addrTo;
} Socket;
typedef struct{
	char name[256];
	uint8_t format;
	void* next;
} songlist;

// Constants and "pseudo"-constants definition
#define MAX_BUFFER_SIZE 524288
u32 BUFFER_SIZE;

// Commands list
enum{ 
	SMURF_GET_SONG_LIST = 0,
	SMURF_GET_SONG,
	SMURF_UPDATE_CACHE,
	SMURF_ABORT_CONNECTION,
};

// Globals definition
FILE* currentSong = NULL;
songlist* songList = NULL;
uint8_t closing = 0;

// getSongFilename: Retrieve filename for a selected song
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

// populateDatabase: Full songlist with songs in a directory
int populateDatabase(DIR* folder){
	int idx = 0;
	songlist* pointer = NULL;
	for (;;){
		struct dirent* curFile;
		if ((curFile = readdir(folder)) != NULL) {
			char fullName[512];
			sprintf(fullName, "./songs/%s", curFile->d_name);
			FILE* file = fopen(fullName,"r");
			if (file < 0) continue;
			else{
				if (idx == 0){
					songList = malloc(sizeof(songlist));
					pointer = songList; 
				}else{
					pointer->next = malloc(sizeof(songlist));
					pointer = pointer->next;
				}
				uint32_t magic;
				fread(&magic, 4, 1, file);
				fclose(file);
				switch (magic){
					case 0x46464952:
						pointer->format = WAV_PCM16;
						break;
					case 0x5367674F:
						pointer->format = OGG_VORBIS;
						break;
					case 0x2E736E64:
						pointer->format = AIFF_PCM16;
						break;
				}
			}
			strcpy(pointer->name,curFile->d_name);
		}else break;
		idx++;
	}
	pointer->next = NULL;
	return idx;
}

// parseSongs: Prepare a query with songs filenames
char* parseSongs(songlist* list, int songs){
	songlist* curFile = list;
	char* query = malloc(128*songs);
	int idx = 0;
	while (idx < songs){
		char filename[128];
		strncpy(filename, curFile->name, 127);
		if ((idx + 1) == songs) filename[127] = 0;
		else filename[127] = ':';
		strncpy(&query[idx*128], filename, 128); 
		idx++;
	}
	return query;
}

int main(int argc,char** argv){

	// Getting IP
	char* host = (char*)(argv[1]);
	
	// Writing info on the screen
	printf("+-------------------------+\n");
	printf("|SMuRF Command-Line Client|\n");
	printf("|    Version: 0.1 BETA    |");
	printf("\n+-------------------------+\n\n");

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
		
			// Commands parser
			if (strstr(cmd, "exec")){
				cmd_id[0] = cmd[4];
				cmd_id[1] = 0;
				switch (atoi(cmd_id)){
				
					// Retrive songs list
					case SMURF_GET_SONG_LIST:
						printf("\nGET_SONG_LIST: Retrieving songs list...");
						DIR* songsdir = opendir("./songs");
						if (songsdir == NULL) printf("\nERROR: Cannot find songs directory!");
						int totalSongs = populateDatabase(songsdir);
						printf(" Done!");
						int dim;
						char* query;
						if (totalSongs == 0){ 
							query = malloc(2);
							query[0] = ":";
							query[1] = 0;
							dim = 2;
						}else{
							char* query = parseSongs(songList, totalSongs);
							dim = 128*totalSongs;
						}
						printf("\nGET_SONG_LIST: Sending songs list...");
						send(my_socket->sock, query, dim, 0);
						while (recv(my_socket->sock, NULL, 2, 0) < 1){}
						printf(" Done!");
						free(query);
						break;
						
					// Fetch selected song
					case SMURF_GET_SONG:
						printf("\nGET_SONG: Opening song to stream...");
						if (currentSong != NULL) fclose(currentSong);
						char song_id[4];
						strncpy(song_id, &cmd[6], 3);
						song_id[3] = 0;
						char* filename = getSongFilename(songList, atoi(song_id));
						if (strcmp(filename, "") == 0){
							printf("\nERROR: Cannot retrieve selected filename.");
							break;
						}
						char file[512];
						sprintf(file,"./songs/%s",filename);
						currentSong = fopen(file, "r");
						if (currentSong < 0) printf("\nERROR: File not found.");
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
							int bytesRead = fread(buffer, stream_size, 1, currentSong);
							printf("\nGET_SONG: Sending first block...");
							send(my_socket->sock, buffer, size, 0);
							while (recv(my_socket->sock, NULL, 2, 0) < 1){}
							printf(" Done! (%i bytes)", bytesRead);
							BUFFER_SIZE = stream_size;
							free(buffer);
						}
						break;
						
					// Update locale stream cache
					case SMURF_UPDATE_CACHE:
						if (currentSong == NULL) printf("\nERROR: No opened song!");
						char* buffer = malloc(BUFFER_SIZE);
						int bytesRead = fread(buffer, BUFFER_SIZE, 1, currentSong);
						printf("\nUPDATE_CACHE: Sending next block...");
						send(my_socket->sock, buffer, bytesRead, 0);
						while (recv(my_socket->sock, NULL, 2, 0) < 1){}
						printf(" Done! (%i bytes)", bytesRead);
						if (bytesRead < BUFFER_SIZE){
							printf("\nUPDATE_CACHE: Sending end song command...");
							send(my_socket->sock, "EOF", 3, 0);
							while (recv(my_socket->sock, NULL, 2, 0) < 1){}
							printf(" Done!");
						}else{
							printf("\nUPDATE_CACHE: Sending next block command...");
							send(my_socket->sock, "OK!", 3, 0);
							while (recv(my_socket->sock, NULL, 2, 0) < 1){}
							printf(" Done!");
						}
						free(buffer);
						
					// Close SMuRF
					case SMURF_ABORT_CONNECTION:
						printf("\nABORT_CONNECTION: Closing opened resources...");
						if (currentSong != NULL) fclose(currentSong);
						close(my_socket->sock);
						printf(" Done!");
						printf("\n\n\nBye bye!");
						break;
					
				}
			}else printf("ERROR: Invalid command! (%s)\n",cmd);
			
			// Closing application or resetting command listener memory
			if (closing) break;
			else memset(&cmd, 0, 10);
			
		}
	}
	
}
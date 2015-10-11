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

// Command sample: exec1:200

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
#define bool int
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

// print: Print a string on stdout
void print(char* text){
	write(1, text, strlen(text));
}

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

bool isDir(const char* target){
   struct stat statbuf;
   stat(target, &statbuf);
   return S_ISDIR(statbuf.st_mode);
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
			if (isDir(fullName)) continue;
			else{
				FILE* file = fopen(fullName, "r");
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
	char* query = malloc(128*songs + 1);
	int idx = 0;
	while (idx < songs){
		char filename[128];
		sprintf(filename, "%i%s", curFile->format, curFile->name);
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
	print("+-------------------------+\n");
	print("|SMuRF Command-Line Client|\n");
	print("|    Version: 0.1 BETA    |\n");
	print("+-------------------------+\n\n");

	// Creating client socket
	Socket* my_socket = (Socket*) malloc(sizeof(Socket));
	memset(&my_socket->addrTo, '0', sizeof(my_socket->addrTo)); 
	my_socket->addrTo.sin_family = AF_INET;
	my_socket->addrTo.sin_port = htons(5000);
	my_socket->addrTo.sin_addr.s_addr = inet_addr(host);
	my_socket->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (my_socket->sock < 0){
		print("\nFailed creating socket.");	
		return -1;
	}else print("\nClient socket created on port 5000.");
	
	// Connecting to server
	int err = connect(my_socket->sock, (struct sockaddr*)&my_socket->addrTo, sizeof(my_socket->addrTo));
	if (err < 0 ){ 
		print("\nFailed connecting server.");
		close(my_socket->sock);
		return -1;
	}else print("\nConnection estabilished, waiting for server response...");
	
	fcntl(my_socket->sock, F_SETFL, O_NONBLOCK);
	
	// Waiting for magic
	char data[13];
	int count = recv(my_socket->sock, &data, 13, 0);
	while (count < 0){
		count = recv(my_socket->sock, &data, 13, 0);
	}
	if (strncmp(data,"WELCOME SMURF",13) == 0){
		print("\n\n\n\n\nWelcome to SMuRF!\n\nWaiting for commands...\n\nDebug Console:\n");	
	}else{
		print("\nWrong magic received, connection aborted.");
		close(my_socket->sock);
		return -1;
	}
	
	//SMuRF command listener
	char cmd[10];
	char cmd_id[2];
	while (1){
		count = recv(my_socket->sock, &cmd, 10, 0);
		if (count > 0){
		
			// Commands parser
			if (strstr(cmd, "exec")){
				cmd_id[0] = cmd[4];
				cmd_id[1] = 0;
				uint8_t cmd_type = atoi(cmd_id);
				
					// Retrieve songs list
					if (cmd_type == SMURF_GET_SONG_LIST){
						print("\nGET_SONG_LIST: Retrieving songs list...");
						DIR* songsdir = opendir("./songs");
						if (songsdir == NULL) print("\nERROR: Cannot find songs directory!");
						else{
							int totalSongs = populateDatabase(songsdir);
							print(" Done!\n");
							int dim;
							char* query;
							if (totalSongs == 0){ 
								query = malloc(2);
								query[0] = ':';
								query[1] = 0;
								dim = 1;
							}else{
								query = parseSongs(songList, totalSongs);
								dim = 128*totalSongs;
							}
							print("GET_SONG_LIST: Sending songs list...");
							write(my_socket->sock, query, dim);
							print(" Done!");
							free(query);
						}
						
					// Fetch selected song
					}else if (cmd_type == SMURF_GET_SONG){
						print("\nGET_SONG: Opening song to stream...");
						if (currentSong != NULL) fclose(currentSong);
						char song_id[4];
						strncpy(song_id, &cmd[6], 3);
						song_id[3] = 0;
						char* filename = getSongFilename(songList, atoi(song_id));
						if (strcmp(filename, "") == 0){
							print("\nERROR: Cannot retrieve selected filename.");
						}else{
							char file[512];
							sprintf(file,"./songs/%s",filename);
							currentSong = fopen(file, "r");
							if (currentSong < 0) print("\nERROR: File not found.");
							else{
								print(" Done!");
								fseek(currentSong, 0, SEEK_END);
								int size = ftell(currentSong);
								int stream_size = size;
								while (stream_size > MAX_BUFFER_SIZE){
									stream_size = stream_size / 2;
								}
								print("\nGET_SONG: Sending buffer size info...");
								char info[64];
								memset(&info, 0, 64);
								sprintf(info, "%i", stream_size);
								write(my_socket->sock, info, 64);
								while (recv(my_socket->sock, NULL, 2, 0) < 1){}
								print(" Done!");
								fseek(currentSong, 0, SEEK_SET);
								char* buffer = malloc(stream_size);
								int bytesRead = fread(buffer, stream_size, 1, currentSong);
								print("\nGET_SONG: Sending first block...");
								write(my_socket->sock, buffer, size);
								while (recv(my_socket->sock, NULL, 2, 0) < 1){}
								print(" Done!");
								BUFFER_SIZE = stream_size;
								free(buffer);
							}
						}
						
					// Update locale stream cache
					}else if (cmd_type == SMURF_UPDATE_CACHE){
						if (currentSong == NULL) print("\nERROR: No opened song!");
						else{
							char* buffer = malloc(BUFFER_SIZE);
							int bytesRead = fread(buffer, BUFFER_SIZE, 1, currentSong);
							print("\nUPDATE_CACHE: Sending next block...");
							write(my_socket->sock, buffer, bytesRead);
							while (recv(my_socket->sock, NULL, 2, 0) < 1){}
							print(" Done!");
							if (bytesRead < BUFFER_SIZE){
								print("\nUPDATE_CACHE: Sending end song command...");
								write(my_socket->sock, "EOF", 3);
								while (recv(my_socket->sock, NULL, 2, 0) < 1){}
								print(" Done!");
							}else{
								print("\nUPDATE_CACHE: Sending next block command...");
								write(my_socket->sock, "OK!", 3);
								while (recv(my_socket->sock, NULL, 2, 0) < 1){}
								print(" Done!");
							}
							free(buffer);
						}
						
					// Close SMuRF
					}else if (cmd_type == SMURF_ABORT_CONNECTION){
						print("\nABORT_CONNECTION: Closing opened resources...");
						if (currentSong != NULL) fclose(currentSong);
						close(my_socket->sock);
						print(" Done!");
						print("\n\n\nBye bye!");
						break;
					}
					
			}else{
				print("\nERROR: Invalid command! ( ");
				print(cmd);
				print(" )\n");
			}
			
			// Closing application or resetting command listener memory
			if (closing) break;
			else memset(&cmd, 0, 10);
			
		}
	}
	
	return 0;
}
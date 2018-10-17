#include <vitasdk.h>
#include <vita2d.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define checkButton(x) ((pad.buttons & x) && (!(oldpad & x)))

// Supported formats list
enum{
	WAV_PCM16 = 0,
	OGG_VORBIS = 1,
	AIFF_PCM16 = 2,
};

// Songs list struct
typedef struct{
	char name[256];
	uint8_t format;
	void *next;
} songlist;

// Song struct
typedef struct{
	char title[256];
	char author[256];
	uint8_t big_endian;
	uint32_t samplerate;
	uint16_t audiotype;
	char total_time[64];
	uint32_t start_tick;
	uint8_t audiobuf[2][32768];
	uint8_t idx;
	uint8_t playing;
} music;

// Network packet struct
typedef struct{
	uint8_t *message;
	uint32_t size;
} packet;

enum {
	BOOTSCREEN,
	SONGS_LIST
};

// Songs list
songlist *pc_songs = NULL;
uint32_t total;
int idx = 1;
volatile music *cur_song = NULL;

// Font related stuffs
vita2d_pgf *font = NULL;
uint32_t white;
uint32_t green;

// sceNet memory related stuffs
static void *net_memory = NULL;
#define NET_INIT_SIZE 1*1024*1024
int client = -1;

// Vita IP info
static unsigned long myAddr;
static char vita_ip[16];

// socketSend: Send a message to a client socket
void socketSend(int sock, char *msg){
	sceNetSend(sock, msg, strlen(msg), 0);
}

packet *socketRecv(int sock, uint32_t size){
	uint8_t *data = (uint8_t*)malloc(size);
	int count = sceNetRecv(sock, data, size, 0);
	if (count <= 0) return NULL;
	
	packet *pkg = (packet*)malloc(sizeof(packet));
	pkg->message = data;
	pkg->size = count;
	return pkg;
}

int socketRecvFast(int sock, uint8_t* addr, uint32_t size){
	memset(addr, 0, size);
	int count = 0;
	do {
		count = sceNetRecv(sock, addr, size, 0);
	}while(count <= 0);
	return count;
}

void format_time(char *dst, uint32_t src){
	uint32_t mins = floor(src / 60);
	uint32_t secs = src % 60;
	uint32_t hrs = floor(mins / 60);
	mins = mins % 60;
	if (hrs > 0) sprintf(dst, "%02lu:%02lu:%02lu", hrs, mins, secs);
	else sprintf(dst, "%02lu:%02lu", mins, secs);
}

// fetchSonglist: Gets songlist from a client
songlist* fetchSonglist(int client, uint32_t* total_songs){
	socketSend(client, "exec0:0000");
	packet *pkg = NULL;
	songlist *head = NULL;
	songlist *list = NULL;
	songlist *old = NULL;
	uint32_t idx = 0;
	*total_songs = 0;
	int notReceived = 1;
	while (((pkg = socketRecv(client, 128)) != NULL) || notReceived){
		if (pkg == NULL){
			if (idx == 0) continue;
			else{
				old->next = NULL;
				free(list);
				break;
			}
		}
		notReceived = 0;
		if (pkg->size == 1){
			free(pkg->message);
			free(pkg);
			return NULL;
		}else{
			if (idx == 0){
				list = (songlist*)malloc(sizeof(songlist));
				head = list;
			}
			char tmp[2];
			tmp[0] = pkg->message[0];
			tmp[1] = 0;
			list->format = atoi(tmp);
			strcpy(list->name, (char*)&pkg->message[1]);
			list->next = (songlist*)malloc(sizeof(songlist));
			old = list;
			list = (songlist*)list->next;
			free(pkg->message);
			free(pkg);
			idx++;
		}
	}
	*total_songs = idx;
	return head;
}

// drawSongList: Draws SongList
void drawSongList(songlist* list){
	int y = 170;
	uint32_t tmp_idx = 1;
	songlist* pointer = list;
	while (tmp_idx < idx){
		pointer = (songlist*)pointer->next;
		tmp_idx++;
	}
	uint32_t st_idx = tmp_idx;
	tmp_idx = 0;
	uint32_t color;
	while (tmp_idx < 18){
		if (tmp_idx == 0) color = green;
		else color = white;
		vita2d_pgf_draw_text(font, 10, y, color, 1.0, pointer->name);
		tmp_idx++;
		if (st_idx + tmp_idx > total) break;
		else{
			pointer = (songlist*)pointer->next;
			y = y + 20;
		}
	}
}

// getSong: Retrieve entry for a selected song
songlist *getSong(uint32_t idx){
	uint32_t i = 0;
	songlist *song = pc_songs;
	if (song == NULL) return NULL;
	while (i < idx){
		if (song->next == NULL) return NULL;
		else{
			song = (songlist*)song->next;
			i++;
		}
	}
	return song;
}

// prepareSong: Receive a song over network
void prepareSong(int client, uint32_t idx){
	
	// Init resources
	if (cur_song != NULL){
		music *old_song = cur_song;
		cur_song = NULL;
		sceKernelDelayThread(10000);
		free(old_song);
	}
	uint16_t bps;
	uint32_t header_size;
	packet *pkg = NULL;
	music *ret = (music*)malloc(sizeof(music));
	
	// Sending command to client
	char cmd[10];
	sprintf(cmd, "exec1:%lu", (idx - 1));
	cmd[9] = 0;
	socketSend(client, cmd);
	
	// Getting file info
	int msg_size = 0;
	while (msg_size != 64){
		pkg = NULL;
		while (pkg == NULL) pkg = socketRecv(client, 32768);
		msg_size = pkg->size;
	}
	songlist *song = getSong(idx);
	uint64_t filesize = atoi((char*)pkg->message);
	
	socketSend(client, "OK");
	if (song->format == WAV_PCM16){
		
		// Getting and parsing header file
		free(pkg->message);
		free(pkg);
		pkg = NULL;
		while (pkg == NULL) pkg = socketRecv(client, 256);
		socketSend(client, "OK");
		uint8_t *header = pkg->message;
		header_size = pkg->size;
		uint32_t jump, chunk = 0;
		strcpy(ret->author, "");
		strcpy(ret->title, "");
		ret->big_endian = 0;
		uint32_t pos = 16;
		while (chunk != 0x61746164){
			memcpy(&jump, &header[pos], 4);
			pos=pos+4+jump;
			memcpy(&chunk, &header[pos], 4);
			pos=pos+4;
			
			//Chunk LIST detection
			if (chunk == 0x5453494C){
				uint32_t chunk_size;
				uint32_t subchunk;
				uint32_t subchunk_size;
				uint32_t sub_pos = pos+4;
				memcpy(&subchunk, &header[sub_pos], 4);
				if (subchunk == 0x4F464E49){
					sub_pos = sub_pos+4;
					memcpy(&chunk_size, &header[pos], 4);
					while (sub_pos < (chunk_size + pos + 4)){
						memcpy(&subchunk, &header[sub_pos], 4);
						memcpy(&subchunk_size, &header[sub_pos + 4], 4);
						if (subchunk == 0x54524149){
							strncpy(ret->author, (char*)&header[sub_pos + 8], subchunk_size);
							ret->author[subchunk_size] = 0;
						}else if (subchunk == 0x4D414E49){
							strncpy(ret->title, (char*)&header[sub_pos + 8], subchunk_size);
							ret->title[subchunk_size] = 0;
						}
						sub_pos = sub_pos + 8 + subchunk_size;
						uint8_t checksum;
						memcpy(&checksum, &header[sub_pos], 1);
						if (checksum == 0) sub_pos++;
					}
				}
			}
			
		}
		memcpy(&ret->audiotype, &header[22], 2);
		memcpy(&ret->samplerate, &header[24], 4);
		memcpy(&bps, &header[32], 2);
		uint32_t total_time = (filesize - header_size) / (bps * ret->samplerate);
		format_time(ret->total_time, total_time);
		ret->idx = 0;
		ret->playing = 1;
		
		socketRecvFast(client, ret->audiobuf[0], 32768);
		cur_song = ret;

	}
	
}

// Audio thread code
static int audioThread(unsigned int args, void* arg){
	
	music *old_music = NULL;
	
	int ch = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 8192, 48000, SCE_AUDIO_OUT_MODE_STEREO);
	
	// Setting audio channel volume
	int vol_stereo[] = {32767, 32767};
	sceAudioOutSetVolume(ch, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol_stereo);
	
	for (;;){
		if (cur_song != NULL && cur_song->playing){
			if (cur_song != old_music) sceAudioOutSetConfig(ch, 16384 / cur_song->audiotype, cur_song->samplerate, cur_song->audiotype == 1 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
			sceAudioOutOutput(ch, cur_song->audiobuf[cur_song->idx]);
			cur_song->idx = (cur_song->idx + 1) % 2;
			socketSend(client, "exec2:0000");
			int count = socketRecvFast(client, cur_song->audiobuf[cur_song->idx], 32768);
			if (count <= 4){
				free(cur_song);
				cur_song = NULL;
			}
			old_music = cur_song;
		}else sceKernelDelayThread(100);
	}
	
	return 0;
}

int main(){
	
	int state = BOOTSCREEN;
	char alert[64];
	
	white = RGBA8(0xFF, 0xFF, 0xFF, 0xFF);
	green = RGBA8(0x00, 0xFF, 0x00, 0xFF);
	
	SceNetInitParam initparam;
	
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
	font = vita2d_load_default_pgf();
	
	// Start SceNet & SceNetCtl
	int ret = sceNetShowNetstat();
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		net_memory = malloc(NET_INIT_SIZE);

		initparam.memory = net_memory;
		initparam.size = NET_INIT_SIZE;
		initparam.flags = 0;

		ret = sceNetInit(&initparam);
		if (ret < 0) return -1;
		
	}	
	ret = sceNetCtlInit();
	if (ret < 0){
		sceNetTerm();
		free(net_memory);
		return -1;
	}
	
	// Getting IP address
	SceNetCtlInfo info;
	sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
	strcpy(vita_ip, info.ip_address);
	sceNetInetPton(SCE_NET_AF_INET, vita_ip, &myAddr);
	
	// Creating server socket
	int server = sceNetSocket("Server Socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	SceNetSockaddrIn addrTo;
	addrTo.sin_family = SCE_NET_AF_INET;
	addrTo.sin_port = sceNetHtons(5000);
	addrTo.sin_addr.s_addr = 0;
	sceNetBind(server, (SceNetSockaddr*)&addrTo, sizeof(addrTo));
	
	// Setting server socket as non-blocking
	int _true = 1;
	sceNetSetsockopt(server, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &_true, sizeof(_true));
	sceNetListen(server, 1);
	
	// Client socket info
	SceNetSockaddrIn addrAccept;
	unsigned int cbAddrAccept = sizeof(addrAccept);
	
	// Spawning audio thread
	SceUID thd = sceKernelCreateThread("Audio Thread", &audioThread, 0x10000100, 0x10000, 0, 0, NULL);
	sceKernelStartThread(thd, 0, NULL);
	
	SceCtrlData pad;
	uint32_t oldpad;
	
	while (1){
		vita2d_start_drawing();
		vita2d_clear_screen();
		switch (state){
		case BOOTSCREEN:
			vita2d_pgf_draw_textf(font, 10, 25, green, 1.0, "Welcome to Simple Music Reproducer & Fetcher (SMuRF) v.0.1", vita_ip);
			vita2d_pgf_draw_textf(font, 10, 85, white, 1.0, "Please insert the following IP in SMuRF client:", vita_ip);
			vita2d_pgf_draw_textf(font, 10, 105, white, 1.0, "IP: %s", vita_ip);
			client = sceNetAccept(server, (SceNetSockaddr*)&addrAccept, &cbAddrAccept);
			if (client > 0){
				socketSend(client, "WELCOME SMURF");
				pc_songs = fetchSonglist(client, &total);
				sprintf(alert, "Songs detected: %lu", total);
				state = SONGS_LIST;
			}
			break;
		case SONGS_LIST:
			vita2d_pgf_draw_text(font, 10, 25, green, 1.0, alert);
			vita2d_pgf_draw_text(font, 10, 45, white, 1.0, "Press Cross to open selected song.");
			vita2d_draw_line(0, 70, 960, 70, white);
			vita2d_draw_line(0, 150, 960, 150, white);
			drawSongList(pc_songs);
			
			if (cur_song != NULL){
				vita2d_pgf_draw_text(font, 10, 65, white, 1.0, "Press Square to pause/resume song.");
				vita2d_pgf_draw_text(font, 10, 85, green, 1.0, cur_song->title);
				vita2d_pgf_draw_text(font, 10, 105, white, 1.0, cur_song->author);
				vita2d_pgf_draw_textf(font, 10, 125, white, 1.0, "Samplerate: %lu Hz    Duration: %s", cur_song->samplerate, cur_song->total_time);
				vita2d_pgf_draw_textf(font, 10, 145, white, 1.0, "Audiotype: %s", cur_song->audiotype == 1 ? "Mono" : "Stereo");
				if (!cur_song->playing) vita2d_pgf_draw_text(font, 870, 85, white, 1.0, "PAUSED");
			}else vita2d_pgf_draw_text(font, 10, 85, green, 1.0, "No song opened");
			
			sceCtrlPeekBufferPositive(0, &pad, 1);
			if checkButton(SCE_CTRL_UP){
				idx--;
				if (idx < 1) idx = total;
			}else if checkButton(SCE_CTRL_DOWN){
				idx++;
				if (idx > total) idx = 1;
			}else if checkButton(SCE_CTRL_RIGHT){
				idx = idx + 18;
				if (idx > total) idx = total;
			}else if checkButton(SCE_CTRL_LEFT){
				idx = idx - 18;
				if (idx < 1) idx = 1;
			}else if checkButton(SCE_CTRL_CROSS){
				prepareSong(client, idx);
			}else if checkButton(SCE_CTRL_SQUARE){
				if (cur_song != NULL) cur_song->playing = (cur_song->playing + 1) % 2;
			}
			
			break;
		default:
			break;
		}
		
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
		oldpad = pad.buttons;
	}
	
	vita2d_fini();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	return 0;
	
}
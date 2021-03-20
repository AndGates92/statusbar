#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <fcntl.h>

#include <alsa/asoundlib.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#define STATUS_LENGTH 500
#define VOLUME_BOUND 100
#define MAX_LINE_LENGTH 100

#define RESOURCE_LENGTH 100
#define UNIT_LENGTH 100

static Display *dpy;

int str_to_int (int k, char str[]){
	int l = 0;
	int int_conv = 0;
	for (l = 0; l < k; l++){
		int_conv = int_conv + ((str[k-l-1]-'0')*((int) pow(10.0,(double)l)));
	}
	return int_conv;
}

int mpc_stat(char **mpc_vol, char **mpc_state, char **mpc_song){

	FILE *file_mpc_state = NULL;

	char line[MAX_LINE_LENGTH];
	char song_num_char[4];
	int i = 0;
	int k = 0;
	int song_num = 0;
	bool song_num_found = false;

	file_mpc_state = fopen("/var/lib/mpd/state", "r");
	if(file_mpc_state == NULL){
		fprintf(stderr, "Error opening mpc state file");
		return -1;
	}

	while((fgets(line, MAX_LINE_LENGTH, file_mpc_state)) != NULL){
		if(i == 0){
			int j = 0;
			while(line[j] < '0' || line[j] > '9'){
				j++;
			}
			strcpy(*mpc_vol,&line[j]);
			if(*(*mpc_vol+((int)strlen(*mpc_vol))-1) <= 32) {
				*(*mpc_vol + (((int)strlen(*mpc_vol))-1)) = 0;
			}
		} else if (i == 2){
			int j = 0;
			while(line[j] != ':'){
				j++;
			}
			strcpy(*mpc_state, &line[j+2]);
			if(*(*mpc_state+((int)strlen(*mpc_state))-1) <= 32) {
				*(*mpc_state + (((int)strlen(*mpc_state))-1)) = 0;
			}
		} else if (i == 3){
			int j = 0;
			while(line[j] < '0' || line[j] > '9'){
				j++;
			}
			while(line[j] >= '0' && line[j] <= '9'){
				song_num_char[k] = line[j];
				k++;
				j++;
			}
			song_num = str_to_int(k, song_num_char);
			song_num_found = true;
		} else if (song_num_found && line[k-1] >= '0' && line[k-1] <= '9'){
			if (song_num == str_to_int(k, line)){
				strcpy(*mpc_song, &line[k+1]);
				if(*(*mpc_song+((int)strlen(*mpc_song))-1) <= 32) {
					*(*mpc_song + (((int)strlen(*mpc_song))-1)) = 0;
				}
			}
		}
		i++;
	}

	fclose(file_mpc_state);

	return 1;
}

int getcpuusage(float* cpu_avg_freq) {
	FILE *file_cpu_usage = NULL;
	char line[MAX_LINE_LENGTH];

	char* resource1;
	char* resource2;
	char* colon;
	float value;

	float cpu_freq_sum;
	int num_proc;

	if((resource1 = malloc(sizeof(char)*RESOURCE_LENGTH)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for resource1.\n");
		exit(1);
	}
	if((resource2 = malloc(sizeof(char)*RESOURCE_LENGTH)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for resource2.\n");
		exit(1);
	}
	if((colon = malloc(sizeof(char)*RESOURCE_LENGTH)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for colon.\n");
		exit(1);
	}
	file_cpu_usage = fopen("/proc/cpuinfo", "r");

	if(file_cpu_usage == NULL){
		fprintf(stderr, "Error opening /proc/cpuinfo");
		return -1;
	}

	num_proc=0;
	cpu_freq_sum=0;

	while((fgets(line, MAX_LINE_LENGTH, file_cpu_usage)) != NULL){
		sscanf(line, "%s %s %s %f", resource1, resource2, colon, &value);

		if ((!(strcmp(resource1, "cpu"))) && (!(strcmp(resource2, "MHz")))) {
			cpu_freq_sum+=value;
			num_proc++;
		}
	}

	*cpu_avg_freq = ((float)cpu_freq_sum/(float)num_proc);

	fclose(file_cpu_usage);

	return 1;
}

int getmemoryusage(int* mem_available, int* mem_used) {
	FILE *file_mem_usage = NULL;
	char line[MAX_LINE_LENGTH];

	char* resource;
	long value;
	char* unit;

	char* found_char=NULL;
	char  search_char=':';

	size_t pos_char;

	long mem_total_kb;
	long mem_available_kb;

	if((resource = malloc(sizeof(char)*RESOURCE_LENGTH)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for resource.\n");
		exit(1);
	}

	if((unit = malloc(sizeof(char)*UNIT_LENGTH)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for unit.\n");
		exit(1);
	}

	file_mem_usage = fopen("/proc/meminfo", "r");

	if(file_mem_usage == NULL){
		fprintf(stderr, "Error opening /proc/meminfo");
		return -1;
	}

	mem_total_kb=0;
	mem_available_kb=0;

	while((fgets(line, MAX_LINE_LENGTH, file_mem_usage)) != NULL){
		sscanf(line, "%s %ld %s", resource, &value, unit);

		found_char = strchr(resource, search_char);
		if(found_char != NULL) {
			pos_char=found_char-resource;
			resource[pos_char]='\0';
		}

		if (!(strcmp(resource, "MemTotal"))) {
			mem_total_kb=value;
		} else if (!(strcmp(resource, "MemAvailable"))) {
			mem_available_kb=value;
		}
	}

	*mem_available = ((float)mem_available_kb/(float)mem_total_kb)*100;
	*mem_used = 100 - (*mem_available);

	fclose(file_mem_usage);

	return 1;
}

int net(char **net_intf, char **ip)
{
	int fd, intf_sel;
	struct ifreq ifr;
	int i = 0;
	int connected = 0;

	char intf_state[4];
	char* file_path = "/sys/class/net";
	char full_search_path[100];

	FILE *file_intf_up = NULL;

	int sel = 3;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	ifr.ifr_addr.sa_family = AF_INET;

	char* intf_scan[2] = {"eno2", "wlo1"};


	for(i=0; i < 2; i++){

		sprintf(full_search_path, "%s/%s/link_mode", file_path, intf_scan[i]);
		file_intf_up = fopen(full_search_path, "r");
		if (file_intf_up == NULL){
			fprintf(stderr, "Error opening link_mode for interface %s\n", intf_scan[i]);
			return -1;
		}
		fscanf(file_intf_up, "%d", &intf_sel);
		fclose(file_intf_up);

		sprintf(full_search_path, "%s/%s/operstate", file_path, intf_scan[i]);
		file_intf_up = fopen(full_search_path, "r");
		if (file_intf_up == NULL){
			fprintf(stderr, "Error opening operstate for interface %s\n", intf_scan[i]);
			return -1;
		}
		fscanf(file_intf_up, "%s", intf_state);
		fclose(file_intf_up);

		if(intf_sel == 1 && !(strcmp(intf_state, "up"))){
			sel = i;
			connected = 1;
		}
	}

	if (connected == 1){
		strncpy(ifr.ifr_name, intf_scan[sel], IFNAMSIZ-1);
		ioctl(fd, SIOCGIFADDR, &ifr);
		*net_intf = intf_scan[sel];
		*ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	} else {
		*net_intf = NULL;
		*ip = NULL;
	}

	close(fd);

	return connected;

}

void setstatus(char *str) {
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

float getfreq(char *file) {
	FILE *fd;
	char *freq; 
	float ret;

	freq = malloc(10);
	fd = fopen(file, "r");
	if(fd == NULL) {
		fprintf(stderr, "Cannot open '%s' for reading.\n", file);
		exit(1);
	}

	fgets(freq, 10, fd);
	fclose(fd);

	ret = atof(freq)/1000000;
	free(freq);
	return ret;
}

int getvolume(long* left_volume, long* right_volume) {
	int ret = 0;

	snd_mixer_t* handle;
	snd_mixer_elem_t* elem;
	snd_mixer_selem_id_t* sid;

	static const char* mix_name = "Master";
	static const char* card = "default";
	static int mix_index = 0;

	snd_mixer_selem_id_alloca(&sid);

	//sets simple-mixer index and name
	snd_mixer_selem_id_set_index(sid, mix_index);
	snd_mixer_selem_id_set_name(sid, mix_name);

	if ((snd_mixer_open(&handle, 0)) < 0)
		return -1;
	if ((snd_mixer_attach(handle, card)) < 0) {
		snd_mixer_close(handle);
		return -2;
	}
	if ((snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
		snd_mixer_close(handle);
		return -3;
	}
	ret = snd_mixer_load(handle);
	if (ret < 0) {
		snd_mixer_close(handle);
		return -4;
	}
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		snd_mixer_close(handle);
		return -5;
	}

	long minv;
	long  maxv;
	snd_mixer_selem_get_playback_volume_range (elem, &minv, &maxv);

	if(snd_mixer_selem_get_playback_volume(elem, 0, left_volume) < 0) {
		snd_mixer_close(handle);
		return -3;
	}

	/* make the value bound to 100 */
	*left_volume -= minv;
	maxv -= minv;
	minv = 0;
	*left_volume = 100 * (*left_volume) / maxv; // make the value bound from 0 to 100

	snd_mixer_selem_get_playback_volume_range (elem, &minv, &maxv);

	if(snd_mixer_selem_get_playback_volume(elem, 1, right_volume) < 0) {
		snd_mixer_close(handle);
		return -4;
	}

	/* make the value bound to 100 */
	*right_volume -= minv;
	maxv -= minv;
	minv = 0;
	*right_volume = 100 * (*right_volume) / maxv; // make the value bound from 0 to 100

	snd_mixer_close(handle);
	return 0;

}

char *getdatetime() {
	char *buf;
	time_t result;
	struct tm *resulttm;

	if((buf = malloc(sizeof(char)*65)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for buf.\n");
		exit(1);
	}
	result = time(NULL);
	resulttm = localtime(&result);
	if(resulttm == NULL) {
		fprintf(stderr, "Error getting localtime.\n");
		exit(1);
	}
	if(!strftime(buf, sizeof(char)*65-1, "%a %b %d %H:%M:%S", resulttm)) {
		fprintf(stderr, "strftime is 0.\n");
		exit(1);
	}
	
	return buf;
}

int getbattery(int* en_now, int* en_full) {
	FILE *fd;
	int energy_now, energy_full, voltage_now;

	fd = fopen("/sys/class/power_supply/BAT0/charge_now", "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening energy_now.\n");
		return -1;
	}
	fscanf(fd, "%d", &energy_now);
	fclose(fd);


	fd = fopen("/sys/class/power_supply/BAT0/charge_full", "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening energy_full.\n");
		return -1;
	}
	fscanf(fd, "%d", &energy_full);
	fclose(fd);


	fd = fopen("/sys/class/power_supply/BAT0/voltage_now", "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening voltage_now.\n");
		return -1;
	}
	fscanf(fd, "%d", &voltage_now);
	fclose(fd);

	*en_now = energy_now;
	*en_full = energy_full;

	return (float)energy_now / (float)energy_full * 100;
}

long time_usec(struct timeval *curr_time){
	gettimeofday(curr_time, NULL);

	return (long)1e6*(curr_time->tv_sec)+(curr_time->tv_usec);
}

int getbrightness(float *brightness) {
	const char * backlight_dir = "/sys/class/backlight/intel_backlight";
	const char * cur_brightness_file = "brightness";
	const char * max_brightness_file = "max_brightness";

	int cur_brightness = 0;
	int max_brightness = 0;

	FILE *file_brightness = NULL;
	char filepath[100];

	sprintf(filepath, "%s/%s", backlight_dir, cur_brightness_file);
	file_brightness = fopen(filepath, "r");
	if (file_brightness == NULL){
		fprintf(stderr, "Error opening current brightness file %s\n", filepath);
		return -1;
	}
	fscanf(file_brightness, "%d", &cur_brightness);
	fclose(file_brightness);

	sprintf(filepath, "%s/%s", backlight_dir, max_brightness_file);
	file_brightness = fopen(filepath, "r");
	if (file_brightness == NULL){
		fprintf(stderr, "Error opening maximum brightness file %s\n", filepath);
		return -1;
	}
	fscanf(file_brightness, "%d", &max_brightness);
	fclose(file_brightness);

	*brightness = ((float)cur_brightness/(float)max_brightness)*100.0;

	return 0;
}

int main(void) {
	char *status;
	if((status = malloc(STATUS_LENGTH*sizeof(char))) == NULL){
		printf("Cannot allocate memory for status");
		return 0;
	}

	char *datetime = NULL;
	char state_bat[12];
	int bat1, energy_full; // bat2;
	float brightness = 0;

	int rem_hours = 0, rem_min = 0, rem_sec = 0;
	long rem_usec = 0;

	char *net_intf, *ip = NULL;

	if((net_intf = malloc(5*sizeof(char))) == NULL){
		printf("Cannot allocate memory for net_intf");
		return 0;
	}

	if((ip = malloc(100*sizeof(char))) == NULL){
		printf("Cannot allocate memory for ip");
		return 0;
	}

	char net_displ[50];

	long left_volume;
	long right_volume;

	int mem_used;
	int mem_available;
	float cpu_avg_freq;

	long usec1, usec2 = 0;
	struct timeval time1, time2;

	int connected = 0;

	int energy1, energy2, dummy = 0;

	int count = 0;

	char *mpc_vol, *mpc_state, *mpc_song = NULL;

	if((mpc_vol = malloc(3*sizeof(char))) == NULL){
		printf("Cannot allocate memory for mpc_vol");
		return 0;
	}

	if((mpc_state = malloc(5*sizeof(char))) == NULL){
		printf("Cannot allocate memory for mpc_state");
		return 0;
	}

	if((mpc_song = malloc(100*sizeof(char))) == NULL){
		printf("Cannot allocate memory for mpc_song");
		return 0;
	}

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Cannot open display.\n");
		return 0;
	}

	strcpy(state_bat, "unknown");

	for (;;sleep(1)) {

		bat1 = getbattery(&dummy, &energy_full);
		getbrightness(&brightness);

		connected = net(&net_intf, &ip);

		datetime = getdatetime();
		getvolume(&left_volume, &right_volume);

		getmemoryusage(&mem_available, &mem_used);
		getcpuusage(&cpu_avg_freq);

		if (connected == 1) sprintf(net_displ, "%s: %s", net_intf, ip);
		else sprintf(net_displ, "not connected");

		//mpc_stat(&mpc_vol, &mpc_state, &mpc_song);

		if (count == 0) {
			usec1 = time_usec(&time1);
			getbattery(&energy1, &energy_full);
			count++;
		} else if (count >= 5) {

			// Estimation of the charging or dischargin time as well as the time left for the battery to be charged or discharged
			count = 0;

			usec2 = time_usec(&time2);
			getbattery(&energy2, &energy_full);

			if ((energy2 - energy1) < 0){
				rem_usec = ((float)(usec2-usec1))*(((float)(energy1+energy2))/((float)(2*(energy1-energy2))));
				strcpy(state_bat, "discharging");
			} else if ((energy2 - energy1) > 0) {
				rem_usec = (((float)(usec2-usec1))*((float)energy_full)/((float)(energy2-energy1)))*(1-(((float)(energy2+energy1))/((float)(2*energy_full))));
				strcpy(state_bat, "charging");
			} else if ( (energy2 == energy1) && bat1 >= 100) {
				rem_usec = 0;
				strcpy(state_bat, "charged");
			} else {
				rem_usec = 0;
				strcpy(state_bat, "unknown");
			}

			rem_hours = (int)(rem_usec/(1e6*60*60));
			rem_min = (int)((rem_usec/(1e6*60)) - rem_hours*60);
			rem_sec = (int)((rem_usec/(1e6)) - rem_hours*60*60 - rem_min*60);


			if ((bat1 < 15) && (strcmp(state_bat, "discharging") == 0)) {
				system("mplayer /home/andrea/.local/statusbar/sound/siren.mp3");
			}

		} else {
			count++;
		}

//		snprintf(status, STATUS_LENGTH,  "MPC: status: %s - song: %s | %s | volume left %i - right %i | Battery %d%% (%s %d h %d min %d s) | %s", mpc_state, mpc_song, net_displ, (int)left_volume, (int)right_volume, bat1, state_bat, rem_hours, rem_min, rem_sec, datetime);
		snprintf(status, STATUS_LENGTH,  " brightness %2.2f%% | CPU (avg) %4.3f MHz | RAM used %d%% available %d%% | %s | volume left %i - right %i | Battery %d%% (%s %d h %d min %d s) | %s", brightness, cpu_avg_freq, mem_used, mem_available, net_displ, (int)left_volume, (int)right_volume, bat1, state_bat, rem_hours, rem_min, rem_sec, datetime);
		setstatus(status);

	}

	free(mpc_song);
	free(mpc_state);
	free(mpc_vol);
	free(net_intf);
	free(ip);
	free(datetime);
	free(status);
	XCloseDisplay(dpy);

	return 0;
}


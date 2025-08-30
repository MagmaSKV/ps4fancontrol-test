#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include "forms.h"

#define SCALE 1

FL_OBJECT *cnt, *bReset, *bSave, *bExit;
uint8_t curTemp = 0x4f;
uint8_t prevTemp = 0;

const char *configFile = "/var/lib/ps4fancontrol/threshold_temp";

int debug = -1;

#define ICC_MAJOR	'I'

struct icc_cmd {
	
	uint8_t major;
	uint16_t minor;
	uint8_t *data;
	uint16_t data_length;
	uint8_t *reply;
	uint16_t reply_length;
};

#define ICC_IOCTL_CMD _IOWR(ICC_MAJOR, 1, struct icc_cmd)

#define CONFIG_DIR "/var/lib/ps4fancontrol"
#define CONFIG_FILE GLOBAL_CONFIG_FILE

int file_exist(const char *filename)
{	
	FILE *f = fopen(filename, "rb");
	if(f == NULL)
		return -1;
	
	fclose(f);
	return 0;
}

int initSettings()
{
	if (geteuid() == 0) { // si se ejecuta como root
	    make_config_world_writable(configFile);
	}

	DIR *dir = opendir(CONFIG_DIR);
	if(dir == NULL)
	{
		if(errno == ENOENT)
		{
			printf("Directory %s not found, creating it...\n", CONFIG_DIR);
			if(mkdir(CONFIG_DIR, 0755))
			{
				perror("Error creating config directory");
				printf("If you get a permission denied error, open as root/using sudo once and try again without root/sudo\n");
				return -1;
			}
		}
		else
		{
			perror("Error opening config directory");
			printf("If you get a permission denied error, open as root/using sudo once and try again without root/sudo\n");
			return -1;
		}
	}
	else
	{
		closedir(dir);
	}

	return 0;
}

int saveConfig(uint8_t temperature)
{
	FILE *f = fopen(configFile, "wb");
	if(f == NULL)
	{
		perror("Error opening config file for writing");
		printf("If you get a permission denied error, open as root/using sudo once and try again without root/sudo\n");
		return -1;
	}
	if(fwrite(&temperature, 1, 1, f) != 1)
	{
		printf("Error writing configuration file\n");
		fclose(f);
		unlink(configFile);
		return -1;
	}
	
	printf("Selected threshold temperature saved in %s\n", configFile);
	fclose(f);
	return 0;
}

int loadConfig()
{
	FILE *f = fopen(configFile, "rb");
	if(f == NULL)
	{
		printf("Configuration file not found\n");
		return -1;
	}

	uint8_t ret;
	uint8_t temp_bak = curTemp;

	if(fread(&ret, 1, 1, f) != 1)
	{
		printf("Error reading configuration file\n");
		curTemp = temp_bak;
		fclose(f);
		return -1;
	}

	if(ret >= 45 && ret <= 85)
		curTemp = ret;
	else
		printf("Configuration file contains an invalid value\n");

	printf("Threshold temperature loaded from configuration file\n");
	fclose(f);

	return 0;
}

void showError(const char *title, const char *str)
{
	FL_FORM *f;
	FL_OBJECT *obj;
	
	f = fl_bgn_form(FL_UP_BOX, 300, 130);
	fl_add_box(FL_NO_BOX, 0, 0, 300, 90, str);
	obj = fl_add_button(FL_NORMAL_BUTTON, 115, 90, 70, 25, "OK");
	
	fl_end_form();
	
	fl_show_form(f, FL_PLACE_MOUSE, FL_FULLBORDER, title);
	
	while(fl_do_forms() != obj)
	
	fl_finish();
	exit(-1);
}

int make_config_world_writable(const char *file)
{
    // 0666 = lectura/escritura para todos
    if (chmod(file, 0666) != 0) {
        perror("Failed to set permissions on config file");
        return -1;
    }
    return 0;
}
	
int set_temp_threshold(uint8_t temperature)
{
	int fd = -1;
	int ret = 0;
	struct icc_cmd arg;
	uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x4f};

	fd = open("/dev/icc", 0);
	if(fd == -1)
	{
		perror("Error");
		return -1;
	}
	
	arg.major = 0xA;
	arg.minor = 0x6;
	arg.data = malloc(sizeof(data));
	arg.data_length = 0x34;
	arg.reply = malloc(0x20);
	arg.reply_length = 0x20;
	
	//one more useless check
	if(temperature >= 45 && temperature <= 85)
		data[5] = temperature;
	else
	{
		printf("Current threshold temperature (%u°C) is out of range, back to default (79°C)\n", temperature);
		temperature = 0x4f;
	}
	
	memcpy(arg.data, data, sizeof(data));
	
	ret = ioctl(fd, ICC_IOCTL_CMD, &arg);
	usleep(1000);
	
	printf("ioctl ret: %d", ret);
	
	switch(ret)
	{
		case -EFAULT:
			printf(" Error: Bad address");
			return ret;
		case -ENOENT:
			printf(" Error: Invalid command");
			return ret;
		case -1:
			perror(" Error");
			return ret;
		default:
			break;
	}
	
	if(!debug)
	{
		printf("\nReply: ");
		for(int i = 0; i < ret; i++)
		{
			printf("0x%02x ", *(arg.reply + i));
		}
	}
	
	switch(*arg.reply)
	{
		case 0x00:
			printf("\nSuccess!\n");
			break;
		case 0x02:
			printf("\nError: Invalid data\n");
			return *arg.reply;
		case 0x04:
			printf("\nError: Invalid data length\n");
			return *arg.reply;
		default:
			printf("\nUnknown status code: 0x%02x\n", *arg.reply);
			return *arg.reply;
	}
	return 0;
}

int get_temp_threshold()
{
	int fd = -1;
	int ret = 0;
	struct icc_cmd arg;
	
	curTemp = 0x4f;

	fd = open("/dev/icc", 0);
	if(fd == -1)
	{
		perror("Error");
		return -1;
	}
	
	arg.major = 0xA;
	arg.minor = 0x7;
	arg.data = 0;
	arg.data_length = 0;
	arg.reply = malloc(0x52);
	arg.reply_length = 0x52;
	
	ret = ioctl(fd, ICC_IOCTL_CMD, &arg);
	usleep(1000);
	
	printf("ioctl ret: %d", ret);
	
	switch(ret)
	{
		case -EFAULT:
			printf(" Error: Bad address");
			return ret;
		case -ENOENT:
			printf(" Error: Invalid command");
			return ret;
		case -1:
			perror(" Error");
			return ret;
		default:
			break;
	}
	
	if(!debug)
	{
		printf("\nReply: ");
		for(int i = 0; i < ret; i++)
		{
			printf("0x%02x ", *(arg.reply + i));
		}
	}
	
	switch(*arg.reply)
	{
		case 0x00:
			printf("\nSuccess!\n");
			break;
		case 0x02:
			printf("\nError: Invalid data\n");
			return *arg.reply;
		case 0x04:
			printf("\nError: Invalid data length\n");
			return *arg.reply;
		default:
			printf("\nUnknown status code: 0x%02x\n", *arg.reply);
			return *arg.reply;
	}
	
	if(*(arg.reply + 5) >= 45 && *(arg.reply + 5) <= 85)
		curTemp = *(arg.reply + 5);
	else
	{
		printf("Current threshold temperature (%u°C) is out of range, back to default (79°C)\n", *(arg.reply + 5));
		set_temp_threshold(curTemp);
	}
		
	return 0;
}

void counter_callback(FL_OBJECT *obj, long id)
{
	int ret;
	
	if(prevTemp == 0 && file_exist(configFile) != -1)
		prevTemp = curTemp;
		
	curTemp = 0x4f;
	
	curTemp = (uint8_t)fl_get_counter_value(obj);
	printf("Threshold temperature set to %d \n", curTemp);
	
	ret = set_temp_threshold(curTemp);
	if(ret)
		exit(ret);
	
	if(prevTemp != curTemp)
		fl_set_button(bSave, 0);
	else
		fl_set_button(bSave, 1);
		
}

void save_callback(FL_OBJECT *obj, long id)
{
	if(prevTemp == curTemp)
	{
		fl_set_button(obj, 1);
	}
	else if(prevTemp != 0)
	{
		saveConfig(curTemp);
		prevTemp = curTemp;
	}
	
	if(file_exist(configFile))
	{
		saveConfig(curTemp);
		fl_set_button(obj, 1);
	}
	
	if(prevTemp == 0)
	{
		prevTemp = curTemp;
		
		if(file_exist(configFile) != -1)
			fl_set_button(obj, 1);
	}
}

void reset_callback(FL_OBJECT *obj, long id)
{
	prevTemp = 0;
	curTemp = 0x4f;
	fl_set_counter_value(cnt, curTemp);
	fl_set_button(bSave, 0);
	
	set_temp_threshold(curTemp);
	
	printf("Threshold temperature back to default\n");
	
	unlink(configFile);
}

/*const char *counterString(FL_OBJECT *obj, double value, int prec) 
{
    static char buf[32];

     sprintf(buf, "°C %u", (uint8_t)value);
     return buf;
}*/


int main(int argc, char *argv[])
{
	int no_gui = -1;
	int ret = -1;
	int gid;
	int uid;
	dev_t dev;
	
	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--debug") == 0)
			debug = 0;
		else if(strcmp(argv[i], "--no-gui") == 0)
			no_gui = 0;
	}
	
	if(no_gui)
		fl_initialize(&argc, argv, 0, 0, 0);
		
	dev = makedev(0x49, 1);
	mknod("/dev/icc", S_IFCHR | 0666, dev);
	
	int fd = open("/dev/icc", 0);
	if(fd == -1)
	{
		if(no_gui)
			showError("Error", "You need run the program as root/using sudo once!");
		else
			printf("Error: you need run the program as root/using sudo once!");
			
		return -1;
	}
	close(fd);
	
	uid = getuid();
	gid = getgid();
	printf("Running as uid=%d, gid=%d\n", uid, gid);
	
	get_temp_threshold(&curTemp);
	
	initSettings();
	ret = loadConfig();
	
	if(no_gui == 0)
	{
		if(set_temp_threshold(curTemp))
			return -1;
		else
			return 0;
	}
	
	//GUI
	FL_FORM *form;
	
	form = fl_bgn_form(FL_UP_BOX, 350*SCALE, 150*SCALE);
    
	cnt = fl_add_counter(FL_NORMAL_COUNTER, 25*SCALE, 50*SCALE, 300*SCALE, 25*SCALE, "Celsius Temperature");
	fl_set_counter_bounds(cnt, 45, 85);
	fl_set_counter_step(cnt, 1, 10);
	fl_set_counter_precision(cnt, 0);
	fl_set_object_callback(cnt, counter_callback, ret);
	fl_set_counter_value(cnt, curTemp);
	fl_set_object_return(cnt, FL_RETURN_END_CHANGED);

	bReset = fl_add_button(FL_NORMAL_BUTTON, 17*SCALE, 115*SCALE, 102*SCALE, 25*SCALE, "Reset");
	bSave  = fl_add_button(FL_BUTTON,        124*SCALE, 115*SCALE, 102*SCALE, 25*SCALE, "Save");
	bExit  = fl_add_button(FL_NORMAL_BUTTON, 231*SCALE, 115*SCALE, 102*SCALE, 25*SCALE, "Exit");

	fl_set_object_lsize(cnt, 10*SCALE);
	fl_set_object_lsize(bSave, 10*SCALE);
	fl_set_object_lsize(bReset, 10*SCALE);
	fl_set_object_lsize(bExit, 10*SCALE);
	
	fl_set_object_callback(bSave, save_callback, ret);
	if(ret != -1)
		fl_set_button(bSave, 1);
		
	fl_set_object_callback(bReset, reset_callback, 0);

    fl_end_form();
 
    fl_show_form(form, FL_PLACE_MOUSE, FL_FULLBORDER, "PS4 Fan Control");

    while(fl_do_forms() != bExit){}
	
	fl_finish();
	return 0;
}

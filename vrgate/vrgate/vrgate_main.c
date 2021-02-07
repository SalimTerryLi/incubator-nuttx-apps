/****************************************************************************
 * examples/hello/hello_main.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <nuttx/progmem.h>
#include <fcntl.h>
#include <nuttx/timers/pwm.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <nuttx/input/buttons.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "i2c_lib.h"
#include "TC358840XBG.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * vrgate_main
 ****************************************************************************/

#define VRGATE_PROGMEM_WHOAMI_H 0x13
#define VRGATE_PROGMEM_WHOAMI_L 0x26

struct vrgate_cfg {
	uint8_t who_am_i[2];
	uint8_t back_light;
} cfg;

int pwm_fd;
int i2c_fd;

static bool is_running=false;
static bool is_valid_output=false;

int main(int argc, FAR char *argv[]);
void print_help(void);
int client_main(int argc, FAR char *argv[]);
int check_progmem_cfg(void);
int begin_backlight(void);
int set_backlight(int level);
int end_backlight(void);

static int pipe_daemon(int argc, char **argv);

static int tc358840xbg_daemon(int argc, char *argv[]);

int main(int argc, FAR char *argv[])
{
	if (is_running){
		return client_main(argc, argv);
	}
	is_running=true;

	int ret=0;
	syslog(LOG_INFO, "VRGATE!\n");

	// init progmem and load cfgs
    if(check_progmem_cfg()<0){
    	syslog(LOG_ERR, "ERROR: check_progmem_cfg() failed\n");
	  	return -1;
    }

    // init pwm backlight
    if(begin_backlight()<0){
	    syslog(LOG_ERR, "ERROR: begin_backlight() failed\n");
	    return -1;
    }
    if(set_backlight(0)<0){
	    syslog(LOG_ERR, "ERROR: set_backlight() failed\n");
	    return -1;
    }

    i2c_fd=open("/dev/i2c2",O_RDONLY);
    if (i2c_fd<0){
	    syslog(LOG_ERR, "ERROR: i2c open() failed: %d errno=%d\n",i2c_fd,errno);
	    ret=i2c_fd;
	    goto do_backlignt_exit;
    }

    // get ChipID
	uint8_t i2cbuf[4]={0x0,0x0,0x0,0x0};
	i2c_xfr(i2c_fd,0x0f,i2cbuf,2,i2cbuf,2);
	syslog(LOG_INFO, "TC358840XBG ChipID=0x%.2x, RevisionID=0x%.2x\n",i2cbuf[1],i2cbuf[0]);

	init_tc358840xbg(i2c_fd);

	ret = task_create("TC358840XBG_INTR",100,2048,tc358840xbg_daemon,NULL);
	if (ret < 0)
	{
		int errcode = errno;
		syslog(LOG_ERR, "ERROR: Failed to start tc358840xbg_daemon: %d\n",
		       errcode);
		goto do_i2c_exit;
	}

	ret = task_create("fifo_server", 100, 2048, pipe_daemon, NULL);
	if (ret < 0)
	{
		int errcode = errno;
		syslog(LOG_ERR, "ERROR: Failed to start pipe_daemon: %d\n",
		       errcode);
		goto do_i2c_exit;
	}

	goto do_exit;

	do_i2c_exit:
    close(i2c_fd);
	do_backlignt_exit:
    end_backlight();

	do_exit:
    return ret;
}

void print_help(){
	printf("vrgate [option] [cmd]\n");
	printf("  options: -l 50 : Set backlight to 50 (0~100)\n");
	printf("  cmds: status   : Show SYS_STATUS\n");
}

int client_main(int argc, FAR char *argv[]){
	int ret=0;
	if (argc == 1){
		print_help();
		return 0;
	}
	char buffer[32];
	int sico_fd = open("/vrgate_work_dir/sico",O_RDWR);
	if(sico_fd<0){
		printf("ERROR: open(\"/vrgate_work_dir/sico\"): %d\n",errno);
		return sico_fd;
	}
	int soci_fd = open("/vrgate_work_dir/soci",O_RDWR);
	if(soci_fd<0){
		printf( "ERROR: open(\"/vrgate_work_dir/soci\"): %d\n",errno);
		close(sico_fd);
		return soci_fd;
	}
	int option;
	while((option = getopt(argc, argv, "hl:")) != -1){ //get option from the getopt() method
		switch(option){
			case 'l':
				sprintf(buffer,"BACKLIGHT=%s",optarg);
				printf("%s\n",buffer);
				write(sico_fd,buffer,strlen(buffer)+1);
				printf("Command sent.\n");
				read(soci_fd,buffer,sizeof(buffer));
				printf("%s",buffer);
				break;
			case 'h':
				print_help();
				break;
			case '?': //used for some unknown options
				printf("unknown option: %c\n", optopt);
				ret=-1;
				goto exit_eargs;
			default:
				break;
		}
	}
	for(; optind < argc; optind++){ //when some extra arguments are passed
		if (strcmp(argv[optind],"status")==0){
			printf("Requesting status...\n");
			sprintf(buffer,"SYS_STATUS=?");
			write(sico_fd,buffer,strlen(buffer)+1);
			read(soci_fd,buffer,sizeof(buffer));
			uint8_t sys_status = atoi(buffer);
			printf("TC358840XBG SYS_STATUS:\n");
			printf("DDC5V\tTMDS\tPHY_PLL\tSCDT\tMode\tHDCP\tAVMUTE\tSYNC\n");
			if (sys_status & 0x01){printf("Valid\t");}else{printf("None\t");}
			if (sys_status & 0x02){printf("Valid\t");}else{printf("None\t");}
			if (sys_status & 0x04){printf("Lock\t");}else{printf("Unlock\t");}
			if (sys_status & 0x08){printf("Valid\t");}else{printf("None\t");}
			if (sys_status & 0x10){printf("HDMI\t");}else{printf("DVI\t");}
			if (sys_status & 0x20){printf("ON\t");}else{printf("OFF\t");}
			if (sys_status & 0x40){printf("ON\t");}else{printf("OFF\t");}
			if (sys_status & 0x80){printf("Valid\n");}else{printf("None\n");}
		}
	}
	// print SYS_STATUS reg
	exit_eargs:
	close(sico_fd);
	close(soci_fd);
	return ret;
}

int check_progmem_cfg(){
	int ret = 0;
	FAR const uint8_t *src=(FAR const uint8_t *)up_progmem_getaddress(8);
	memcpy(&cfg,src,sizeof(cfg));
	if (cfg.who_am_i[0]!=VRGATE_PROGMEM_WHOAMI_L && cfg.who_am_i[1]!=VRGATE_PROGMEM_WHOAMI_H){   // init progmem
		syslog(LOG_INFO,"Info: init progmem\n");
		cfg.who_am_i[0]=VRGATE_PROGMEM_WHOAMI_L;
		cfg.who_am_i[1]=VRGATE_PROGMEM_WHOAMI_H;
		cfg.back_light=50;
		ret = up_progmem_eraseblock(8);
		if (ret<0){
			syslog(LOG_ERR,"Error: up_progmem_eraseblock(8) failed: %d\n",ret);
		}
		ret = up_progmem_write(up_progmem_getaddress(8),(FAR const uint8_t *)&cfg, sizeof(cfg));
		if (ret<0){
			syslog(LOG_ERR,"Error: up_progmem_write()  failed: %d\n",ret);
		}
	}
	return ret;
}

int begin_backlight(){
	pwm_fd = open("/dev/pwm0",O_RDONLY);
	if (pwm_fd<0){
		syslog(LOG_ERR,"Error: open(\"/dev/pwm0\",O_RDONLY) failed: %d errno=%d\n",pwm_fd,errno);
		return pwm_fd;
	}
	return ioctl(pwm_fd, PWMIOC_START, 0);
}

int end_backlight(){
	ioctl(pwm_fd, PWMIOC_STOP, 0);
	close(pwm_fd);
	return 0;
}

int set_backlight(int level){
	if(level > 100){
		level=100;
	}
	if(level < 0){
		level=0;
	}
	struct pwm_info_s info;
	info.frequency=600;
	info.duty=((uint32_t)level *65534) / 100+1;

	int ret = ioctl(pwm_fd, PWMIOC_SETCHARACTERISTICS,
	            (unsigned long)((uintptr_t)&info));
	if (ret<0){
		syslog(LOG_ERR,"Error: ioctl(pwm_fd) failed: %d errno=%d\n",ret,errno);
	}
	return ret;
}

static int tc358840xbg_daemon(int argc, char *argv[]){
	syslog(LOG_INFO,"Info: tc358840xbg_daemon() started\n");
	int ret=0;

	int exti_fd;
	exti_fd = open("/dev/exti", O_RDONLY | O_NONBLOCK);
	if (exti_fd<0){
		syslog(LOG_ERR,"Error: open(exti_fd) failed: %d errno=%d\n",exti_fd,errno);
		return exti_fd;
	}
	btn_buttonset_t supported;
	ret = ioctl(exti_fd, BTNIOC_SUPPORTED,
	            (unsigned long)((uintptr_t)&supported));
	if (ret < 0)
	{
		int errcode = errno;
		syslog(LOG_ERR,"ERROR: ioctl(BTNIOC_SUPPORTED) failed: %d\n",
		       errcode);
		goto do_exti_fd_exit;
	}
	printf("Supported EXTI 0x%02x\n",
	       (unsigned int)supported);

	uint8_t last_sys_status=0x00;
	while(true){
		struct pollfd fds[1];
		fds[0].fd      = exti_fd;
		fds[0].events  = POLLIN;

		ret = poll(fds, 1,200);

		if (ret<0){ // err
			syslog(LOG_ERR,"ERROR poll failed: %d\n", errno);
		}/*else if(ret==0){   // timeout
			syslog(LOG_ERR,"ERROR unexpected poll timeout\n");
		}*/else if(ret==1 || ret == 0){  // normal
			/*if (fds[0].revents & POLLIN){*/
				btn_buttonset_t sampled;
				int nbytes = read(fds[0].fd, (void *)&sampled,
				                  sizeof(btn_buttonset_t));
				if (nbytes<0){
					syslog(LOG_ERR,"ERROR read failed: %d\n", errno);
				}else{
					uint8_t sys_status;
					check_again:
					reset_intr_tc358840xbg(i2c_fd);
					uint8_t i2cbuf[4]={};
					sys_status=get_hdmi_status(i2c_fd);
					if(sys_status != last_sys_status){
						last_sys_status=sys_status;
						continue;
					}
					last_sys_status=sys_status;
					/*usleep(20000);
					if(sys_status !=get_hdmi_status(i2c_fd)){
						goto check_again;
					}*/
					if ((sys_status & 0x80)){
						if (!is_valid_output){
							init_tc358840xbg(i2c_fd);
							is_valid_output=true;
							set_backlight(cfg.back_light);
						}
						//restart_output_tc358840xbg(i2c_fd);
					}else{
						if(is_valid_output){
							set_backlight(0);
							is_valid_output=false;
							i2cbuf[0]=0x00;
							i2cbuf[1]=0x06;
							i2cbuf[2]=0x08;
							i2cbuf[3]=0x00;
							i2c_xfr(i2c_fd,0x0f,i2cbuf,4,NULL,0);
						}
					}
				}
			/*}else{
				syslog(LOG_ERR,"Error: poll returned %d, revent=%d\n",ret,fds[0].revents);
			}*/
		}else{  // unknown error
			syslog(LOG_ERR,"ERROR poll returned: %d\n", ret);
		}
	}

	do_exti_fd_exit:
	close(exti_fd);
	syslog(LOG_INFO,"Info: tc358840xbg_daemon() exited\n");
	return 0;
}

static int pipe_daemon(int argc, char **argv){
	int ret=0;
	int sico_fd = open("/vrgate_work_dir/sico",O_RDWR);
	if(sico_fd<0){
		syslog(LOG_ERR, "ERROR: open(\"/vrgate_work_dir/sico\"): %d\n",errno);
		return -1;
	}
	int soci_fd = open("/vrgate_work_dir/soci",O_RDWR);
	if(soci_fd<0){
		syslog(LOG_ERR, "ERROR: open(\"/vrgate_work_dir/soci\"): %d\n",errno);
		close(sico_fd);
		return -1;
	}
	while(true){
		char buffer[32];
		ret = read(sico_fd,buffer,sizeof(buffer));
		buffer[31]='\0';
		syslog(LOG_INFO,"Info: client side request len=%d\n",ret);
		if(strstr(buffer,"BACKLIGHT=")==buffer){
			int val=atoi(buffer+10);
			syslog(LOG_INFO,"Info: backlight=%d\n",val);
			if(is_valid_output){
				set_backlight(val);
			}
			cfg.back_light=val;
			up_progmem_eraseblock(8);
			up_progmem_write(up_progmem_getaddress(8),(FAR const uint8_t *)&cfg, sizeof(cfg));
			sprintf(buffer,"BACKLIGHT=%d OK\n",val);
			write(soci_fd,buffer,strlen(buffer)+1);
			syslog(LOG_INFO,"Info: progmem updated\n");
		}
		if(strstr(buffer,"SYS_STATUS=?")==buffer){
			sprintf(buffer,"%d\n",(char)get_stored_sysstatus());
			write(soci_fd,buffer,strlen(buffer)+1);
		}
	}
	close(sico_fd);
	close(soci_fd);
	return 0;
}
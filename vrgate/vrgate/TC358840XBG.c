#include "TC358840XBG.h"
#include <fcntl.h>
#include <stdio.h>
#include "i2cdump_gen.h"

static uint8_t sys_status=0x00;
uint8_t get_stored_sysstatus(){return sys_status;}

void replay_i2c(int i2c_fd, const i2c_op *ops, const uint16_t size){

	for(uint16_t i=0;i<size;++i){
		//usleep(ops[i].timestamp);
		uint8_t i2cbuf[6];
		if(ops[i].operate==0){	// Write
			i2cbuf[0]=ops[i].address / 256;
			i2cbuf[1]=ops[i].address % 256;
			if(ops[i].datasize==2){
				i2cbuf[2]=ops[i].data / 256;
				i2cbuf[3]=ops[i].data % 256;
			}else if(ops[i].datasize==1){
				i2cbuf[2]=ops[i].data % 256;
			}else if(ops[i].datasize==4){
				i2cbuf[2]= ops[i].data/(256*256*256);
				i2cbuf[3]= (ops[i].data % (256*256*256))/(256*256);
				i2cbuf[4]= (ops[i].data % (256*256))/(256);
				i2cbuf[5]= (ops[i].data % (256));
			}
			i2c_xfr(i2c_fd,0x0f,i2cbuf,ops[i].datasize+2,NULL,0);
		}else{	// Read
			i2cbuf[0]=ops[i].address / 256;
			i2cbuf[1]=ops[i].address % 256;
			i2c_xfr(i2c_fd,0x0f,i2cbuf,2,i2cbuf,ops[i].datasize);
		}
	}
}

void write_edid(int i2c_fd){
	int edid_fd=open("/rom/edid.bin",O_BINARY | O_RDONLY);
	uint8_t i2cbuf[3]={0x8c,0x00,0x00};
	for(uint16_t i=0;i<256;++i){
		i2cbuf[1]=i;
		read(edid_fd,i2cbuf+2,1);
		i2c_xfr(i2c_fd,0x0f,i2cbuf,3,NULL,0);
	}
	close(edid_fd);
}

void init_tc358840xbg(int i2c_fd){
	replay_i2c(i2c_fd,i2cops_init_before_edid,sizeof(i2cops_init_before_edid)/sizeof(i2c_op));
	write_edid(i2c_fd);
	replay_i2c(i2c_fd,i2cops_init_after_edid,sizeof(i2cops_init_after_edid)/sizeof(i2c_op));
}

void reset_intr_tc358840xbg(int i2c_fd){
	uint8_t i2cbuf[4]={0x00,0x00,0x00,0x00};
	i2cbuf[0]=0x85;
	i2cbuf[1]=0x0b;
	i2cbuf[2]=0xff;
	i2c_xfr(i2c_fd,0x0f,i2cbuf,3,NULL,0);
	i2cbuf[0]=0x00;
	i2cbuf[1]=0x14;
	i2cbuf[2]=0xbf;
	i2cbuf[3]=0x0f;
	i2c_xfr(i2c_fd,0x0f,i2cbuf,4,NULL,0);
}

void restart_output_tc358840xbg(int i2c_fd){

}

uint8_t get_hdmi_status(int i2c_fd){
	uint8_t i2cbuf[3]={0x85,0x20,0x00};
	i2c_xfr(i2c_fd,0x0f,i2cbuf,2,i2cbuf+2,1);
	sys_status=i2cbuf[2];
	//syslog(LOG_INFO,"INFO: SYS_STATUS %.2x\n",sys_status);
	return i2cbuf[2];
}
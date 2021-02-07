#include <syslog.h>
#include "i2c_lib.h"

/* DO NOT MODIFY */
int i2c_xfr(int fd, char addr, uint8_t* w_buf, int w_len, uint8_t* r_buf, int r_len){
	if (w_len<=0 && r_len<=0){
		return -1;
	}
	struct i2c_transfer_s xfer;
	struct i2c_msg_s msg[2];
	msg[0].frequency = I2C_SPEED_FAST;
	msg[0].addr      = addr;
	msg[0].flags     = r_len==0? 0: I2C_M_NOSTOP;
	msg[0].buffer    = w_buf;
	msg[0].length    = w_len;

	msg[1].frequency = I2C_SPEED_FAST;
	msg[1].addr      = addr;
	msg[1].flags     = I2C_M_READ;
	msg[1].buffer    = r_buf;
	msg[1].length    = r_len;

	xfer.msgv=r_len==0?msg:w_len==0?msg+1:msg;
	xfer.msgc=r_len == 0 || w_len == 0? 1:2;

	int ret=ioctl(fd, I2CIOC_TRANSFER, (unsigned long)((uintptr_t)&xfer));
	if (ret<0){
		syslog(LOG_ERR, "ERROR: i2c ioctl() failed\n");
	}

	return ret;
};
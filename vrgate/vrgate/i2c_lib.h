#ifndef __APPS_VRGATE_VRGATE_I2C_LIB_H
#define __APPS_VRGATE_VRGATE_I2C_LIB_H

#include <nuttx/i2c/i2c_master.h>
#include <sys/ioctl.h>

int i2c_xfr(int fd, char addr, uint8_t* w_buf, int w_len, uint8_t* r_buf, int r_len);

#endif // __APPS_VRGATE_VRGATE_I2C_LIB_H
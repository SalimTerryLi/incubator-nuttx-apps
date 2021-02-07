#ifndef __APPS_VRGATE_VRGATE_TC358840XBG_H
#define __APPS_VRGATE_VRGATE_TC358840XBG_H

#include "i2c_lib.h"

uint8_t get_stored_sysstatus(void);

void init_tc358840xbg(int i2c_fd);

uint8_t get_hdmi_status(int i2c_fd);

void reset_intr_tc358840xbg(int i2c_fd);

void restart_output_tc358840xbg(int i2c_fd);

#endif // __APPS_VRGATE_VRGATE_TC358840XBG_H

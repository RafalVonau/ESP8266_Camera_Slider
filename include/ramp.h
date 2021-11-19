/*
 * Ramp
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */
#ifndef __RAMP_H__
#define __RAMP_H__

#define RSTART_STOP_SPEED   (3 * 16 * 200)
#define RMAXIMUM_SPEED      (10 * 16 * 200)
#define RSTART_STOP_PERIOD  (80000000u/(3 * 16 * 200))
#define RSTART_STOP_HPERIOD (40000000u/(3 * 16 * 200))
#define RMAXIMUM_PERIOD     (80000000u/(10 * 16 * 200))

#define USE_RAMP

#ifdef USE_RAMP

extern unsigned char ramp[2916];

#endif


#endif
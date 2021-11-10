/*
 * One axis (1D) motion class for ESP8266.
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */
#ifndef __MOTION_1D__
#define __MOTION_1D__


#include "Arduino.h"
#include <stdint.h>
#include <c_types.h>
#include <eagle_soc.h>
#include <ets_sys.h>
#include "osapi.h"
#include "Command.h"

extern volatile int               x_target;
extern volatile int               x_pos;


#define MOTION_QUEUE_SIZE (64)

#ifdef MOTION_QUEUE_SIZE
#define MOTION_QUEUE_MASK (MOTION_QUEUE_SIZE-1)
typedef struct motion_queue_s {
	int cmd;
	int duration;
	int x;
} motion_queue_t;
#endif

class Motion1D
{
public:
	Motion1D(int step1, int dir1, int en_pin);

	void motorsOff();
	void motorsOn();
	void toggleMotors();
	
	boolean loop();
	boolean isInMotion();
	void goToReal(int duration, int xSteps);


#ifdef MOTION_QUEUE_SIZE
	void goTo(uint16_t duration, int xSteps) {motionQ_push(1, duration, xSteps);}

	void motionQ_push(int cmd, int duration, int x) {
		int pos = m_motionQWr;
		motion_queue_t *v = &m_motionQ[pos];
		v->cmd = cmd;
		v->duration = duration;
		v->x = x;
		pos++;
		pos &= MOTION_QUEUE_MASK;
		m_motionQWr = pos;
	}
	
	uint32_t motionQ_is_full() {
		int pos = m_motionQWr;
		pos++;
		pos &= MOTION_QUEUE_MASK;
		if (pos == m_motionQRd) return 1;
		pos++;
		pos &= MOTION_QUEUE_MASK;
		if (pos == m_motionQRd) return 1;
		return 0;
	}
	
	void motionQ_pull() {
		if (m_motionQWr != m_motionQRd) {
			int pos = m_motionQRd;
			motion_queue_t *v = &m_motionQ[pos];
			switch (v->cmd) {
				case 1: goToReal(v->duration, v->x); break;
				default: break;
			}
			pos++;
			pos &= MOTION_QUEUE_MASK;
			m_motionQRd = pos;
		}
	}
#else
	void goTo(int duration, int xSteps) {goToReal(duration, xSteps);}
#endif
	void stop();
	void printStat(CommandQueueItem *c);
public:
	int           m_x_dir;
	boolean       m_motorsEnabled;
	int           m_en_pin;
#ifdef MOTION_QUEUE_SIZE
	motion_queue_t m_motionQ[MOTION_QUEUE_SIZE];
	int            m_motionQWr;
	int            m_motionQRd;
#endif
};

#endif
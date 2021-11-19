/*
 * One axis (1D) motion class for ESP8266 (simple constant speed no RAMP motion controll).
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */
extern "C" {
	#include <osapi.h>
	#include <os_type.h>
}
#include "Motion1D.h"
#include "esp8266_gpio_direct.h"
#include "core_esp8266_waveform.h"
#include "ramp.h"

#define TIMER1_DIVIDE_BY_1              0x0000
#define TIMER1_DIVIDE_BY_16             0x0004
#define TIMER1_ENABLE_TIMER             0x0080
#define TIMER1_AUTORELOAD               (1u<<6)

#define MIN_PERIOD                      (4000)

static volatile int        int_active       = 0;   /*!< Timer1 interrupt is active (Timer1 is running).   */
static volatile int        in_motion        = 0;   /*!< We are in motion.                                 */

/* X */
static uint16_t            x_gpio_mask      = 0;   /*!< GPIO mask for STEP pin.                           */
static volatile uint32_t   x_hperiod        = 0;   /*!< TIMER1 half period in clock cycles (80MHz clock). */
volatile int               x_target         = 0;   /*!< Target position.                                  */
volatile int               x_pos            = 0;   /*!< Current position.                                 */
static volatile int        x_pulse          = 0;   /*!< STEP pulse phase 0 (level 0), 1 (level 1).        */

#ifdef USE_RAMP
volatile int               x_pos_start      = 0;   /*!< Motion start position.                            */
volatile int               x_pos_middle     = 0;   /*!< Motion middle point.                              */
volatile int               x_ramp_len       = 0;   /*!< Motion ramp length.                               */
volatile int               x_ramp_pos       = 0;   /*!< Current ramp table index.                         */
volatile int               x_ramp_iter      = 0;   /*!< Current iterations.                               */
volatile int               x_ramp_phase     = 0;   /*!< Current ramp phase.                               */
volatile uint32_t          x_target_hperiod = 0;   /*!< Target half period.                               */
volatile int               x_dir            = 0;   /*!< Motion direction.                                 */
#endif



struct timer_regs {
	uint32_t frc1_load;   /* 0x60000600 */
	uint32_t frc1_count;  /* 0x60000604 */
	uint32_t frc1_ctrl;   /* 0x60000608 */
	uint32_t frc1_int;    /* 0x6000060C */
	uint8_t  pad[16];
	uint32_t frc2_load;   /* 0x60000620 */
	uint32_t frc2_count;  /* 0x60000624 */
	uint32_t frc2_ctrl;   /* 0x60000628 */
	uint32_t frc2_int;    /* 0x6000062C */
	uint32_t frc2_alarm;  /* 0x60000630 */
};
static struct timer_regs* timer = (struct timer_regs*)(void*)(0x60000600);

static void motion_intr_handler(void);

//#pragma GCC optimize ("Os")

static inline ICACHE_RAM_ATTR uint32_t GetCycleCount()
{
	uint32_t ccount;
	__asm__ __volatile__("esync; rsr %0,ccount":"=a"(ccount));
	return ccount;
}
//===========================================================================================

/*!
 * \brief Disable Timer1.
 */
static inline ICACHE_RAM_ATTR void motion1D_timer1_disable()
{
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	timer->frc1_ctrl = 0;
	ETS_FRC_TIMER1_INTR_ATTACH(motion_intr_handler, NULL);
	TM1_EDGE_INT_ENABLE();
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	timer->frc1_ctrl = 0;
	int_active       = 0;
}
//===========================================================================================

/*!
 * \brief Enable Timer1.
 */
static inline ICACHE_RAM_ATTR void motion1D_timer1_enable()
{
	int_active = 1;
	ETS_FRC1_INTR_ENABLE();
	RTC_REG_WRITE(FRC1_LOAD_ADDRESS, x_hperiod);
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	timer->frc1_ctrl = TIMER1_DIVIDE_BY_1 | TIMER1_AUTORELOAD | TIMER1_ENABLE_TIMER;
}
//===========================================================================================


void Motion1D::printStat(CommandQueueItem *c)
{
	c->print("now="+String(GetCycleCount()) + "\r\nint_active="+String(int_active)+"\r\nin_motion="+String(in_motion)+"\r\n" + \
		"x_pulse="+String(x_pulse)+ "\r\n" \
		"x_pos="+String(x_pos)+",target = "+String(x_target)+"\r\nOK\r\n");
}
//===========================================================================================



/*!
 * \breif Constructor.
 */
Motion1D::Motion1D(int step1, int dir1, int en_pin)
{
	pinMode(step1, OUTPUT);
	pinMode(dir1, OUTPUT);
	digitalWrite(step1, LOW);
	m_x_dir         = dir1;
	x_gpio_mask     = (1 << step1);
	/* Disable timer */
	motion1D_timer1_disable();
	in_motion       = 0;
	m_en_pin        = en_pin;
	m_motorsEnabled = 0;
	pinMode(en_pin, OUTPUT);
	motorsOff();
#ifdef MOTION_QUEUE_SIZE
	m_motionQWr   = 0;
	m_motionQRd   = 0;
#endif
}
//====================================================================================


void Motion1D::stop()
{
	/* Flush the motion Queue */
#ifdef MOTION_QUEUE_SIZE
	m_motionQWr   = 0;
	m_motionQRd   = 0;
#endif
	/* Stop timer 1 */
	motion1D_timer1_disable();
	x_target = x_pos;
	if (x_pulse) {
		asm volatile ("" : : : "memory");
		gpio_r->out_w1tc = (uint32_t)(x_gpio_mask);
		x_pulse = 0;
	}
}
//====================================================================================

/*!
 * \brief Disable Motors.
 */
void Motion1D::motorsOff()
{
	digitalWrite(m_en_pin, HIGH);
	m_motorsEnabled = 0;
}
//====================================================================================

/*!
 * \brief Enable Motors.
 */
void Motion1D::motorsOn()
{
	digitalWrite(m_en_pin, LOW) ;
	m_motorsEnabled = 1;
}
//====================================================================================

/*!
 * \brief Toggle Motors.
 */
void Motion1D::toggleMotors()
{
	if (m_motorsEnabled) {
		motorsOff();
	} else {
		motorsOn();
	}
}
//====================================================================================

/*!
 * \brief Prepare and start move.
 */
void Motion1D::goToReal(int duration, int xSteps)
{
	uint64_t tmp;
	if (in_motion) { return; }
	if (!m_motorsEnabled) {motorsOn();}
	
	/* Disable timer */
	motion1D_timer1_disable();
	
	in_motion       = 0;
	x_pulse         = 0;
	if (duration == 0) duration = 100;
	/* Set target */
	x_target += xSteps;
	/* Set direction pin */
	if (x_target > x_pos) digitalWrite(m_x_dir, HIGH); else digitalWrite(m_x_dir, LOW);
	/* Prepare RAMP */
#ifdef USE_RAMP
	x_pos_start = x_pos;
	if (x_target > x_pos) {
		x_dir = 1;
		x_pos_middle = x_pos - 2 + ((x_target - x_pos)>>1);
	} else {
		x_dir = 0;
		x_pos_middle = x_pos + 2 - ((x_pos-x_target)>>1);
	}
	x_ramp_pos   = 0;
	x_ramp_iter  = 0;
	x_ramp_phase = 0;
#endif
	/* ABS */
	if (xSteps < 0) xSteps = -xSteps;
	/* Set period (timer1 clock  = 80MHz) */
	if (xSteps) {
		tmp = ((duration * 80000)/xSteps)-1;
	} else {
		tmp = 160000;
	}
	if (tmp < MIN_PERIOD) tmp = MIN_PERIOD;
#ifdef USE_RAMP
	if (tmp < RMAXIMUM_PERIOD) tmp = RMAXIMUM_PERIOD;

	if (tmp < RSTART_STOP_PERIOD) {
		x_target_hperiod = ((tmp >> 1)&0xffffffff);
		x_ramp_phase     = 1;
		tmp = RSTART_STOP_PERIOD;
	}
#endif
	/* Calculate half period */
	x_hperiod = ((tmp >> 1)&0xffffffff);

	/* Start timer1 */
	in_motion  = 1;
	
	/* Start timer */
	motion1D_timer1_enable();
}
//====================================================================================

/*!
 * \brief Executed in main loop.
 */
boolean Motion1D::loop()
{
#ifdef MOTION_QUEUE_SIZE
	if (in_motion) {
		if (int_active == 0) {
			in_motion = 0;
			motionQ_pull();
		}
	} else {
		motionQ_pull();
	}
	return motionQ_is_full();
#else
	if (in_motion) {
		if (int_active == 0) {
			in_motion = 0;
		}
	}
	return in_motion;
#endif
}
//====================================================================================

/*!
 * \brief Check in motion flag.
 */
boolean Motion1D::isInMotion()
{
	if (in_motion) return true;
#ifdef MOTION_QUEUE_SIZE
	if (m_motionQWr  != m_motionQRd) return true;
#endif	
	return false;
}
//====================================================================================

// Speed critical bits
#pragma GCC optimize ("O2")
// Normally would not want two copies like this, but due to different
// optimization levels the inline attribute gets lost if we try the
// other version.

//static inline ICACHE_RAM_ATTR uint32_t GetCycleCountIRQ()
//{
//	uint32_t ccount;
//	__asm__ __volatile__("rsr %0,ccount":"=a"(ccount));
//	return ccount;
//}
//===========================================================================================

/*!
 * \brief timer1 interrupt handler.
 */
void ICACHE_RAM_ATTR motion_intr_handler(void)
{
	/* Clear interrupt mask */
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	if (x_pulse) {
		asm volatile ("" : : : "memory");
		gpio_r->out_w1tc = (uint32_t)(x_gpio_mask);
		x_pulse = 0;
		if (x_pos == x_target) {
			/* We are in target position :-) - disable timer */
			timer->frc1_int &= ~FRC1_INT_CLR_MASK;
			timer->frc1_ctrl = 0;
			int_active = 0;
		}
#ifdef USE_RAMP
		/* Tabled ramp implementation */
		else if (x_ramp_phase) {
			if (x_ramp_phase == 1) {
				/* Ramp UP */
				if (x_hperiod > x_target_hperiod) {
					if (x_ramp_iter == 0) {
						x_hperiod--;
						RTC_REG_WRITE(FRC1_LOAD_ADDRESS, x_hperiod);
						x_ramp_iter = ramp[x_ramp_pos++];
						/* Recalculate middle point */
						if (x_hperiod == x_target_hperiod) {
							if (x_dir == 1) {
								x_ramp_len   = x_pos - x_pos_start;
								x_pos_middle = x_target - x_ramp_len;
							} else {
								x_ramp_len = x_pos_start - x_pos;
								x_pos_middle = x_target + x_ramp_len;
							}
						}
					} else {
						x_ramp_iter--;
					}
				}
				/* Check middle point */
				if (x_dir == 1) {
					if (x_pos > x_pos_middle) {x_ramp_phase = 2;}
				} else {
					if (x_pos < x_pos_middle) {x_ramp_phase = 2;}
				}
			} else {
				/* Ramp DOWN */
				if (x_hperiod < RSTART_STOP_HPERIOD) {
					if (x_ramp_iter == 0) {
						x_hperiod++;
						RTC_REG_WRITE(FRC1_LOAD_ADDRESS, x_hperiod);
						x_ramp_iter = ramp[x_ramp_pos--];
					} else {
						x_ramp_iter--;
					}
				}
			}
		}
#endif
	} else if (x_pos != x_target) {
		asm volatile ("" : : : "memory");
		gpio_r->out_w1ts = (uint32_t)(x_gpio_mask);
		x_pulse = 1;
		if (x_pos > x_target) x_pos--; else x_pos++;
	} else {
		/* We are in target position :-) - disable timer */
		timer->frc1_int &= ~FRC1_INT_CLR_MASK;
		timer->frc1_ctrl = 0;
		int_active = 0;
	}
}
//===========================================================================================

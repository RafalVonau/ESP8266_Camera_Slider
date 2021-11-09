/*
 * One axis (1D) motion class for ESP8266.
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

#define TIMER1_DIVIDE_BY_16             0x0004
#define TIMER1_ENABLE_TIMER             0x0080

#define MIN_PERIOD                      (250)
/* Use period correction - calculate time to enter interrupt handler */
#define CORRECT_PERIOD

static volatile int        int_active   = 0;
static volatile int        in_motion    = 0;

/* X */
static uint16_t            x_gpio_mask  = 0;
static volatile uint32_t   x_period0    = 0;
static volatile uint32_t   x_period1    = 0;
static volatile int        x_target     = 0;
static volatile int        x_pos        = 0;
static volatile int        x_pulse      = 0;
static volatile uint32_t   next_event_time = 0;


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
	RTC_REG_WRITE(FRC1_LOAD_ADDRESS, x_period1);
	timer->frc1_int &= ~FRC1_INT_CLR_MASK;
	next_event_time = GetCycleCount() + x_period1;
	timer->frc1_ctrl = TIMER1_DIVIDE_BY_16 | TIMER1_ENABLE_TIMER;
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
	int tmp;
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
	/* ABS */
	if (xSteps < 0) xSteps = -xSteps;
	/* Set period assume 5MHz timer1 clock (80MHz / 16) */
	if (xSteps) {
#ifdef CORRECT_PERIOD
		tmp = ((duration * 5000)/xSteps)-29;
#else
		tmp = ((duration * 5000)/xSteps)-10;
#endif
	} else {
		tmp = 10000;
	}
	if (tmp < MIN_PERIOD) tmp = MIN_PERIOD;
	/* Calculate half period */
	x_period0 = (tmp >> 1);
	x_period1 = tmp - x_period0;

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

// Speed critical bits
#pragma GCC optimize ("O2")
// Normally would not want two copies like this, but due to different
// optimization levels the inline attribute gets lost if we try the
// other version.

static inline ICACHE_RAM_ATTR uint32_t GetCycleCountIRQ()
{
	uint32_t ccount;
	__asm__ __volatile__("rsr %0,ccount":"=a"(ccount));
	return ccount;
}
//===========================================================================================

/*!
 * \brief timer1 interrupt handler.
 */
void ICACHE_RAM_ATTR motion_intr_handler(void)
{
#ifdef CORRECT_PERIOD
	uint32_t c, tmp;
#endif	
	if (x_pulse) {
		asm volatile ("" : : : "memory");
		gpio_r->out_w1tc = (uint32_t)(x_gpio_mask);
		x_pulse = 0;
		if (x_pos == x_target) {
			/* We are in target position :-) - disable timer */
			timer->frc1_int &= ~FRC1_INT_CLR_MASK;
			timer->frc1_ctrl = 0;
			int_active = 0;
		} else { 
			/* Start timer */
#ifdef CORRECT_PERIOD
		next_event_time += x_period0;
		c = GetCycleCountIRQ();
		tmp = next_event_time - c; 
		if (tmp > x_period0) { 
			/* period is too short - step missed */
			tmp = x_period0; 
			next_event_time = c + x_period0; 
		}
		timer->frc1_int &= ~FRC1_INT_CLR_MASK;
		WRITE_PERI_REG(&timer->frc1_load, tmp);
#else			
		timer->frc1_int &= ~FRC1_INT_CLR_MASK;
		WRITE_PERI_REG(&timer->frc1_load, x_period0);
#endif			
		}
	} else if (x_pos != x_target) {
		asm volatile ("" : : : "memory");
		gpio_r->out_w1ts = (uint32_t)(x_gpio_mask);
		x_pulse = 1;
		if (x_pos > x_target) x_pos--; else x_pos++;
		/* Start timer */
#ifdef CORRECT_PERIOD
		next_event_time += x_period1;
		c = GetCycleCountIRQ();
		tmp = next_event_time - c;
		if (tmp > x_period1) { 
			/* period is too short - step missed */
			tmp = x_period1; 
			next_event_time = c + x_period1; 
		}
		timer->frc1_int &= ~FRC1_INT_CLR_MASK;
		WRITE_PERI_REG(&timer->frc1_load, tmp);
#else			
		timer->frc1_int &= ~FRC1_INT_CLR_MASK;
		WRITE_PERI_REG(&timer->frc1_load, x_period1);
#endif		
	} else {
		/* We are in target position :-) - disable timer */
		timer->frc1_int &= ~FRC1_INT_CLR_MASK;
		timer->frc1_ctrl = 0;
		int_active = 0;
	}
}
//===========================================================================================

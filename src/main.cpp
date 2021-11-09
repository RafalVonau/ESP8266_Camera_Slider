#include <ESP8266WiFi.h>
extern "C" {
	#include <osapi.h>
	#include <os_type.h>
}
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include "Motion1D.h"
#include <TMCStepper.h>
#include "NetworkCommand.h"
#include "HTTPCommand.h"
#include "UdpLogger.h"

/* SWITCHES */
#define HOSTNAME                 "slider"
#define NPORT                    (2500)


#ifdef DEBUG_ENABLED
UdpLoggerClass     UdpLogger;
#endif
DNSServer          dnsServer;
class WiFiManager {
	public:
	WiFiManager() {}
	void autoConnect(String x) {
		WiFi.softAP("SliderAP");
		dnsServer.start(53, "*", WiFi.softAPIP());
	}
};
#include "secrets.h"


//Rotational Stepper: ("X - EGG")
#define step1        14
#define dir1         13
#define enableMotor  2
//#define GPIO_LED_PIN 2
#define SERIAL_PORT Serial // TMC2208/TMC2224 HardwareSerial port
#define R_SENSE 0.11  // SilentStepStick series use 0.11     Watterott TMC5160 uses 0.075


TMC2208Stepper driver = TMC2208Stepper(&SERIAL_PORT, R_SENSE); // Hardware Serial0
Motion1D          *m1d;
CommandDB         CmdDB;
NetworkCommand    *NCmd;
HTTPCommand       *HCmd;
volatile int ota_in_progress = 0;
static int current_microsteps = 256;
static void makeCmdInterface();

void setup()
{
	/* Setup pins */
	Serial.begin(115200);
	pinMode(enableMotor, OUTPUT);
	pinMode(step1, OUTPUT);
	pinMode(dir1, OUTPUT);
	digitalWrite(enableMotor, HIGH);
	/* Connect to WiFi */
	detect_network();
#ifdef DEBUG_ENABLED
	UdpLogger.init(12345, "slider: ");
	//UdpLogger.WriteStartMessage();
#endif
	pdebug("WIFI::IP address: %s\n", WiFi.localIP().toString().c_str());
	/* Setup OTA */
	ArduinoOTA.setHostname(HOSTNAME);
	ArduinoOTA.onStart([]() {
		ota_in_progress = 1;
	});
	ArduinoOTA.begin();
	
	makeCmdInterface();
	
	/* Setup driver */
	digitalWrite(enableMotor, LOW);    // Enable driver in hardware
	driver.begin();
	driver.pdn_disable(true);          // Use PDN/UART pin for communication
	driver.I_scale_analog(false);      // Use internal voltage reference
	driver.rms_current(400);           // Set motor RMS current
	driver.mstep_reg_select(1);        // necessary for TMC2208 to set microstep register with UART
	driver.microsteps(8);
	current_microsteps = 8;
	driver.toff(5);                    // Enables driver in software
	//  driver.en_pwm_mode(true);      // Enable stealthChop
	driver.pwm_autoscale(true);        // Needed for stealthChop
	m1d = new Motion1D(step1, dir1, enableMotor);
	pdebug("Setup done :-)\n");
}
//====================================================================================

void loop()
{
	/* Handle OTA */
	ArduinoOTA.handle();
	if (ota_in_progress) return;
	/* Execute command from queue */
	if ( m1d->loop() ) {
		CmdDB.loop();
	} else {
		CmdDB.loopMotion();
		CmdDB.loop();
	}
}
//====================================================================================

static int g_pos_x = 0;

static void stepperMove(CommandQueueItem *c)
{
	int duration = c->m_arg0;
	int rotStepsEBB   = c->m_arg1;

	if ((c->m_arg_mask & 3) != 3) {
		c->sendError();
		return;
	}
	c->sendAck();
	if ( rotStepsEBB == 0 ) {
		delay(duration);
		return;
	}
	m1d->goTo(duration, rotStepsEBB);
}
//====================================================================================

#define MAX_DIST_MOTTOR (45)

static void stepperMoveSmart(CommandQueueItem *c)
{
	int d        = c->m_arg0;
	int newS     = c->m_arg1;
	int newE     = c->m_arg2;
	int duration, aX, dX;
	int speed    = 3000;

	if ((c->m_arg_mask & 7) != 7) {
		c->sendError();
		return;
	}
	
	/* Limit move distance */
	if (newS > MAX_DIST_MOTTOR) newS = MAX_DIST_MOTTOR;
	if (newS < 0) newS = 0;
	if (newE > MAX_DIST_MOTTOR) newE = MAX_DIST_MOTTOR;
	if (newE < 0) newE = 0;
	
	/* Convert to microsteps */
	newS*=(200 * current_microsteps);
	newE*=(200 * current_microsteps);

	dX = newS - g_pos_x;
	if (dX < 0) aX = -dX; else aX = dX;

	/* Calculate duration in [ms] */
	if (aX) {
		duration = (aX * 1000)/speed;
		m1d->goTo(duration, dX);
	}
	g_pos_x = newS;

	dX = newE - g_pos_x;
	if (dX < 0) aX = -dX; else aX = dX;
	if (aX) {
		m1d->goTo(d, dX);
	}
	g_pos_x = newE;
	c->sendAck();
}
//====================================================================================

static void stepperMoveRelative(CommandQueueItem *c)
{
	int d        = c->m_arg0;
	int newS     = g_pos_x + (c->m_arg1 * 200 * current_microsteps);
	int aX, dX, mm = MAX_DIST_MOTTOR * 200 * current_microsteps;

	if ((c->m_arg_mask & 3) != 3) {
		c->sendError();
		return;
	}
	/* Limit move distance */
	if (newS > mm) newS = mm;
	if (newS < 0) newS = 0;
	
	dX = newS - g_pos_x;
	if (dX < 0) aX = -dX; else aX = dX;
	if (aX) {
		m1d->goTo(d, dX);
	}
	g_pos_x = newS;
	c->sendAck();
}
//====================================================================================


void cmdG90(CommandQueueItem *c)
{
	g_pos_x = 0;
	c->sendAck();
}
//====================================================================================

static void cmdCurrent(CommandQueueItem *c)
{
	if ((c->m_arg_mask & 1) != 1) {
		c->sendError();
		return;
	}
	driver.rms_current(c->m_arg0);
	c->sendAck();
}
//====================================================================================

static void cmdSteps(CommandQueueItem *c)
{
	if ((c->m_arg_mask & 1) != 1) {
		c->sendError();
		return;
	}
	driver.mstep_reg_select(1);        // necessary for TMC2208 to set microstep register with UART
	driver.microsteps(c->m_arg0);
	current_microsteps = c->m_arg0;
	c->sendAck();
}
//====================================================================================



static void enableMotors(CommandQueueItem *c)
{
	int value = -1, cmd = -1;

	if (c->m_arg_mask & 1) cmd = c->m_arg0;
	if (c->m_arg_mask & 2) value = c->m_arg1;

	if ((cmd != -1) && (value == -1)) {
		switch (cmd) {
			case 0: {m1d->motorsOff();c->sendAck();}break;
			case 1: {m1d->motorsOn();c->sendAck();}break;
			default: c->sendError(); break;
		}
	}
	if ((cmd != -1) && (value != -1)) {
		switch (value) {
			case 0: {m1d->motorsOff();c->sendAck();}break;
			case 1: {m1d->motorsOn();c->sendAck();}break;
			default: c->sendError(); break;
		}
	}
}
//====================================================================================

static void unrecognized(const char *command, Command *c) {c->print("!8 Err: Unknown command\r\n");}

static void makeCmdInterface()
{
	CmdDB.addCommand("v",[](CommandQueueItem *c) {
		c->print("Slider-Firmware V1.0\r\n");
	});
	CmdDB.addCommand("EM",enableMotors);
	CmdDB.addCommand("SM",stepperMove, true);
	CmdDB.addCommand("M",stepperMoveSmart, true);
	CmdDB.addCommand("MR",stepperMoveRelative, true);
	CmdDB.addCommand("G90", cmdG90, true);
	CmdDB.addCommand("C", cmdCurrent, true);
	CmdDB.addCommand("S", cmdSteps, true);
	CmdDB.addCommand("XX",[](CommandQueueItem *c){m1d->printStat(c);});
	CmdDB.setDefaultHandler(unrecognized); // Handler for command that isn't matched (says "What?")

	NCmd = new NetworkCommand(&CmdDB, NPORT);
	HCmd = new HTTPCommand(&CmdDB);
}
//====================================================================================

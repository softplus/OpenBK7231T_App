#include "../new_common.h"
#include "../new_pins.h"
#include "../new_cfg.h"
// Commands register, execution API and cmd tokenizer
#include "../cmnds/cmd_public.h"
#include "../mqtt/new_mqtt.h"
#include "../logging/logging.h"
#include "../hal/hal_pins.h"
#include "drv_local.h"
#include "drv_uart.h"
#include "../httpserver/new_http.h"

#include "BkDriverTimer.h"
#include "BkDriverGpio.h"
#include "sys_timer.h"
#include "gw_intf.h"

// HLW8012 aka BL0937
#define ELE_HW_TIME 1
#define HW_TIMER_ID 0

// Those can be set by Web page pins configurator
// The below are default values for Mycket smart socket
int GPIO_HLW_SEL = 24; // pwm4
int GPIO_HLW_CF = 7;
int GPIO_HLW_CF1 = 8;

bool g_sel = true;
uint32_t res_v = 0;
uint32_t res_c = 0;
uint32_t res_p = 0;
float calib_v = 0.13253012048f;
float calib_p = 1.5f;
float calib_c = 0.0118577075f;

// counters for pulses
volatile uint32_t g_vc_pulses = 0;
volatile uint32_t g_p_pulses = 0;
#define POWER_LOOPS 15
volatile int g_pwr_count = 0;

// Service Voltage and Current
void HlwCf1Interrupt(unsigned char pinNum) { g_vc_pulses++; }
// Service Power
void HlwCfInterrupt(unsigned char pinNum) { g_p_pulses++; }

// sets scaling factor based on actual value supplied
int BL0937_CalibSet(const char *args, uint32_t counter, float *calib) {
	float actual;
	if(args==0||*args==0) {
		addLogAdv(LOG_INFO, LOG_FEATURE_ENERGYMETER,"This command needs one argument");
		return 1;
	}
	actual = atof(args);
	*calib = POWER_LOOPS * actual / counter;
	char dbg[128];
	snprintf(dbg, sizeof(dbg), "Calibration: you gave %f, set REF to %f\n", actual, *calib);
	addLogAdv(LOG_INFO, LOG_FEATURE_ENERGYMETER,dbg);
	return 0;
}

int BL0937_PowerSet(const void *context, const char *cmd, const char *args, int cmdFlags) {
	return BL0937_CalibSet(args, res_p, &calib_p);
}
int BL0937_VoltageSet(const void *context, const char *cmd, const char *args, int cmdFlags) {
	return BL0937_CalibSet(args, res_v, &calib_v);
}
int BL0937_CurrentSet(const void *context, const char *cmd, const char *args, int cmdFlags) {
	return BL0937_CalibSet(args, res_c, &calib_c);
}

// set scaling factor directly
int BL0937_RefSet(const char *args, float *ref) {
	if(args==0||*args==0) {
		addLogAdv(LOG_INFO, LOG_FEATURE_ENERGYMETER,"This command needs one argument");
		return 1;
	}
	*ref = atof(args);
	return 0;
}
int BL0937_PowerRef(const void *context, const char *cmd, const char *args, int cmdFlags) {
	return BL0937_RefSet(args, &calib_p); 
}
int BL0937_CurrentRef(const void *context, const char *cmd, const char *args, int cmdFlags) {
	return BL0937_RefSet(args, &calib_c); 
}
int BL0937_VoltageRef(const void *context, const char *cmd, const char *args, int cmdFlags) {
	return BL0937_RefSet(args, &calib_v); 
}

// initialize interrupts for power meter
void BL0937_Init() {
	// if not found, this will return the already set value
	GPIO_HLW_SEL = PIN_FindPinIndexForRole(IOR_BL0937_SEL,GPIO_HLW_SEL);
	GPIO_HLW_CF = PIN_FindPinIndexForRole(IOR_BL0937_CF,GPIO_HLW_CF);
	GPIO_HLW_CF1 = PIN_FindPinIndexForRole(IOR_BL0937_CF1,GPIO_HLW_CF1);

	// outputs
	HAL_PIN_Setup_Output(GPIO_HLW_SEL);
    HAL_PIN_SetOutputValue(GPIO_HLW_SEL, g_sel);

	// set interrupts
	HAL_PIN_Setup_Input_Pullup(GPIO_HLW_CF1);
    gpio_int_enable(GPIO_HLW_CF1,IRQ_TRIGGER_FALLING_EDGE,HlwCf1Interrupt);
	HAL_PIN_Setup_Input_Pullup(GPIO_HLW_CF);
    gpio_int_enable(GPIO_HLW_CF,IRQ_TRIGGER_FALLING_EDGE,HlwCfInterrupt);

	// register commands for cmd-line interface
	CMD_RegisterCommand("PowerSet","",BL0937_PowerSet, "Sets current power value for calibration", NULL);
	CMD_RegisterCommand("VoltageSet","",BL0937_VoltageSet, "Sets current V value for calibration", NULL);
	CMD_RegisterCommand("CurrentSet","",BL0937_CurrentSet, "Sets current I value for calibration", NULL);
	CMD_RegisterCommand("PREF","",BL0937_PowerRef, "Sets the calibration multiplier", NULL);
	CMD_RegisterCommand("VREF","",BL0937_VoltageRef, "Sets the calibration multiplier", NULL);
	CMD_RegisterCommand("IREF","",BL0937_CurrentRef, "Sets the calibration multiplier", NULL);

	g_pwr_count = POWER_LOOPS;
}

// This function is run with the 1s timer
void BL0937_RunFrame() {
	float final_v, final_c, final_p;

	// average over a few loops for more accuracy / stability
	g_pwr_count--; if (g_pwr_count>0) return;
	g_pwr_count = POWER_LOOPS;

	if (g_sel) { // alternate between voltage & current
		res_v = g_vc_pulses;
		g_sel = false;
	} else {
		res_c = g_vc_pulses;
		g_sel = true;
	}
    HAL_PIN_SetOutputValue(GPIO_HLW_SEL, g_sel);
	g_vc_pulses = 0;

	res_p = g_p_pulses;
	g_p_pulses = 0;

	final_v = res_v * calib_v / POWER_LOOPS;
	final_c = res_c * calib_c / POWER_LOOPS;
	final_p = res_p * calib_p / POWER_LOOPS;

#if 1
	char buff[200];
	snprintf(buff, sizeof(buff), "Pulses: divider: %i, voltage: %i, current: %i, power: %i\n", 
			g_pwr_count, res_v, res_c, res_p);
	addLogAdv(LOG_DEBUG, LOG_FEATURE_ENERGYMETER, buff);

	snprintf(buff, sizeof(buff), "Values: voltage %f, current %f, power %f\n", 
			final_v, final_c, final_p);
	addLogAdv(LOG_DEBUG, LOG_FEATURE_ENERGYMETER, buff);

#endif
#if 0

		MQTT_PublishMain_StringFloat("power_count", (float)res_p);
		MQTT_PublishMain_StringFloat("power_accuracy_watt", 2.0 * calib_p / POWER_LOOPS);
#endif
	BL_ProcessUpdate(final_v,final_c,final_p);
}


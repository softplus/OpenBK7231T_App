
#include "../../new_common.h"
#include "../../logging/logging.h"


#include "../../beken378/app/config/param_config.h"


#include "drv_model_pub.h"
#include "wdt_pub.h"

// main timer tick every 1s
beken_timer_t g_main_timer_1s;


#define LOG_FEATURE LOG_FEATURE_MAIN


void user_main(void)
{
    OSStatus err;
	Main_Init();


  err = rtos_init_timer(&g_main_timer_1s,
                        1 * 1000,
                        Main_OnEverySecond,
                        (void *)0);
  ASSERT(kNoErr == err);

  err = rtos_start_timer(&g_main_timer_1s);
  ASSERT(kNoErr == err);
	ADDLOGF_DEBUG("started timer\r\n");
}

#if PLATFORM_BK7231N

// right now Free is somewhere else

#else

#undef Free
// This is needed by tuya_hal_wifi_release_ap.
// How come that the Malloc was not undefined, but Free is?
// That's because Free is defined to os_free. It would be better to fix it elsewhere
void Free(void* ptr)
{
    os_free(ptr);
}


#endif


// copied from platforms/bk7231n/tuya_os_adapter/src/system/tuya_os_adapt_system.c
// start the hardware watchdog with 'timeval' seconds timeout (max 60s)
unsigned int watchdog_init_start(const unsigned int timeval)
{
    unsigned int ret;
    //init 
    int cyc_cnt = timeval * 1000;
    
    if(cyc_cnt > 0xFFFF) { /* 60s */
        cyc_cnt = 0xFFFF;
    }
    
    //init wdt
    ret = sddev_control(WDT_DEV_NAME, WCMD_SET_PERIOD, &cyc_cnt);
    
    // start wdt timer
    ret |= sddev_control(WDT_DEV_NAME, WCMD_POWER_UP, NULL);

    if(ret != 0) {
        ADDLOGF_ERROR("watchdog_init_start(): watch dog set error!\r\n");
        return 0;
    }
    
    return timeval;
}

// ping the watchdog to let it know we're ok
void watchdog_refresh(void)
{
    if(sddev_control(WDT_DEV_NAME, WCMD_RELOAD_PERIOD, NULL) != 0) {
        ADDLOGF_ERROR("watchdog_refresh(): refresh watchdog err!\r\n");
    }
}

// stop the watchdog
void watchdog_stop(void)
{
    sddev_control(WDT_DEV_NAME, WCMD_POWER_DOWN, NULL);
}
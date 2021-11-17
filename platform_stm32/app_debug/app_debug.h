#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stdio.h>

#define DEBUG_LEVEL_ALL         0
#define DEBUG_LEVEL_VERBOSE     1
#define DEBUG_LEVEL_INFO        2
#define DEBUG_LEVEL_WARN        3
#define DEBUG_LEVEL_ERROR       4

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL             DEBUG_LEVEL_VERBOSE  
#endif

#include "SEGGER_RTT.h"
#if 1       // RTT color
#define KNRM  "\x1B[0m"
#define KRED  RTT_CTRL_TEXT_RED
#define KGRN  RTT_CTRL_TEXT_GREEN
#define KYEL  RTT_CTRL_TEXT_YELLOW
#define KBLU  RTT_CTRL_TEXT_BLUE
#define KMAG  RTT_CTRL_TEXT_MAGENTA
#define KCYN  RTT_CTRL_TEXT_CYAN
#define KWHT  RTT_CTRL_TEXT_WHITE
#else   // Teraterm
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#endif

#define DEBUG_RAW                               app_debug_rtt_raw


#define DEBUG_DUMP                              app_debug_dump

#if (DEBUG_LEVEL_VERBOSE >= DEBUG_LEVEL)
#define DEBUG_VERBOSE(s, args...)               app_debug_rtt_raw(KMAG "<%u> [I] %s : " s KNRM,  sys_get_tick_ms(), "", ##args)
#else
#define DEBUG_VERBOSE(s, args...)               app_debug_rtt_nothing(s, ##args)
#endif

#if (DEBUG_LEVEL_INFO >= DEBUG_LEVEL)
#define DEBUG_INFO(s, args...)                  app_debug_rtt_raw(KGRN "<%u> [I] %s : " s KNRM,  sys_get_tick_ms(), "", ##args)
#else
#define DEBUG_INFO(s, args...)                  app_debug_rtt_nothing(s, ##args)
#endif

#if (DEBUG_LEVEL_ERROR >= DEBUG_LEVEL)
#define DEBUG_ERROR(s, args...)                 app_debug_rtt_raw(KRED "<%u> [E] %s : " s KNRM,  sys_get_tick_ms(), "", ##args)
#else
#define DEBUG_ERROR(s, args...)                 app_debug_rtt_nothing(s, ##args)
#endif

#if (DEBUG_LEVEL_WARN >= DEBUG_LEVEL)
#define DEBUG_WARN(s, args...)                  app_debug_rtt_raw(KYEL "<%u> [W] %s : " s KNRM,  sys_get_tick_ms(), "", ##args)
#else
#define DEBUG_WARN(s, args...)                  app_debug_rtt_nothing(s, ##args)
#endif



#define DEBUG_COLOR(color, s, args...)          app_debug_rtt_raw(color s KNRM, ##args)


#define DEBUG_PRINTF                            app_debug_rtt
#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(String...)	SEGGER_RTT_printf(0, String)
#endif

#ifndef DEBUG_FLUSH
#define DEBUG_FLUSH()      while(0)
#endif

extern uint32_t sys_get_tick_ms(void);

int32_t app_debug_rtt_nothing(const char *fmt,...);

int32_t app_debug_rtt(const char *fmt,...);

int32_t app_debug_rtt_raw(const char *fmt,...);

void app_debug_dump(const void* data, int len, const char* string, ...);

#endif // !DEBUG_H




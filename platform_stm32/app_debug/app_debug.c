#include "app_debug.h"
#include <stdarg.h>
#include "stdio.h"
#include <string.h>
#include <ctype.h>
#include "main.h"

//#include "gd32e10x.h"

//#define SEGGER_RTT_PRINTF_BUFFER_SIZE 256
static char m_debug_buffer[SEGGER_RTT_PRINTF_BUFFER_SIZE];

int32_t app_debug_rtt_nothing(const char *fmt,...)
{
    return -1;
}

int32_t app_debug_rtt(const char *fmt,...)
{
    int32_t     n;

    char *p = &m_debug_buffer[0];
    int32_t size = SEGGER_RTT_PRINTF_BUFFER_SIZE;
    int32_t time_stamp_size;

    p += sprintf(m_debug_buffer, "<%lu>: ", sys_get_tick_ms());
    time_stamp_size = (p-m_debug_buffer);
    size -= time_stamp_size;
    va_list args;

    va_start (args, fmt);
    n = vsnprintf(p, size, fmt, args);
    if (n > (int)size) 
    {
        SEGGER_RTT_Write(0, m_debug_buffer, size + time_stamp_size);
    } 
    else if (n > 0) 
    {
        SEGGER_RTT_Write(0, m_debug_buffer, n + time_stamp_size);
    }
    va_end(args);
    
    return n;
}

int32_t app_debug_rtt_raw(const char *fmt,...)
{
    int32_t     n;

    char *p = &m_debug_buffer[0];
    int32_t size = SEGGER_RTT_PRINTF_BUFFER_SIZE;
    va_list args;

    va_start (args, fmt);
    n = vsnprintf(p, size, fmt, args);
    if (n > (int)size) 
    {
        SEGGER_RTT_Write(0, m_debug_buffer, size);
    } 
    else if (n > 0) 
    {
        SEGGER_RTT_Write(0, m_debug_buffer, n);
    }
    va_end(args);
    
    return n;
}

void app_debug_dump(const void* data, int len, const char* string, ...)
{
	uint8_t* p = (uint8_t*)data;
    uint8_t  buffer[16];
    int32_t i_len;
    int32_t i;

//    DEBUG_RAW("%s %u bytes\n", string, len);
    while (len > 0)
    {
        i_len = (len > 16) ? 16 : len;
        memset(buffer, 0, 16);
        memcpy(buffer, p, i_len);
        for (i = 0; i < 16; i++)
        {
            if (i < i_len)
                DEBUG_RAW("%02X ", buffer[i]);
            else
                DEBUG_RAW("   ");
        }
        DEBUG_RAW("\t");
        for (i = 0; i < 16; i++)
        {
            if (i < i_len)
            {
                if (isprint(buffer[i]))
				{
                    DEBUG_RAW("%c", (char)buffer[i]);
				}
                else
				{
                    DEBUG_RAW(".");
				}
            }
            else
                DEBUG_RAW(" ");
        }
        DEBUG_RAW("\r\n");
        len -= i_len;
        p += i_len;
    }
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gsm.h"
#include "gsm_utilities.h"

bool gsm_utilities_get_signal_strength_from_buffer(uint8_t *buffer, uint8_t *csq)
{
    char *tmp_buff = strstr((char *)buffer, "+CSQ:");

    if (tmp_buff == NULL)
    {
        return false;
    }

    *csq = gsm_utilities_get_number_from_string(6, tmp_buff);
    return true;
}

void gsm_utilities_get_imei(uint8_t *imei_buffer, uint8_t *result, uint8_t max_lenth)
{
    uint8_t count = 0;
    uint8_t tmp_count = 0;

    for (count = 0; count < strlen((char *)imei_buffer); count++)
    {
        if (imei_buffer[count] >= '0' && imei_buffer[count] <= '9')
        {
            result[tmp_count++] = imei_buffer[count];
        }

        if (tmp_count >= max_lenth)
        {
            result[tmp_count-1] = 0;
            break;
        }
    }

    result[tmp_count] = 0;
}

void gsm_utilities_get_sim_ccid(uint8_t *imei_buffer, uint8_t *result, uint8_t max_lenth)
{
	/* +QCCID: 8984012012120000151F\r\nOK\r\n*/
	char *p = strstr((char*)imei_buffer, "+QCCID: ");
	if (p == NULL)
	{
		*result = 0;
		return;
	}
	p += strlen("+QCCID: ");
	
    uint8_t count = 0;
    uint8_t tmp_count = 0;

    for (count = 0; count < strlen((char *)p); count++)
    {
        if (p[count] >= '0' && p[count] <= '9')
        {
            result[tmp_count++] = p[count];
        }
		else
		{
			break;
		}
        if (tmp_count >= max_lenth)
        {
            result[tmp_count-1] = 0;
            break;
        }
    }

    result[tmp_count] = 0;
}

/*
+CUSD: 1,"84353078550. TKG: 0d, dung den 0h ngay 18/02/2020. Bam chon dang ky:1. 15K=3GB/3ngay2. 30K=7GB/7ngayHoac bam goi *098#",15
OK
hoac
+CUSD: 4
+CME ERROR: unknown
*/
void gsm_utilities_process_cusd_message(char *buffer, char *money, uint32_t max_len)
{
#if 1	
	char *cusd = strstr(buffer, "+CUSD: ");
	if (cusd)
	{
            memcpy(money, cusd, max_len);
	}
        else
        {
            if (max_len > 3)
            {
                max_len = 3;
                memcpy(money, "NA", max_len);
            }
        }
#endif
}


bool gsm_utilities_get_network_access_tech(char *buffer, uint8_t *access_technology)
{
    /**
	* +CGREG: 2,1,"3279","487BD01",7
	*
	* OK
	*/
    char *tmp_buff = strstr(buffer, "+CGREG:");
    if (tmp_buff == NULL)
        return false;

    uint8_t comma_idx[12] = {0};
    uint8_t index = 0;
    for (uint8_t i = 0; i < strlen(tmp_buff); i++)
    {
        if (tmp_buff[i] == ',')
        {
            comma_idx[index++] = i;
        }
    }

    if (index >= 4)
    {
        *access_technology = gsm_utilities_get_number_from_string(comma_idx[3] + 1, tmp_buff);

        if (*access_technology > 9)
            *access_technology  = 9;
        return true;
    }

    return false;
}

/*****************************************************************************/
/*!
 * @brief	:  
 * @param	:  +COPS: 0,0,"Viettel Viettel",7
 * @retval	:
 * @author	:	Phinht
 * @created	:	15/10/2015
 * @version	:
 * @reviewer:	
 */
void gsm_utilities_get_network_operator(char *buffer, char *nw_operator, uint8_t max_len)
{
    /**
	* AT+COPS=? 
	* +COPS: (2,"Viettel","Viettel","45204",7),(1,"VIETTEL","VIETTEL","45204",2),(1,"VIETTEL","VIETTEL","45204",0),
	* (1,"Vietnamobile","VNMOBILE","45205",2),(1,"VN VINAPHONE","GPC","45202",2),(1,"Vietnamobile","VNMOBILE","45205",0),
	* (1,"VN Mobifone","Mobifone","45201",7),(1,"VN VINAPHONE","GPC","45202",0),(1,"VN Mobifone","Mobifone","45201",0),
	* (1,"VN VINAPHONE","GPC","45202",7),(1,"VN Mobifone","Mobifone","45201",2),,(0-4),(0-2)
	*
	* Query selected operator: 
	* AT+COPS?
	* +COPS: 0,0,"Viettel Viettel",7
	*
	* OK
	*/
#if 1
        *nw_operator = 0;
	char *tmp_buff = strstr(buffer, "+COPS:");
	if(tmp_buff == NULL) return;	
	
	uint8_t comma_idx[5] = {0};
	uint8_t index = 0;
	for(uint8_t i = 0; i < strlen(tmp_buff); i++)
	{
		if(tmp_buff[i] == '"') comma_idx[index++] = i;
	}
	if(index >= 2)
	{
		uint8_t length = comma_idx[1] - comma_idx[0];
		if(length > max_len) length = max_len;
		
		//Copy operator name
		memcpy(nw_operator, &tmp_buff[comma_idx[0] + 1], length - 1);
	}
#endif
}

// +HTTPACTION: 0,200,12314\r\n
// +QHTTPGET: 0,200,12314\r\n
// +QHTTPREAD: 0,200,12314\r\n
bool gsm_utilities_parse_http_action_response(char *response, uint32_t *error_code, uint32_t *content_length)
{
    bool retval = false;
    char *p;
    p = strstr(response, ": 0,");
    if (p)
    {
        p += strlen(": 0,");
        *error_code = gsm_utilities_get_number_from_string(0, p);
        if (*error_code == 200)
        {
            retval = true;
        }
        
        p = strstr(p, ",");
        if (p)
        {
            p++;
            *content_length = gsm_utilities_get_number_from_string(0, p);
        }
        else
        {
            *content_length = 0;
        }
    }
    return retval;
}

int32_t gsm_utilities_parse_httpread_msg(char *buffer, uint8_t **begin_data_pointer)
{
#if 0 // SIMCOM
    // +HTTPREAD: 123\r\nData
    char tmp[32];
    char *p = strstr(buffer, "+HTTPREAD: ");
    if (p == NULL)
    {
        return -1;
    }

    p += strlen("+HTTPREAD: ");
    if (strstr(p, "\r\n") == NULL)
    {
        return -1;
    }

    for (uint32_t i = 0; i < (sizeof(tmp) - 1); i++, p++)
    {
        if (*p != '\r')
        {
            tmp[i] = *p;
        }
        else
        {
            tmp[i] = '\0';
            break;
        }
    }
    p += 2; // Skip \r\n
    *begin_data_pointer = (uint8_t*)p;

    return atoi(tmp);
#else // Quectel
    char tmp[32];
    char *p = strstr(buffer, "CONNECT\r\n");
    if (p == NULL)
    {
        return -1;
    }

    p += strlen("CONNECT\r\n");
    if (strstr(p, "OK\r\n") == NULL)
    {
        return -1;
    }

    for (uint32_t i = 0; i < (sizeof(tmp) - 1); i++, p++)
    {
        if (*p != '\r')
        {
            tmp[i] = *p;
        }
        else
        {
            tmp[i] = '\0';
            break;
        }
    }
    p += 2; // Skip \r\n
    *begin_data_pointer = (uint8_t *)p;

    return atoi(tmp);
#endif
}

/*
 * 	Ham doc mot so trong chuoi bat dau tu dia chi nao do.
 *	Buffer = abc124mff thi gsm_utilities_get_number_from_string(3,Buffer) = 123
 *
 */
uint32_t gsm_utilities_get_number_from_string(uint16_t begin_index, char *buffer)
{
    // assert(buffer);

    uint32_t value = 0;
    uint16_t tmp = begin_index;
    uint32_t len = strlen(buffer);
    while (buffer[tmp] && tmp < len)
    {
        if (buffer[tmp] >= '0' && buffer[tmp] <= '9')
        {
            value *= 10;
            value += buffer[tmp] - 48;
        }
        else
        {
            break;
        }
        tmp++;
    }

    return value;
}

int32_t find_index_of_char(char char_to_find, char *buffer_to_find)
{
    uint32_t tmp_cnt = 0;
    uint32_t max_length = 0;

    /* Do dai du lieu */
    max_length = strlen(buffer_to_find);

    for (tmp_cnt = 0; tmp_cnt < max_length; tmp_cnt++)
    {
        if (buffer_to_find[tmp_cnt] == char_to_find)
        {
            return tmp_cnt;
        }
    }
    return -1;
}

bool gsm_utilities_copy_parameters(char *src, char *dst, char comma_begin, char comma_end)
{
    int16_t begin_idx = find_index_of_char(comma_begin, src);
    int16_t end_idx;
    if (comma_begin == comma_end)
    {
        end_idx = find_index_of_char(comma_end, src+1);
        if (end_idx != -1)
        {
            end_idx++;
        }
    }
    else
    {
        end_idx = find_index_of_char(comma_end, src);
    }
    int16_t tmp_cnt, i = 0;

    if (begin_idx == -1 || end_idx == -1)
    {
        return false;
    }

    if (end_idx - begin_idx <= 1)
    {
        return false;
    }

    for (tmp_cnt = begin_idx + 1; tmp_cnt < end_idx; tmp_cnt++)
    {
        dst[i++] = src[tmp_cnt];
    }

    dst[i] = 0;

    return true;
}

#if 0
bool gsm_utilities_parse_timestamp_buffer(char *response_buffer, void *date_time)
{
    // Parse response buffer
    // "\r\n+CCLK : "yy/MM/dd,hh:mm:ss+zz"\r\n\r\nOK\r\n        zz : timezone
    // \r\n+CCLK : "10/05/06,00:01:52+08"\r\n\r\nOK\r\n
    bool val = false;
    uint8_t tmp[4];
    char *p_index = strstr(response_buffer, "+CCLK");
    if (p_index == NULL)
    {
        goto exit;
    }

    while (*p_index && ((*p_index) != '"'))
    {
        p_index++;
    }
    if (*p_index == '\0')
    {
        goto exit;
    }
    p_index++;
    response_buffer = p_index;

    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, response_buffer, 2);
    ((rtc_date_time_t*)date_time)->year = atoi((char*)tmp);
    if (((rtc_date_time_t*)date_time)->year < 20) // 2020
    {
        // Invalid timestamp
        val = false;
    }
    else
    {
        // MM
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 3, 2);
        ((rtc_date_time_t*)date_time)->month = atoi((char*)tmp);

        // dd
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 6, 2);
        ((rtc_date_time_t*)date_time)->day = atoi((char*)tmp);

        // hh
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 9, 2);
        ((rtc_date_time_t*)date_time)->hour = atoi((char*)tmp);

        // mm
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 12, 2);
        ((rtc_date_time_t*)date_time)->minute = atoi((char*)tmp);

        // ss
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, response_buffer + 15, 2);
        ((rtc_date_time_t*)date_time)->second = atoi((char*)tmp);

        val = true;
    }

exit:
    return val;
}
#endif

//uint16_t gsm_utilities_crc16(const uint8_t* data_p, uint8_t length)
//{    
//    uint8_t x;
//    uint16_t crc = 0xFFFF;

//    while (length--)
//    {
//        x = crc >> 8 ^ *data_p++;
//        x ^= x>>4;
//        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
//    }
//    
//    return crc;
//}

void gsm_utilities_parse_file_handle(char *buffer, int32_t *file_handle)
{
	*file_handle = -1;
	char *p = strstr(buffer, "+QFOPEN: ");
	
	if (p)
	{
		p += strlen("+QFOPEN: ");
		*file_handle = gsm_utilities_get_number_from_string(0, p);
	}
}

void gsm_utilities_get_qfile_content(char *buffer, uint8_t **content, uint32_t *size)
{
	char *p = strstr(buffer, "CONNECT ");
	*size = 0;
	*content = (void*)0;
	if (p)
	{
		p += strlen("CONNECT ");
		*size = gsm_utilities_get_number_from_string(0, p);
		p = strstr(p, "\r\n");
		p += 2;
		*content = (uint8_t*)p;		
	}
}

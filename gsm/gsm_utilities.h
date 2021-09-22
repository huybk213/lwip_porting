#ifndef GSM_UTILITIES_H
#define GSM_UTILITIES_H

//#include "sys_ctx.h"
#include <stdint.h>
#include <stdbool.h>

/*!
 * @brief               Get gsm imei from buffer
 * @param[in]           imei_buffer raw buffer from gsm module
 * @param[out]          result result output
 * @param[in]           max_lenth Max result length
 * @note                Maximum imei length is 15
 */ 
void gsm_utilities_get_imei(uint8_t *imei_buffer, uint8_t * result, uint8_t max_lenth);


/*!
 * @brief				Get signal strength from buffer
 * @param[in]			buffer buffer response from GSM module
 * @param[out]			CSQ value
 * @note 				buffer = "+CSQ:..."
 * @retval 				TRUE Get csq success
 *         				FALSE Get csq fail
 */ 
bool gsm_utilities_get_signal_strength_from_buffer(uint8_t *buffer, uint8_t *csq);

/*!
 * @brief				Get Number from string
 * @param[in]			Index Begin index of buffer want to find
 * @param[in]			buffer Data want to search
 * @note				buffer = "abc124mff" =>> gsm_utilities_get_number_from_string(3, buffer) = 123
 * @retval				Number result from string
 */ 
uint32_t gsm_utilities_get_number_from_string(uint16_t index, char* buffer);

/*!
 * @brief				Get Number from buffer
 * @param[in]			Index Begin index of buffer want to find
 * @param[in]			buffer Data want to search
 * @param[in]			length Buffer length
 * @note				buffer = "abc124mff" =>> gsm_utilities_get_number_from_buffer(3, buffer) = 123
 * @retval				Number result from string
 */ 
uint32_t gsm_utilities_get_number_from_buffer(char* buffer, uint8_t offset, uint8_t length);
  
/*!
 * @brief				Caculate CRC16
 * @param[in]			data_p Data to caculate CRC16 value
 * @param[in]			length Length of data
 * @retval				CRC16 value
 */ 
uint16_t gsm_utilities_crc16(const uint8_t* data_p, uint8_t length);

/*!
 * @brief				get phone number from AT+CLIP message
 * @param[in]			data : Pointer to buffer data will be parsed
 * @param[in]			result : Phone number result
 * @note 				Format input data "0942018895",129,"",0,"",0
 */ 
void gsm_utilities_get_phone_number_from_at_clip(char * data, char * result);

/*!
 * @brief				Parse HTTP action response
 * @param[in]			response : Pointer to buffer data will be parsed
 * @param[out]			error_code : HTTP response code
 * @param[out]			content_length : HTTP response length
 * @note				Format data input "+HTTPACTION: 0,200,12314\r\n"
 * @retval				TRUE  : Message is valid format, operation success
 *         				FALSE : Message is invalid format, operation failed
 */ 
bool gsm_utilities_parse_http_action_response(char* response, uint32_t *error_code, uint32_t *content_length);

/*!
 * @brief				Parse HTTPRREAD message response size
 * @param[in]			buffer : Pointer to buffer data will be parsed
 * @param[out]			begin_data_pointer : Address of user data in buffer
 * @note				Format data input "+HTTPREAD: 123\r\nData...."
 * @retval				-1  : GSM HTTPREAD message error
 *         				Otherwise : HTTP DATA size
 */ 
int32_t gsm_utilities_parse_httpread_msg(char *buffer, uint8_t **begin_data_pointer);

/*!
 * @brief                       Get ip address from string
 * @param[in]                   buffer : Pointer to buffer data will be parsed
 * @param[out]                  ip_addr : Ip address output
 */ 
void gsm_utilities_get_ip_addr_from_string(char *buffer, void *ip_addr);


/*!
 * @brief                       Copy parameter from src to des
 * @param[in]                   src : Pointer to source buffer
 * @param[out]                  src : Pointer to des buffer
 * @param[in]                   find_char_begin : Begin charactor to find
 * @param[in]                   find_char_end : eegin charactor to find
 * @retval                      TRUE : Copy success
 *                              FALSE : Copy failed
 */ 
bool gsm_utilities_copy_parameters(char* src, char* des, char find_char_begin, char find_char_end);

/*!
 * @brief                       Caculate CRC16
 * @param[in]                   data_int : Pointer to source buffer
 * @param[in]                   nb_of_bytes : Size of buffer
 * @retval                      CRC16 value
 */ 
uint16_t gsm_utilities_calculate_crc16(uint8_t *data_int, uint8_t nb_of_bytes);


/*!
 * @brief                       Parse GSM HTTP timestamp response message
 * @param[in]                   buffer : Pointer to buffer data will be parsed
 * @param[out]                  date_time : Date time result
 * @note :                      Format message "\r\n+CCLK : "yy/MM/dd,hh:mm:ss+zz"\r\n\r\nOK\r\n"
 * @retval                      TRUE  : Operation success
 *                              FALSE : Response message is invalid format or invalid timestamp
 */ 
bool gsm_utilities_parse_timestamp_buffer(char *response_buffer, void *date_time);

///**
// * @brief Parse GSM HTTP timestamp response message
// * @param[in] buffer : Pointer to buffer data will be parsed
// * @param[out] date_time : Date time result
// * @note : Format message "\r\n+CCLK : "yy/MM/dd,hh:mm:ss+zz"\r\n\r\nOK\r\n"
// * @retval TRUE  : Operation success
// *         FALSE : Response message is invalid format or invalid timestamp
// */ 
//bool gsm_utilities_parse_timestamp_buffer(char *buffer, date_time_t *date_time);

///**
// * @brief Parse GSM timezone buffer
// * @param[in] buffer : Pointer to buffer data will be parsed
// * @param[out] timezone : Timezone result
// * @note : Format message "+CTZV: +28,0\r\n"
// * @retval TRUE  : Operation success
// *         FALSE : Response message is invalid format or invalid timezone
// */ 
//bool gsm_utilities_parse_timezone_buffer(char *buffer, int8_t *timezone);

///**
// * @brief Parse GSM timestamp psuttz message
// * @param[in] buffer : Pointer to buffer data will be parsed
// * @param[out] date_time : Datetime result
// * @param[out] gsm_adjust : Timezone
// * @note : Format message "*PSUTTZ: 2021,1,24,10,13,52,"+28",0\r\n"
// * @retval TRUE  : Operation success
// *         FALSE : Response message is invalid format or invalid timezone
// */ 
//bool gps_utilities_parse_timestamp_from_psuttz_message(char *buffer, date_time_t *date_time, int8_t *gsm_adjust);

/*!
 * @brief       Get GSM network operator
 * @param[in]   buffer : Pointer to buffer data will be parsed
 * @param[out]  nw_operator : Network operator
 * @param[in]   max_len : Max network operator name length
 * @note        Format message "+COPS: 0,0,"Viettel Viettel",7\r\n"
 */ 
void gsm_utilities_get_network_operator(char *buffer, char *nw_operator, uint8_t max_len);

/*!
 * @brief       Get GSM access technology
 * @param[in]   buffer : Pointer to buffer data will be parsed
 * @param[out]  access_technology : Access technology result
 * @note        Format message     +CGREG: 2,1,"3279","487BD01",7
 * @retval      TRUE : Operation success
 *              FALSE : Operation failed
 */ 
bool gsm_utilities_get_network_access_tech(char *buffer, uint8_t *access_technology);

/*!
 * @brief       Get GSM cusd money
 * @param[in]   buffer : Pointer to buffer data will be parsed
 * @param[out]  money : Money from str
 * @param[in]   max_len : Max len of money string
 * @note : +CUSD: 1,"84353078550. TKG: 0d, dung den 0h ngay 18/02/2020. Bam chon dang ky:1. 15K=3GB/3ngay2. 30K=7GB/7ngayHoac bam goi *098#",15
 */ 
void gsm_utilities_process_cusd_message(char *buffer, char *money, uint32_t max_len);


/*!
 * @brief       Get file handle from QOPEN message
 * @param[in]   buffer : Pointer to buffer data will be parsed
 * @param[out]  file_handler : -1 on error
 * @note : +QFOPEN: 3000\r\nOK\r\n
 */ 
void gsm_utilities_parse_file_handle(char *buffer, int32_t *file_handle);

/*!
 * @brief       Get file content from QREAD message
 * @param[in]   buffer : Pointer to buffer data will be parsed
 * @param[out]  content : Pointer to positions of data
 * @param[out]  size : Data size
 * @note : CONNECT size\r\n+ Raw_data +OK\r\n
 */ 
void gsm_utilities_get_qfile_content(char *buffer, uint8_t **content, uint32_t *size);

/*!
 * @brief               Get SIM CCID
 * @param[in]           imei_buffer raw buffer from gsm module
 * @param[out]          result result output
 * @param[in]           max_lenth Max result length
 * @note                Maximum imei length is 15
 */ 
void gsm_utilities_get_sim_ccid(uint8_t *imei_buffer, uint8_t *result, uint8_t max_lenth);

#endif /* GSM_UTILITIES_H */


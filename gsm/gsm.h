#ifndef GSM_H
#define GSM_H

/**
 * \defgroup        gsm GSM
 * \brief           All gsm function process here
 * \{
 */

#include <stdint.h>
#include <stdbool.h>

#define GSM_ON 1
#define GSM_OFF 2

#define GSMIMEI 0
#define SIMIMEI 1


#define GSM_ATC_BUFFER_SIZE             (256)   // AT command buffer size
#define GSM_PPP_MODEM_BUFFER_SIZE       1024    // PPP buffer size

typedef struct
{
    uint8_t buffer[GSM_ATC_BUFFER_SIZE];
    uint16_t index;
} gsm_atc_buffer_t;

typedef struct
{
    uint16_t idx_in;
    uint16_t idx_out;
    uint8_t buffer[GSM_PPP_MODEM_BUFFER_SIZE];
} gsm_modem_buffer_t;

typedef enum
{
    GSM_EVENT_OK = 0,  // GSM response ok
    GSM_EVENT_TIMEOUT, // Timeout is over
    GSM_EVENT_ERROR,   // GSM response wrong value
} gsm_response_event_t;

typedef enum
{
    GSM_STATE_OK = 0,
    GSM_STATE_RESET = 1,
    GSM_STATE_SEND_SMS = 2,
    GSM_STATE_READ_SMS = 3,
    GSM_STATE_POWER_ON = 4,
    GSM_STATE_REOPEN_PPP = 5,
    GSM_STATE_GET_BTS_INFO = 6,
//    GSM_STATE_SEND_ATC = 7,
    GSM_STATE_GOTO_SLEEP = 8,
    GSM_STATE_WAKEUP = 9,
    GSM_STATE_AT_MODE_IDLE,
    GSM_STATE_SLEEP,
    GSM_STATE_HTTP_GET,
    GSM_STATE_HTTP_POST,
    GSM_STATE_FILE_READ,
} gsm_state_t;

//typedef enum
//{
//    GSM_AT_MODE = 1,
//    GSM_PPP_MODE
//} gsm_at_mode_t;


typedef enum
{
    GSM_INTERNET_MODE_AT_STACK,
    GSM_INTERNET_MODE_PPP_STACK,
} gsm_internet_mode_t;

typedef struct
{
    gsm_state_t state;
    uint8_t step;
    uint8_t gsm_ready;
    gsm_internet_mode_t mode;
    uint8_t ppp_phase; // @ref lwip ppp.h
} gsm_manager_t;

typedef void (*gsm_send_at_cb_t)(gsm_response_event_t event, void *response_buffer);

typedef struct
{
    char *cmd;
    char *expect_resp_from_atc;
    char *expected_response_at_the_end;
    uint32_t timeout_atc_ms;
    uint32_t current_timeout_atc_ms;
    uint8_t retry_count_atc;
    gsm_atc_buffer_t recv_buff;
    gsm_send_at_cb_t send_at_callback;
} gsm_at_cmd_t;

typedef struct
{
    gsm_at_cmd_t atc;
} gsm_hardware_atc_t;

typedef void (*gsm_send_at_cb_t)(gsm_response_event_t event, void *ResponseBuffer);

void gsm_hw_send_at_cmd(char *cmd, char *expect_resp, char *expect_resp_at_the_end,
                        uint32_t timeout,uint8_t retry_count, gsm_send_at_cb_t callback);

/**
 * @brief           Initialize gsm hardware layer
 */
void gsm_init_hw(void);

/**
 * @brief           Initialize gsm data layer
 */
void gsm_data_layer_initialize(void);


void gsm_state_machine_polling(void);

/**
 * @brief           Initialize gsm data layer
 */
void gsm_hardware_tick(void);


/*!
 * @brief           Change GSM state 
 * @param[in]       new_state New gsm state
 */
void gsm_change_state(gsm_state_t new_state);


/*!
 * @brief       Send data directly to serial port
 * @param[in]   raw Raw data send to serial port
 * @param[in]   len Data length
 */
void gsm_hw_uart_send_raw(uint8_t *raw, uint32_t length);


/*!
 * @brief       Get internet mode
 * @retval      Internet mode
 */
gsm_internet_mode_t *gsm_get_internet_mode(void);

/*!
 * @brief       GSM hardware uart polling
 */
void gsm_hw_layer_run(void);

/*!
 * @brief       Change GSM hardware uart polling interval is ms
 * @param[in]   Polling interval in ms
 */
void gsm_change_hw_polling_interval(uint32_t ms);


/*!
 * @brief       Update current index of gsm dma rx buffer
 * @param[in]   rx_status RX status code
 *				TRUE : data is valid, FALSE data is invalid
 */
void gsm_uart_rx_dma_update_rx_index(bool rx_status);

/*!
 * @brief       UART transmit callback
 * @param[in]   status TX status code
 *				TRUE : transmit data is valid
 *				FALSE transmit data is invalid
 */
void gsm_uart_tx_complete_callback(bool status);

/*!
 * @brief		Get SIM IMEI
 * @retval		SIM IMEI
 */
char *gsm_get_sim_imei(void);

/*!
 * @brief		Get SIM CCID
 * @retval		SIM CCID pointer
 */
char *gsm_get_sim_ccid(void);

/*!
 * @brief		Get GSM IMEI
 * @retval		GSM IMEI
 */
char* gsm_get_module_imei(void);


/*!
 * @brief		Set SIM IMEI
 * @param[in]	SIM IMEI
 */
void gsm_set_sim_imei(char *imei);

/*!
 * @brief		Set GSM IMEI
 * @param[in]	GSM IMEI
 */
void gsm_set_module_imei(char *imei);

/*!
 * @brief		Set network operator
 * @param[in]	Network operator
 */
void gsm_set_network_operator(char *nw_operator);

/*!
 * @brief		Get network operator
 * @retval		Network operator
 */
char *gsm_get_network_operator(void);

/*!
 * @brief		Set GSM CSQ
 * @param[in]	CSQ GSM CSQ
 */
void gsm_set_csq(uint8_t csq);

/*!
 * @brief		Get GSM CSQ
 * @retval	 	GSM CSQ
 */
uint8_t gsm_get_csq(void);


/*!
 * @brief		Reset gsm rx buffer
 */
void gsm_hw_layer_reset_rx_buffer(void);

/*!
 * @brief		GSM mnr task
 */
void gsm_mnr_task(void *arg);

/*!
 * @brief		Copy data from serial RX to ppp buffer
 * @param[in]	data Buffer hold data
 * @param[in]   len Number of bytes to read
 * @retval      Number of byte availble
 */
uint32_t gsm_hardware_layer_copy_ppp_buffer(uint8_t *data, uint32_t len);

/*!
 * @brief		Check gsm is in data mode or cmd mode
 * @retval      TRUE : PPP mode
 *              FALSE : Command mode
 */
bool gsm_is_in_ppp_mode(void);

/*!
 * @brief		Get PPP connect status
 * @retval      TRUE : PPP connected
 *              FALSE : PPP not connected
 */
bool gsm_data_layer_is_ppp_connected(void);

/**
 * \}
 */

#endif // GSM_H

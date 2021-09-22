#include <stdio.h>
#include <string.h>
#include "gsm.h"
#include "app_debug.h"
#include "gsm_utilities.h"
#include "main.h"
#include "usart.h"
#include "app_debug.h"

#define UNLOCK_BAND     1
#define CUSD_ENABLE     0

static gsm_manager_t m_gsm_manager;

void gsm_config_module(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_data_layer_switch_mode_at_cmd(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_exit_sleep(gsm_response_event_t event, void *resp_buffer);
void gsm_power_on_sequence(void);

volatile uint32_t m_delay_wait_for_measurement_again_s = 0;
void gsm_state_machine_polling(void)
{
    /* GSM state machine */
    switch (m_gsm_manager.state)
    {
    case GSM_STATE_POWER_ON:
        if (m_gsm_manager.step == 0)
        {
            m_gsm_manager.step = 1;
            gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 30, gsm_config_module);
        }
        break;

    case GSM_STATE_OK:
        break;

    case GSM_STATE_RESET: /* Hard Reset */
        m_gsm_manager.gsm_ready = 0;
        gsm_power_on_sequence();
        break;

    default:
        DEBUG_ERROR("Unhandled case %u\r\n", m_gsm_manager.state);
        break;
    }
}

void gsm_data_layer_initialize(void)
{
//    m_gsm_manager.ri_signal = 0;
//    gsm_http_cleanup();
//    init_http_msq();
}

bool gsm_data_layer_is_module_sleeping(void)
{
    if (m_gsm_manager.state == GSM_STATE_SLEEP)
    {
        return 1;
    }
    return 0;
}

void gsm_change_state(gsm_state_t new_state)
{
    if (new_state == GSM_STATE_OK) // Command state -> Data state in PPP mode
    {
        m_gsm_manager.gsm_ready = 2;
    }
    DEBUG_INFO("Change GSM state to: ");
    switch ((uint8_t)new_state)
    {
    case GSM_STATE_OK:
        DEBUG_RAW("OK\r\n");
        break;
    case GSM_STATE_RESET:
        DEBUG_RAW("RESET\r\n");
        gsm_hw_layer_reset_rx_buffer();
        GSM_PWR_EN(0);      // Set GSM power en level to 0 = power off device
        GSM_PWR_RESET(1);   // Set GSM Reset pin to high
        GSM_PWR_KEY(0);     // Reset GSM power key pin
        break;
    
    case GSM_STATE_POWER_ON:
        DEBUG_RAW("POWERON\r\n");
        gsm_hw_layer_reset_rx_buffer(); // Reset USART RX buffer
        break;
//    case GSM_STATE_REOPEN_PPP:
//        DEBUG_VERBOSE("REOPENPPP\r\n");
//        break;
//    case GSM_STATE_GET_BTS_INFO:
//        DEBUG_VERBOSE("GETSIGNAL\r\n");
//        break;
//    case GSM_STATE_SEND_ATC:
//        DEBUG_RAW("Quit PPP and send AT command\r\n");
//        break;
    default:
        break;
    }
    m_gsm_manager.state = new_state;
    m_gsm_manager.step = 0;
}

#if UNLOCK_BAND
static uint8_t m_unlock_band_step = 0;
void do_unlock_band(gsm_response_event_t event, void *resp_buffer)
{
    switch(m_unlock_band_step)
    {
        case 0:
        {
                gsm_hw_send_at_cmd("AT+QCFG=\"nwscanseq\",3,1\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
        }
            break;
        
        case 1:
        {
                gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",3,1\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);			
        }
                break;
        
        case 2:
        {
            gsm_hw_send_at_cmd("AT+QCFG=\"band\",00,45\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
        }
            break;
        
        case 3:
        {
            DEBUG_INFO("Unlock band: %s\r\n",(event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
            m_unlock_band_step = 0;
            gsm_hw_send_at_cmd("AT+CPIN?\r\n", "+CPIN: READY\r\n", "", 1000, 10, gsm_config_module);
            break;
        }
        
        default:
            break;
    }
    m_unlock_band_step++;
}
#endif /* UNLOCK_BAND */

void gsm_config_module(gsm_response_event_t event, void *resp_buffer)
{
    switch (m_gsm_manager.step)
    {
    case 1:
        if (event != GSM_EVENT_OK)
        {
            DEBUG_ERROR("Connect modem ERR!\r\n");
            gsm_change_state(GSM_STATE_RESET);
        }
        else
        {
            gsm_hw_send_at_cmd("ATE0\r\n", "OK\r\n", "", 1000, 10, gsm_config_module);
        }
        break;

    case 2: /* Use AT+CMEE=2 to enable result code and use verbose values */
        DEBUG_INFO("Disable AT echo : %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMEE=2\r\n", "OK\r\n", "", 1000, 10, gsm_config_module);
        break;

    case 3:
        DEBUG_INFO("Set CMEE report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("ATI\r\n", "OK\r\n", "", 1000, 10, gsm_config_module);
        break;

    case 4:
        DEBUG_INFO("Get module info: %s\r\n", resp_buffer);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 10, gsm_config_module);
//        gsm_hw_send_at_cmd("AT+QURCCFG=\"URCPORT\",\"uart1\"\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
        break;

    case 5:
        DEBUG_INFO("Set URC port: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",2000\r\n", "OK\r\n", "", 1000, 10, gsm_config_module);
        break;

    case 6:
        DEBUG_INFO("Set URC ringtype: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CNMI=2,1,0,0,0\r\n", "", "OK\r\n", 1000, 10, gsm_config_module);
        break;
    
    case 7:
        DEBUG_INFO("Config SMS event report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "", "OK\r\n", 1000, 10, gsm_config_module);
        break;

    case 8:
        DEBUG_INFO("Set SMS text mode: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
        // gsm_hw_send_at_cmd("AT+CMGD=1,4\r\n", "	if (eeprom_cfg->io_enable.name.input_4_20ma_0_enable)OK\r\n", "", 2000, 5, gsm_config_module);
        //gsm_hw_send_at_cmd("AT+CNUM\r\n", "", "OK\r\n", 2000, 5, gsm_config_module);
        break;

    case 9:
        DEBUG_INFO("AT CNUM: %s, %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
										(char*)resp_buffer);
//        DEBUG_INFO("Delete all SMS: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGSN\r\n", "", "OK\r\n", 1000, 5, gsm_config_module);
        break;

    case 10:
	{
        DEBUG_INFO("CSGN resp: %s, Data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char*)resp_buffer);
		uint8_t *imei_buffer = (uint8_t*)gsm_get_module_imei();
        if (strlen((char*)imei_buffer) < 14)
        {
            gsm_utilities_get_imei(resp_buffer, (uint8_t *)imei_buffer, 16);
            DEBUG_INFO("Get GSM IMEI: %s\r\n", imei_buffer);
            imei_buffer = (uint8_t*)gsm_get_module_imei();
            if (strlen((char*)imei_buffer) < 15)
            {
                DEBUG_INFO("IMEI's invalid!\r\n");
                gsm_change_state(GSM_STATE_RESET); // We cant get GSM imei, maybe gsm module error =>> Restart module
                return;
            }
        }
        gsm_hw_send_at_cmd("AT+CIMI\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
	}
        break;

    case 11:
	{
		uint8_t *imei_buffer = (uint8_t*)gsm_get_sim_imei();
        gsm_utilities_get_imei(resp_buffer, imei_buffer, 16);
        DEBUG_INFO("Get SIM IMSI: %s\r\n", gsm_get_sim_imei());
        if (strlen(gsm_get_sim_imei()) < 15)
        {
            DEBUG_ERROR("SIM's not inserted!\r\n");
            m_gsm_manager.step = 10;
            gsm_hw_send_at_cmd("AT+CGSN\r\n", "", "OK\r\n", 1000, 5, gsm_config_module);
        }
        else
        {
            // Get SIM CCID
            gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_config_module);
        }
	}
        break;

    case 12:
    {
        DEBUG_INFO("Get SIM IMEI: %s\r\n", (char *)resp_buffer);
        gsm_change_hw_polling_interval(5);
		uint8_t *ccid_buffer = (uint8_t*)gsm_get_sim_ccid();
        if (strlen((char*)ccid_buffer) < 10)
        {
            gsm_utilities_get_sim_ccid(resp_buffer, ccid_buffer, 20);
        }
        DEBUG_INFO("SIM CCID: %s\r\n", ccid_buffer);
        static uint32_t retry = 0;
        if (strlen((char*)ccid_buffer) < 10 && retry < 2)
        {
            retry++;
            gsm_hw_send_at_cmd("AT+QCCID\r\n", "QCCID", "OK\r\n", 1000, 3, gsm_config_module); 
            m_gsm_manager.step = 11;
        }
        else
        {
            gsm_hw_send_at_cmd("AT+CPIN?\r\n", "READY\r\n", "", 3000, 3, gsm_config_module); 
        }
    }
        break;

    case 13:
        DEBUG_INFO("CPIN: %s\r\n", (char *)resp_buffer);
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT+QIDEACT=1\r\n", "OK\r\n", "", 3000, 1, gsm_config_module);
        break;
#if UNLOCK_BAND == 0        // Active band, availble on Quectel EC200
    case 14:
        DEBUG_INFO("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",0\r\n", "OK\r\n", "", 5000, 2, gsm_config_module); // Select mode AUTO
        break;
#else
    case 14:
        DEBUG_INFO("De-activate PDP: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        m_unlock_band_step = 0;
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
        break;
#endif
    case 15:
#if UNLOCK_BAND == 0
        DEBUG_INFO("Network search mode AUTO: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
#else
        DEBUG_INFO("Unlock band: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
#endif
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "", "OK\r\n", 1000, 2, gsm_config_module); /** <cid> = 1-24 */
        break;

    case 16:
        DEBUG_INFO("Define PDP context: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        //			gsm_hw_send_at_cmd("AT+QIACT=1\r\n", "OK\r\n", "", 5000, 5, gsm_config_module);	/** Turn on QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
        gsm_hw_send_at_cmd("AT+CSCS=\"GSM\"\r\n", "OK\r\n", "", 1000, 1, gsm_config_module);
        break;

    case 17:
        DEBUG_INFO("CSCS=GSM: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG=2\r\n", "OK\r\n", "", 1000, 3, gsm_config_module); /** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */
        break;

    case 18:
        DEBUG_INFO("Network registration status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char*)resp_buffer);
        gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
        break;

    case 19:
        DEBUG_INFO("Query network status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char*)resp_buffer); /** +CGREG: 2,1,"3279","487BD01",7 */
        gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 2000, 5, gsm_config_module);
        break;

    case 20:
        DEBUG_INFO("Query network operator: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +COPS: 0,0,"Viettel Viettel",7 */
        if (event == GSM_EVENT_OK)
        {
            // Get network operator : Viettel, Vina...
            gsm_utilities_get_network_operator(resp_buffer, 
                                                gsm_get_network_operator(), 
                                                32);
            if (strlen(gsm_get_network_operator()) < 5)
            {
                gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
                gsm_change_hw_polling_interval(1000);
                return;
            }
            DEBUG_INFO("Network operator: %s\r\n", gsm_get_network_operator());
        }
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
        break;

    case 21:
    {
//        DEBUG_INFO("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CCLK?\r\n", "+CCLK:", "OK\r\n", 1000, 5, gsm_config_module);
    }
    break;

    case 22:
    {
        // Get clock from module
        DEBUG_INFO("Query CCLK: %s,%s\r\n",
                     (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                     (char *)resp_buffer);
        // Get 4G signal strength
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "", "OK\r\n", 1000, 5, gsm_config_module);
    }
    break;

    case 23:
    {
        if (event != GSM_EVENT_OK)
        {
            DEBUG_INFO("GSM: init fail, reset modem...\r\n");
            m_gsm_manager.step = 0;
            gsm_change_state(GSM_STATE_RESET);
            return;
        }
		
		uint8_t csq = 99;
        // Parse signal strength response buffer
        gsm_utilities_get_signal_strength_from_buffer(resp_buffer, &csq);
        DEBUG_INFO("CSQ: %d\r\n", csq);

        if (csq == 99)      // Invalid CSQ =>> Polling CSQ again
        {
            m_gsm_manager.step = 21;
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
            gsm_change_hw_polling_interval(500);
        }
        else
        {
#if CUSD_ENABLE
            gsm_change_hw_polling_interval(100);
            gsm_hw_send_at_cmd("AT+CUSD=1,\"*101#\"\r\n", "+CUSD: ", "\r\n", 10000, 1, gsm_config_module);
#else
            gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 2000, 1, gsm_config_module);
#endif
        }
    }
        break;

        case 24:
#if CUSD_ENABLE
            if (event != GSM_EVENT_OK)
            {
                DEBUG_INFO("GSM: CUSD query failed\r\n");
            }
            else
            {
                char *p = strstr((char*)resp_buffer, "+CUSD: ");
                p += 5;
                DEBUG_INFO("CUSD %s\r\n", p);
                //Delayms(5000);
            }
#endif
            // GSM ready, switch to PPP mode
            gsm_change_hw_polling_interval(5);
            m_gsm_manager.gsm_ready = 1;
            m_gsm_manager.step = 0;
            gsm_change_state(GSM_STATE_OK);
        break;

        default:
            DEBUG_WARN("GSM unhandled step %u\r\n", m_gsm_manager.step);
            break;

    }
   
    m_gsm_manager.step++;
}


/*
* Reset module GSM
*/
void gsm_power_on_sequence(void)
{
    static uint8_t step = 0;
    DEBUG_INFO("GSM hard reset step %d\r\n", step);

    switch (step)
    {
    case 0: // Power off
        m_gsm_manager.gsm_ready = 0;
        GSM_PWR_EN(0);
        GSM_PWR_RESET(1);
        GSM_PWR_KEY(0);
        step++;
        break;
    
    case 1:
        GSM_PWR_RESET(0);
        DEBUG_INFO("Gsm power on\r\n");
        GSM_PWR_EN(1);
        step++;
        break;

    case 2:
        step++;
        break;

    case 3:
        GSM_UART_CONTROL(true);       // Enable MCU uart port
        DEBUG_INFO("Pulse power key\r\n");
        /* Generate pulse from (1-0-1) |_| to Power On module */
        GSM_PWR_KEY(1);     // set power key to 1, depend on hardware and module
        step++;
        break;

    case 4:
        GSM_PWR_KEY(0);
        GSM_PWR_RESET(0);
        step++;
        break;

    case 5:
    case 6:
        step++;
        break;
    
    case 7:
        step = 0;
        gsm_change_state(GSM_STATE_POWER_ON);       // GSM power seq finished, active gsm by at command
        break;
    
    default:
        break;
    }
}


void gsm_mnr_task(void *arg)
{
	gsm_state_machine_polling();
}

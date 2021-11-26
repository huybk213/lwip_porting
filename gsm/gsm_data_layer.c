#include <stdio.h>
#include <string.h>
#include "gsm.h"
#include "app_debug.h"
#include "gsm_utilities.h"
#include "main.h"
#include "usart.h"
#include "app_debug.h"
#include "ppp.h"
#include "pppos.h"
#include "lwip/init.h"
#include "dns.h"
#include "sio.h"
#include "sntp.h"

#define UNLOCK_BAND 0
#define CUSD_ENABLE 0

static gsm_manager_t m_gsm_manager;

// PPP
/* The PPP IP interface */
static struct netif m_ppp_netif;
/* The PPP control block */
static ppp_pcb *m_ppp_control_block;
static uint32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);
static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);
static void ppp_notify_phase_cb(ppp_pcb *pcb, u8_t phase, void *ctx);
static void open_ppp_stack(gsm_response_event_t event, void *response_buffer);
static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx);
static bool m_ppp_connected = false;

void gsm_config_module(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_read_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_send_sms(gsm_response_event_t event, void *resp_buffer);
void gsm_data_layer_switch_mode_at_cmd(gsm_response_event_t event, void *resp_buffer);
void gsm_at_cb_exit_sleep(gsm_response_event_t event, void *resp_buffer);
void gsm_power_on_sequence(void);

volatile uint32_t m_delay_wait_for_measurement_again_s = 0;
void gsm_state_machine_polling(void)
{
    static uint32_t m_last_tick = 0;
    uint32_t now = sys_get_tick_ms();
    if (now - m_last_tick >= (uint32_t)1000) // Poll every 1 second
    {
        m_last_tick = now;
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
    m_gsm_manager.state = new_state;
    m_gsm_manager.step = 0;
    DEBUG_INFO("Change GSM state to: ");
    switch ((uint8_t)new_state)
    {
    case GSM_STATE_OK:
        DEBUG_RAW("OK\r\n");
        break;
    case GSM_STATE_RESET:
        DEBUG_RAW("RESET\r\n");
        gsm_hw_layer_reset_rx_buffer();
        GSM_PWR_EN(0);    // Set GSM power en level to 0 = power off device
        GSM_PWR_RESET(1); // Set GSM Reset pin to high
        GSM_PWR_KEY(0);   // Reset GSM power key pin
        break;

    case GSM_STATE_POWER_ON:
        DEBUG_RAW("POWERON\r\n");
        gsm_hw_layer_reset_rx_buffer(); // Reset USART RX buffer
        break;
    
    case GSM_STATE_REOPEN_PPP:
        if (m_gsm_manager.step == 0)
        {
                m_gsm_manager.step = 1;
                m_ppp_connected = false;
                gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 5, open_ppp_stack);
        }
        break;
        
    default:
        break;
    }
}

#if UNLOCK_BAND
static uint8_t m_unlock_band_step = 0;
void do_unlock_band(gsm_response_event_t event, void *resp_buffer)
{
    switch (m_unlock_band_step)
    {
    case 0:
    {
        gsm_hw_send_at_cmd("AT+QCFG=\"nwscanseq\",3,1\r\n", "OK\r\n", "", 1000, 10, do_unlock_band);
    }
    break;

    case 1:
    {
        gsm_hw_send_at_cmd("AT+QCFG=\"nwscanmode\",3,1\r\n", "OK\r\n", "", 1000, 2, do_unlock_band);
    }
    break;

    case 2:
    {
        gsm_hw_send_at_cmd("AT+QCFG=\"band\",00,45\r\n", "OK\r\n", "", 1000, 2, do_unlock_band);
    }
    break;

    case 3:
    {
        DEBUG_INFO("Unlock band: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        m_unlock_band_step = 0;
        gsm_hw_send_at_cmd("AT+CPIN?\r\n", "+CPIN: READY\r\n", "", 1000, 2, gsm_config_module);
        break;
    }

    default:
        break;
    }
    m_unlock_band_step++;
}
#endif /* UNLOCK_BAND */

#ifndef MC60
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
                   (char *)resp_buffer);
        //        DEBUG_INFO("Delete all SMS: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGSN\r\n", "", "OK\r\n", 1000, 5, gsm_config_module);
        break;

    case 10:
    {
        DEBUG_INFO("CSGN resp: %s, Data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char *)resp_buffer);
        uint8_t *imei_buffer = (uint8_t *)gsm_get_module_imei();
        if (strlen((char *)imei_buffer) < 14)
        {
            gsm_utilities_get_imei(resp_buffer, (uint8_t *)imei_buffer, 16);
            DEBUG_INFO("Get GSM IMEI: %s\r\n", imei_buffer);
            imei_buffer = (uint8_t *)gsm_get_module_imei();
            if (strlen((char *)imei_buffer) < 15)
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
        uint8_t *imei_buffer = (uint8_t *)gsm_get_sim_imei();
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
        uint8_t *ccid_buffer = (uint8_t *)gsm_get_sim_ccid();
        if (strlen((char *)ccid_buffer) < 10)
        {
            gsm_utilities_get_sim_ccid(resp_buffer, ccid_buffer, 20);
        }
        DEBUG_INFO("SIM CCID: %s\r\n", ccid_buffer);
        static uint32_t retry = 0;
        if (strlen((char *)ccid_buffer) < 10 && retry < 2)
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
#if UNLOCK_BAND == 0 // Active band, availble on Quectel EC200
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
        			gsm_hw_send_at_cmd("AT+QIACT=1\r\n", "OK\r\n", "", 5000, 5, gsm_config_module);	/** Turn on QIACT lỗi gửi tin với 1 số SIM dùng gói cước trả sau! */
        //gsm_hw_send_at_cmd("AT+CSCS=\"GSM\"\r\n", "OK\r\n", "", 1000, 1, gsm_config_module);
        break;

    case 17:
        DEBUG_INFO("CSCS=GSM: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG=2\r\n", "OK\r\n", "", 1000, 3, gsm_config_module); /** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */
        break;

    case 18:
        DEBUG_INFO("Network registration status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char *)resp_buffer);
        gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 2, gsm_config_module);
        break;

    case 19:
        DEBUG_INFO("Query network status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char *)resp_buffer); /** +CGREG: 2,1,"3279","487BD01",7 */
        if (event == GSM_EVENT_OK)
        {
            bool retval;
            uint8_t access_tech = 0;
            retval = gsm_utilities_get_network_access_tech(resp_buffer, &access_tech);
            if (retval == false)
            {
                gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 2, gsm_config_module);
                gsm_change_hw_polling_interval(1000);
                return;
            }
        }
        gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 2000, 2, gsm_config_module);
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
                gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 1000, 3, gsm_config_module);
                gsm_change_hw_polling_interval(1000);
                return;
            }
            DEBUG_INFO("Network operator: %s\r\n", gsm_get_network_operator());
        }
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 3, gsm_config_module);
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
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "", "OK\r\n", 1000, 2, gsm_config_module);
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

        if (csq == 99) // Invalid CSQ =>> Polling CSQ again
        {
            m_gsm_manager.step = 21;
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 3, gsm_config_module);
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
            char *p = strstr((char *)resp_buffer, "+CUSD: ");
            p += 5;
            DEBUG_INFO("CUSD %s\r\n", p);
            //Delayms(5000);
        }
#endif
        // GSM ready, switch to PPP mode
        gsm_change_hw_polling_interval(5);
        m_gsm_manager.gsm_ready = 1;
        m_gsm_manager.step = 0;
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 2000, 1, open_ppp_stack);
        //            gsm_change_state(GSM_STATE_OK);
        break;

    default:
        DEBUG_WARN("GSM unhandled step %u\r\n", m_gsm_manager.step);
        break;
    }

    m_gsm_manager.step++;
}
#else
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
        gsm_hw_send_at_cmd("AT+CNMI=2,1,0,0,0\r\n", "", "OK\r\n", 1000, 10, gsm_config_module);
        break;

    case 6:
        DEBUG_INFO("Config SMS event report: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "", "OK\r\n", 1000, 10, gsm_config_module);
        break;

    case 7:
        DEBUG_INFO("Set SMS text mode: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+GSN\r\n", "", "OK\r\n", 1000, 5, gsm_config_module);
        break;

    case 8:
    {
        DEBUG_INFO("CSGN resp: %s, Data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char *)resp_buffer);
        uint8_t *imei_buffer = (uint8_t *)gsm_get_module_imei();
        if (strlen((char *)imei_buffer) < 14)
        {
            gsm_utilities_get_imei(resp_buffer, (uint8_t *)imei_buffer, 16);
            DEBUG_INFO("Get GSM IMEI: %s\r\n", imei_buffer);
            imei_buffer = (uint8_t *)gsm_get_module_imei();
            if (strlen((char *)imei_buffer) < 15)
            {
                DEBUG_INFO("IMEI's invalid!\r\n");
                gsm_change_state(GSM_STATE_RESET); // We cant get GSM imei, maybe gsm module error =>> Restart module
                return;
            }
        }
        gsm_hw_send_at_cmd("AT+CFUN=1\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
    }
    break;

    case 9:
    {
        gsm_hw_send_at_cmd("AT+CMGF=1\r\n", "OK\r\n", "", 1000, 5, gsm_config_module);
    }
    break;

    case 10:
        gsm_hw_send_at_cmd("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "", "OK\r\n", 1000, 5, gsm_config_module); /** <cid> = 1-24 */
        break;

    case 11:
        DEBUG_INFO("Set APN: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CGREG=1\r\n", "OK\r\n", "", 1000, 3, gsm_config_module); /** Query CGREG? => +CGREG: <n>,<stat>[,<lac>,<ci>[,<Act>]] */
    break;

    case 12:
        DEBUG_INFO("Network registration status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char *)resp_buffer);
        gsm_hw_send_at_cmd("AT+CGREG?\r\n", "OK\r\n", "", 1000, 2, gsm_config_module);
        break;

    case 13:
        DEBUG_INFO("Query network status: %s, data %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]", (char *)resp_buffer); /** +CGREG: 2,1,"3279","487BD01",7 */
        gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 2000, 2, gsm_config_module);
        break;

    case 14:
        DEBUG_INFO("Query network operator: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]"); /** +COPS: 0,0,"Viettel Viettel",7 */
        if (event == GSM_EVENT_OK)
        {
            // Get network operator : Viettel, Vina...
            gsm_utilities_get_network_operator(resp_buffer,
                                               gsm_get_network_operator(),
                                               32);
            if (strlen(gsm_get_network_operator()) < 5)
            {
                gsm_hw_send_at_cmd("AT+COPS?\r\n", "OK\r\n", "", 1000, 3, gsm_config_module);
                gsm_change_hw_polling_interval(1000);
                return;
            }
            DEBUG_INFO("Network operator: %s\r\n", gsm_get_network_operator());
        }
        gsm_change_hw_polling_interval(5);
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 1000, 3, gsm_config_module);
        break;

    case 15:
    {
        //        DEBUG_INFO("Select QSCLK: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
        gsm_hw_send_at_cmd("AT+CCLK?\r\n", "+CCLK:", "OK\r\n", 1000, 5, gsm_config_module);
    }
    break;

    case 16:
    {
        // Get clock from module
        DEBUG_INFO("Query CCLK: %s,%s\r\n",
                   (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]",
                   (char *)resp_buffer);
        // Get 4G signal strength
        gsm_hw_send_at_cmd("AT+CSQ\r\n", "", "OK\r\n", 1000, 2, gsm_config_module);
    }
    break;

    case 17:
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

        if (csq == 99) // Invalid CSQ =>> Polling CSQ again
        {
            m_gsm_manager.step = 16;
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 3, gsm_config_module);
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

    case 18:
#if CUSD_ENABLE
        if (event != GSM_EVENT_OK)
        {
            DEBUG_INFO("GSM: CUSD query failed\r\n");
        }
        else
        {
            char *p = strstr((char *)resp_buffer, "+CUSD: ");
            p += 5;
            DEBUG_INFO("CUSD %s\r\n", p);
            //Delayms(5000);
        }
#endif
        // GSM ready, switch to PPP mode
        gsm_change_hw_polling_interval(5);
        m_gsm_manager.gsm_ready = 1;
        m_gsm_manager.step = 0;
        gsm_hw_send_at_cmd("AT\r\n", "OK\r\n", "", 2000, 1, open_ppp_stack);
        //            gsm_change_state(GSM_STATE_OK);
        break;

    default:
        DEBUG_WARN("GSM unhandled step %u\r\n", m_gsm_manager.step);
        break;
    }

    m_gsm_manager.step++;
}

#endif /* Quectel MC60 */

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
        GSM_UART_CONTROL(true); // Enable MCU uart port
        DEBUG_INFO("Pulse power key\r\n");
        /* Generate pulse from (1-0-1) |_| to Power On module */
        GSM_PWR_KEY(1); // set power key to 1, depend on hardware and module
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
        gsm_change_state(GSM_STATE_POWER_ON); // GSM power seq finished, active gsm by at command
        break;

    default:
        break;
    }
}

static void open_ppp_stack(gsm_response_event_t event, void *response_buffer)
{
    DEBUG_INFO("Open PPP stack\r\n");
    static uint8_t retry_count = 0;

    switch (m_gsm_manager.step)
    {
        case 1:
        {
            gsm_hw_send_at_cmd("AT+CSQ\r\n", "OK\r\n", "", 1000, 2, open_ppp_stack);
            m_gsm_manager.step = 2;
        }
        break;

        case 2:
        {
            //Check SIM inserted, if removed -> RESET module NOW!
            gsm_hw_send_at_cmd("AT+CPIN?\r\n", "OK\r\n", "", 1000, 5, open_ppp_stack);
            m_gsm_manager.step = 3;
        }
        break;

        case 3:
        {
            if (event == GSM_EVENT_OK)
            {
                if (strstr(response_buffer, "+CPIN: NOT INSERTED"))
                {
                    DEBUG_INFO("Sim card not inserted\r\n");
                    gsm_change_state(GSM_STATE_RESET);
                    return;
                }
                gsm_hw_send_at_cmd("ATD*99***1#\r\n",
                                   "CONNECT",
                                   "",
                                   1000,
                                   10,
                                   open_ppp_stack);
                m_gsm_manager.step = 4;
            }
            else
            {
                DEBUG_ERROR("Open ppp stack failed\r\n");
                gsm_change_state(GSM_STATE_RESET);
                return;
            }
        }
        break;

        case 4:
        {
            DEBUG_INFO("PPP state: %s\r\n", (event == GSM_EVENT_OK) ? "[OK]" : "[FAIL]");
            m_gsm_manager.mode = GSM_INTERNET_MODE_PPP_STACK;

            if (event != GSM_EVENT_OK)
            {
                retry_count++;
                if (retry_count > 4)
                {
                    retry_count = 0;
                    ppp_close(m_ppp_control_block, 0);

                    /* Reset GSM */
                    gsm_change_state(GSM_STATE_RESET);
                }
                else
                {
                    m_gsm_manager.step = 3;
                    ppp_close(m_ppp_control_block, 0);
                    gsm_hw_send_at_cmd("ATV1\r\n", "OK\r\n", "", 1000, 5, open_ppp_stack);
                }
            }
            else
            {
                gsm_change_state(GSM_STATE_OK);

                //Create PPP connection
                m_ppp_control_block = pppos_create(&m_ppp_netif, ppp_output_callback, ppp_link_status_cb, NULL);
                if (m_ppp_control_block == NULL)
                {
                    DEBUG_ERROR("Create PPP interface ERR!\r\n");
                    //assert(0);
                    // TODO handle memory error
                    NVIC_SystemReset();
                }

                /* Set this interface as default route */
                ppp_set_default(m_ppp_control_block);
                //ppp_set_auth(m_ppp_control_block, PPPAUTHTYPE_CHAP, "", "");
                ppp_set_notify_phase_callback(m_ppp_control_block, ppp_notify_phase_cb);
                ppp_connect(m_ppp_control_block, 0);
            }
        }
        break;
    
        default:
            break;
    }
}

void *gsm_data_layer_get_ppp_control_block(void)
{
    return m_ppp_control_block;
}

static void initialize_stnp(void)
{
    static bool sntp_start = false;
    if (sntp_start == false)
    {
        sntp_start = true;
//        DEBUG_INFO("Initialize stnp\r\n");
//        sntp_setoperatingmode(SNTP_OPMODE_POLL);
//        sntp_setservername(0, "pool.ntp.org");
//        sntp_init();
    }
}

/**
 * PPP status callback
 * ===================
 *
 * PPP status callback is called on PPP status change (up, down, ...) from lwIP core thread
 */
static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx)
{
    struct netif *pppif = ppp_netif(pcb);
    LWIP_UNUSED_ARG(ctx);

    switch (err_code)
    {
    case PPPERR_NONE:
    {
#if LWIP_DNS
        const ip_addr_t *ns;
#endif /* LWIP_DNS */

        DEBUG_INFO("PPP Connected\r\n");

#if PPP_IPV4_SUPPORT

        DEBUG_INFO("\tour_ipaddr    = %s\r\n", ipaddr_ntoa(&pppif->ip_addr));
        DEBUG_INFO("\this_ipaddr    = %s\r\n", ipaddr_ntoa(&pppif->gw));
        DEBUG_INFO("\tnetmask       = %s\r\n", ipaddr_ntoa(&pppif->netmask));

#if LWIP_DNS
        ns = dns_getserver(0);
        DEBUG_INFO("\tdns1          = %s\r\n", ipaddr_ntoa(ns));
        ns = dns_getserver(1);
        DEBUG_INFO("\tdns2          = %s\r\n", ipaddr_ntoa(ns));
#endif /* LWIP_DNS */

#endif /* PPP_IPV4_SUPPORT */

#if PPP_IPV6_SUPPORT
        DEBUG_INFO("\r   our6_ipaddr = %s\n", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif /* PPP_IPV6_SUPPORT */
        break;
    }

    case PPPERR_PARAM:
    {
        DEBUG_INFO("status_cb: Invalid parameter\r\n");
        break;
    }
    case PPPERR_OPEN:
    {
        DEBUG_INFO("status_cb: Unable to open PPP session\r\n");
        break;
    }
    case PPPERR_DEVICE:
    {
        DEBUG_INFO("status_cb: Invalid I/O device for PPP\r\n");
        break;
    }
    case PPPERR_ALLOC:
    {
        DEBUG_INFO("status_cb: Unable to allocate resources\r\n");
        break;
    }
    case PPPERR_USER: /* 5 */
    {
        /* ppp_close() was previously called, reconnect */
        DEBUG_INFO("status_cb: ppp is closed by user OK! Try to re-open...\r\n");
        /* ppp_free(); -- can be called here */
        ppp_free(m_ppp_control_block);
        gsm_change_state(GSM_STATE_REOPEN_PPP);
        break;
    }
    case PPPERR_CONNECT: /* 6 */
    {
        DEBUG_INFO("status_cb: Connection lost\r\n");
        m_ppp_connected = false;
        ppp_close(m_ppp_control_block, 1);
        break;
    }
    case PPPERR_AUTHFAIL:
    {
        DEBUG_INFO("status_cb: Failed authentication challenge\r\n");
        break;
    }
    case PPPERR_PROTOCOL:
    {
        DEBUG_INFO("status_cb: Failed to meet protocol\n");
        break;
    }
    case PPPERR_PEERDEAD:
    {
        DEBUG_INFO("status_cb: Connection timeout\r\n");
        break;
    }
    case PPPERR_IDLETIMEOUT:
    {
        DEBUG_INFO("status_cb: Idle Timeout\r\n");
        break;
    }
    case PPPERR_CONNECTTIME:
    {
        DEBUG_INFO("status_cb: Max connect time reached\r\n");
        break;
    }
    case PPPERR_LOOPBACK:
    {
        DEBUG_INFO("status_cb: Loopback detected\r\n");
        break;
    }
    default:
    {
        DEBUG_INFO("status_cb: Unknown error code %d\r\n", err_code);
        break;
    }
    }

    /*
	* This should be in the switch case, this is put outside of the switch
	* case for example readability.
	*/

    if (err_code == PPPERR_NONE)
    {
        DEBUG_INFO("PPP is opened OK\r\n");
        initialize_stnp();
        return;
    }

    //  /* ppp_close() was previously called, don't reconnect */
    //  if (err_code == PPPERR_USER) {
    //    /* ppp_free(); -- can be called here */
    //	 m_ppp_connected = false;
    //	 ppp_free(m_ppp_control_block);
    //	 DEBUG_INFO("\r\nPPP opened ERR!\r\n");
    //    return;
    //  }

    /*
   * Try to reconnect in 30 seconds, if you need a modem chatscript you have
   * to do a much better signaling here ;-)
   */
    //  ppp_connect(pcb, 30);
    /* OR ppp_listen(pcb); */
}

static uint32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
{
    for (uint32_t i = 0; i < len; i++)
    {
        LL_USART_TransmitData8(USART1, data[i]);
        while (0 == LL_USART_IsActiveFlag_TC(USART1));
    }
    return len;
}

static void ppp_notify_phase_cb(ppp_pcb *pcb, u8_t phase, void *ctx)
{
    switch (phase)
    {
    /* Session is down (either permanently or briefly) */
    case PPP_PHASE_DEAD:
        DEBUG_INFO("PPP_PHASE_DEAD\r\n");
        m_gsm_manager.ppp_phase = PPP_PHASE_DEAD;
        break;

    /* We are between two sessions */
    case PPP_PHASE_HOLDOFF:
        DEBUG_INFO("PPP_PHASE_HOLDOFF\r\n");
        m_gsm_manager.ppp_phase = PPP_PHASE_HOLDOFF;
        break;

    /* Session just started */
    case PPP_PHASE_INITIALIZE:
        DEBUG_INFO("PPP_PHASE_INITIALIZE\r\n");
        m_gsm_manager.ppp_phase = PPP_PHASE_INITIALIZE;
        break;

    case PPP_PHASE_NETWORK:
        DEBUG_INFO("PPP_PHASE_NETWORK\r\n");
        break;

    case PPP_PHASE_ESTABLISH:
        DEBUG_INFO("PPP_PHASE_ESTABLISH\r\n");
        break;

    /* Session is running */
    case PPP_PHASE_RUNNING:
        DEBUG_INFO("PPP_PHASE_RUNNING\r\n");
        m_gsm_manager.ppp_phase = PPP_PHASE_RUNNING;
        m_ppp_connected = true;
        break;

    case PPP_PHASE_TERMINATE:
        DEBUG_INFO("PPP_PHASE_TERMINATE\r\n");
        break;

    case PPP_PHASE_DISCONNECT:
        DEBUG_INFO("PPP_PHASE_DISCONNECT\r\n");
        break;

    default:
        DEBUG_INFO("Unknown PPP phase %d\r\n", phase);
        break;
    }
}

void lwip_sntp_recv_callback(uint32_t time)
{
    if (time == 0)
    {
        DEBUG_WARN("NTP: Error, server not responding or bad response\r\n");
    }
    else
    {
        DEBUG_INFO("NTP: %u seconds elapsed since 1.1.1970\r\n", time);
    }
}

uint32_t sio_read(sio_fd_t fd, u8_t *data, u32_t len)
{
    return gsm_hardware_layer_copy_ppp_buffer(data, len);
}

static uint8_t m_ppp_rx_buffer[512];
void gsm_hw_pppos_polling(void)
{
    uint32_t sio_size;

    sys_check_timeouts();

    sio_size = sio_read(0, m_ppp_rx_buffer, 512);
    if (sio_size > 0)
    {
        // Bypass data into ppp stack
        pppos_input(gsm_data_layer_get_ppp_control_block(), m_ppp_rx_buffer, sio_size);
    }
}

bool gsm_is_in_ppp_mode(void)
{
    return m_gsm_manager.mode == GSM_INTERNET_MODE_PPP_STACK ? true : false;
}

bool gsm_data_layer_is_ppp_connected(void)
{
    return m_ppp_connected;
}

void gsm_mnr_task(void *arg)
{
    gsm_hw_layer_run();
    gsm_state_machine_polling();
    gsm_hw_pppos_polling();
}

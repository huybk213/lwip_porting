#include <stdio.h>
#include <string.h>
#include "app_debug.h"
#include "lwip/pbuf.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "gsm.h"
#include "mqtt_client.h"
#include "main.h"

#define MQTT_KEEP_ALIVE_INTERVAL 600
#define MQTT_TOPIC_MAX_NAME_LENGTH 48

#define	TOPIC_PUB_HEADER	"lwip_porting/pub/"
#define	TOPIC_SUB_HEADER	"lwip_porting/sub/"


static char m_mqtt_publish_topic_name[MQTT_TOPIC_MAX_NAME_LENGTH];
static char m_mqtt_subscribe_topic_name[MQTT_TOPIC_MAX_NAME_LENGTH];
static char m_mqtt_tx_buffer[256];

static ip_addr_t m_mqtt_server_address;
static mqtt_client_t m_mqtt_static_client;
static uint8_t m_is_valid_sub_topic;
static uint8_t m_is_dns_resolved = 0;
static uint8_t m_sub_req_err_count;

static mqtt_client_cfg_t m_cfg;
static mqtt_client_state_t m_mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
static bool m_mqtt_process_now = false;

static void mqtt_sub_request_cb(void *arg, err_t result)
{
    /* Just print the result code here for simplicity, 
        normal behaviour would be to take some action if subscribe fails like 
        notifying user, retry subscribe or disconnect from server 
    */
    if (result != ERR_OK)
    {
        DEBUG_INFO("Retry send subscribe...\r\n");
        m_sub_req_err_count++;
        if (m_sub_req_err_count >= 5)
        {
            /* close mqtt connection */
            mqtt_disconnect(&m_mqtt_static_client);

            m_sub_req_err_count = 0;
            m_mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
        }
        else
        {
            err_t err = mqtt_subscribe(&m_mqtt_static_client,
                                       m_mqtt_subscribe_topic_name,
                                       SUB_QoS,
                                       mqtt_sub_request_cb,
                                       arg);
            if (err != ERR_OK)
            {
                DEBUG_INFO("mqtt_subscribe error %d\r\n", err);
            }
        }
    }
    else
    {
        m_sub_req_err_count = 0;
        m_mqtt_process_now = true;
        DEBUG_INFO("Subscribed\r\n");
    }
}

/* 3. Implementing callbacks for incoming publish and data */
/* The idea is to demultiplex topic and create some reference to be used in data callbacks
   Example here uses a global variable, better would be to use a member in arg
   If RAM and CPU budget allows it, the easiest implementation might be to just take a copy of
   the topic string and use it in mqtt_incoming_data_cb
*/
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    DEBUG_INFO("MQTT publish cb topic %s, length %u\r\n", topic, (unsigned int)tot_len);

    /* Decode topic string into a user defined reference */
    if (strcmp(topic, m_mqtt_subscribe_topic_name) == 0)
    {
        m_is_valid_sub_topic = 1;
    }
    else
    {
        /* For all other topics */
        m_is_valid_sub_topic = 0;
    }
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    DEBUG_INFO("MQTT data cb, length %d, flags %u\r\n", len, (unsigned int)flags);

    if (flags & MQTT_DATA_FLAG_LAST)
    {
        /* Last fragment of payload received (or whole part if payload fits receive buffer
              See MQTT_VAR_HEADER_BUFFER_LEN)  */

        DEBUG_INFO("Payload data: %s\r\n", (const char *)data);

        if (m_is_valid_sub_topic == 1)
        {
            m_is_valid_sub_topic = 0;

            /* Update firmware message  */
            if (strstr((char *)data, "UDFW,"))
            {
                DEBUG_INFO("Update firmware\r\n");
            }
            else if (strstr((char *)data, "PLEASE RESET"))
            {
                NVIC_SystemReset();
            }
        }

        // Clear received buffer of client -> du lieu nhan lan sau khong bi thua cua lan truoc,
        // neu lan truoc gui length > MQTT_VAR_HEADER_BUFFER_LEN
        memset(m_mqtt_static_client.rx_buffer, 0, MQTT_VAR_HEADER_BUFFER_LEN);
    }
    else
    {
        /* Handle fragmented payload, store in buffer, write to file or whatever */
    }
}

static void mqtt_client_connection_callback(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    DEBUG_INFO("mqtt_client_connection_callback reason: %d\r\n", status);

    err_t err;
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        DEBUG_INFO("mqtt_connection_cb: Successfully connected\r\n");
        m_mqtt_process_now = true;
        m_mqtt_state = MQTT_CLIENT_STATE_CONNECTED;

        /* Setup callback for incoming publish requests */
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);

        /* Subscribe to a topic named "fire/sub/IMEI" with QoS level 1, 
            call mqtt_sub_request_cb with result */
        DEBUG_INFO("Subscribe %s\r\n", m_mqtt_subscribe_topic_name);
        err = mqtt_subscribe(client, m_mqtt_subscribe_topic_name, SUB_QoS, mqtt_sub_request_cb, arg);

        if (err != ERR_OK)
        {
            DEBUG_INFO("mqtt_subscribe return: %d\r\n", err);
        }
    }
    else
    {
        DEBUG_INFO("[%s] mqtt connection status %d\r\n", __FUNCTION__);
        mqtt_disconnect(&m_mqtt_static_client);
        /* Its more nice to be connected, so try to reconnect */
        m_mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
    }
}

/* -----------------------------------------------------------------
4. Using outgoing publish
*/
/* Called when publish is complete either with sucess or failure */
static void mqtt_pub_request_cb(void *arg, err_t result)
{
    if (result != ERR_OK)
    {
        DEBUG_INFO("Publish result: %d\r\n", result);
    }
    else
    {
        DEBUG_INFO("Publish: OK\r\n");
    }
}


static void mqtt_client_send_heart_beat(void)
{
   snprintf(m_mqtt_tx_buffer, sizeof(m_mqtt_tx_buffer), "%s", "Xin chao cac ban\r\n");

    err_t err = mqtt_publish(&m_mqtt_static_client,
                             m_mqtt_publish_topic_name,
                             m_mqtt_tx_buffer,
                             strlen(m_mqtt_tx_buffer),
                             PUB_QoS,
                             PUB_RETAIN,
                             mqtt_pub_request_cb,
                             NULL);
    if (err != ERR_OK)
    {
        DEBUG_INFO("Publish err: %d\r\n", err);
        return;
    }
}

static void mqtt_client_send_subscribe_req(void)
{
    /* Subscribe to a topic named "qrm/imei/st_data" with QoS level 1, call mqtt_sub_request_cb with result */
    err_t err = mqtt_subscribe(&m_mqtt_static_client, m_mqtt_subscribe_topic_name, SUB_QoS, mqtt_sub_request_cb, NULL);

    DEBUG_INFO("%s: topic %s\r\n", __FUNCTION__, m_mqtt_subscribe_topic_name);
}

static void heart_beat_msg_tick(void)
{
    mqtt_client_send_heart_beat();
}

static int8_t mqtt_connect_broker(mqtt_client_t *client)
{
    struct mqtt_connect_client_info_t client_info =
        {
            m_cfg.client_id,
            NULL, NULL,               //User, pass
            MQTT_KEEP_ALIVE_INTERVAL, //Keep alive in seconds, 0 - disable
            NULL, NULL, 0, 0          //Will topic, will msg, will QoS, will retain
        };

    /* Minimal amount of information required is client identifier, so set it here */
    client_info.client_user = m_cfg.user_name;
    client_info.client_pass = m_cfg.password;

    /* 
    * Initiate client and connect to server, if this fails immediately an error code is returned
    * otherwise mqtt_connection_cb will be called with connection result after attempting 
    * to establish a connection with the server. 
    * For now MQTT version 3.1.1 is always used 
    */
    err_t err = mqtt_client_connect(client,
                                    &m_mqtt_server_address,
                                    m_cfg.port,
                                    mqtt_client_connection_callback,
                                    0,
                                    &client_info);

    /* For now just print the result code if something goes wrong */
    if (err != ERR_OK)
    {
        DEBUG_INFO("mqtt_connect return %d\r\n", err);
        if (err == ERR_ISCONN)
        {
            DEBUG_INFO("MQTT already connected\r\n");
        }
    }
    else
    {
        DEBUG_INFO("Host %s:%d %s, client id %s\r\n",
                   m_cfg.broker_addr,
                   m_cfg.port,
                   ipaddr_ntoa(&m_mqtt_server_address),
                   m_cfg.client_id);
        DEBUG_INFO("mqtt_client_connect: OK\r\n");
    }

    return err;
}

/**
 * @brief DNS found callback when using DNS names as server address.
 */
static void mqtt_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    DEBUG_INFO("mqtt_dns_found: %s\r\n", hostname);

    LWIP_UNUSED_ARG(hostname);
    LWIP_UNUSED_ARG(arg);

    if (ipaddr != NULL)
    {
        /* Address resolved, send request */
        m_mqtt_server_address.addr = ipaddr->addr;
        DEBUG_INFO("Server address resolved = %s\r\n", ipaddr_ntoa(&m_mqtt_server_address));
        m_is_dns_resolved = 1;
        m_mqtt_process_now = true;
    }
    else
    {
        /* DNS resolving failed -> try another server */
        DEBUG_INFO("mqtt_dns_found: Failed to resolve server address resolved, trying next server\r\n");
        m_is_dns_resolved = 0;
    }
}

/**
 * @note Task call every 1s
 */
void mqtt_client_polling_task(void *arg)
{
    static uint8_t mqtt_tick = 0;
    static uint32_t tick = 0, last_time_send_sub_request = 0, last_time_send_hearbeat = 0;
    static uint32_t m_current_tick;
    uint32_t now = sys_get_tick_ms();

    if ((now - m_current_tick >= (uint32_t)1000) || m_mqtt_process_now)
    {
        m_mqtt_process_now = false;
        m_current_tick = now;
    }
    else
    {
        return;
    }

    static bool m_mqtt_allow = false;
    if (gsm_data_layer_is_ppp_connected())
    {
        if (m_mqtt_allow == false)
        {
            m_mqtt_allow = true;
            if (mqtt_client_is_connected(&m_mqtt_static_client))
            {
                mqtt_disconnect(&m_mqtt_static_client);
                m_mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
            }

            DEBUG_INFO("MQTT thread started\r\n");
        }
    }
    else
    {
        if (m_mqtt_allow)
        {
            DEBUG_INFO("MQTT thread stopped\r\n");
        }
        m_mqtt_allow = false;
    }

    if (m_mqtt_allow)
    {
        mqtt_tick++;
        switch (m_mqtt_state)
        {
        case MQTT_CLIENT_STATE_DISCONNECTED:
            /* init client info...*/
            m_is_dns_resolved = 0;
            m_mqtt_state = MQTT_CLIENT_STATE_RESOLVE_HOSTNAME;
            mqtt_tick = 4;
            break;

        case MQTT_CLIENT_STATE_RESOLVE_HOSTNAME:
            if (!m_is_dns_resolved)
            {
                if (mqtt_tick >= 5)
                {
                    mqtt_tick = 0;

                    err_t err = dns_gethostbyname(m_cfg.broker_addr,
                                                  &m_mqtt_server_address,
                                                  mqtt_dns_found,
                                                  NULL);

                    if (err == ERR_INPROGRESS)
                    {
                        /* DNS request sent, wait for sntp_dns_found being called */
                        DEBUG_INFO("sntp_request: %d - Waiting for server address to be resolved\r\n", err);
                    }
                    else if (err == ERR_OK)
                    {
                        DEBUG_INFO("DNS resolved aready, host %s, mqtt_ipaddr = %s\r\n",
                                   m_cfg.broker_addr,
                                   ipaddr_ntoa(&m_mqtt_server_address));
                        m_is_dns_resolved = 1;
                    }
                }
            }
            else
            {
                mqtt_tick = 9;
                m_mqtt_state = MQTT_CLIENT_STATE_CONNECTING;
            }
            break;

        case MQTT_CLIENT_STATE_CONNECTING:
            if (mqtt_tick >= 10)
            {
                if (mqtt_connect_broker(&m_mqtt_static_client) == ERR_OK)
                {
                    mqtt_tick = 5; /* Gui login sau 5s */
                }
                else
                {
                    mqtt_tick = 0;
                }
            }
            break;

        case MQTT_CLIENT_STATE_CONNECTED:
        {
            tick = now;

            if (last_time_send_sub_request == 0 || last_time_send_sub_request > tick)
                last_time_send_sub_request = tick;

            if (mqtt_client_is_connected(&m_mqtt_static_client))
            {
                /* Send subscribe message periodic */
                if (tick >= (last_time_send_sub_request + m_cfg.periodic_sub_req_s))
                {
                    last_time_send_sub_request = tick;
                    mqtt_client_send_subscribe_req();
                }
                
                if (tick - last_time_send_hearbeat >= (uint32_t)30000)
                {
                    last_time_send_hearbeat = tick;
                    /* Send heartbeat message */
                    heart_beat_msg_tick();
                }
            }
            else
            {
                m_mqtt_state = MQTT_CLIENT_STATE_DISCONNECTED;
            }
        }
        break;
        default:
            break;
        }
    }
    else
    {
        mqtt_tick = 4;
    }
}

bool mqtt_client_is_connected_to_broker(void)
{
    if (m_mqtt_state == MQTT_CLIENT_STATE_CONNECTED)
        return true;

    return false;
}

void mqtt_client_initialize(mqtt_client_cfg_t *cfg)
{
    memcpy(&m_cfg, cfg, sizeof(mqtt_client_cfg_t));
    snprintf(m_mqtt_subscribe_topic_name, sizeof(m_mqtt_subscribe_topic_name), "%s", "test_pub_porting_lwip_stm32");
    snprintf(m_mqtt_publish_topic_name, sizeof(m_mqtt_publish_topic_name), "%s", "test_sub_porting_lwip_stm32");
}

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

/* QoS value, 0 - at most once, 1 - at least once or 2 - exactly once */
#define SUB_QoS	1
#define PUB_QoS	1 		
#define PUB_RETAIN	0

typedef struct
{
    uint32_t periodic_sub_req_s;            // second
    const char *broker_addr;
    uint16_t port;
    const char *client_id;
    const char *user_name;
    const char *password;
} mqtt_client_cfg_t;

typedef enum 
{
    MQTT_CLIENT_STATE_DISCONNECTED = 0,
    MQTT_CLIENT_STATE_RESOLVE_HOSTNAME,
    MQTT_CLIENT_STATE_CONNECTING,
    MQTT_CLIENT_STATE_CONNECTED
} mqtt_client_state_t;


/**
 * @brief           MQTT client initialize
 * @param[in]       MQTT config parameter
 */
void mqtt_client_initialize(mqtt_client_cfg_t *cfg);

/**
 * @brief Polling MQTT service
 */
void mqtt_client_polling_task(void *arg);

/**
 * @brief Get MQTT connect status
 * @retval TRUE Mqtt connected
 *         False MQTT not connected
 */
bool mqtt_client_is_connected_to_broker(void);

#endif // MQTT_CLIENT_H


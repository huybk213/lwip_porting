#ifndef APP_HTTP_H
#define APP_HTTP_H

#include <stdint.h>
#include <stdbool.h>

#define APP_HTTP_MAX_URL_SIZE 256

typedef struct
{
    uint32_t length;
    uint8_t * data;
} app_http_data_t;


typedef enum
{
    APP_HTTP_EVENT_START,
    APP_HTTP_EVENT_CONNTECTED,
    APP_HTTP_EVENT_DATA,
    APP_HTTP_EVENT_FINISH_SUCCESS,
    APP_HTTP_EVENT_FINISH_FAILED
} app_http_event_t;

typedef void (*app_http_event_cb_t)(app_http_event_t event, void * data);


typedef struct
{
    app_http_event_cb_t on_event_cb;
    char url[APP_HTTP_MAX_URL_SIZE];
    char file[APP_HTTP_MAX_URL_SIZE];
    uint16_t port;
} app_http_config_t;


/**
 * @brief       Get max url size allowed
 * @retval      Max url length
 */
static inline uint32_t app_http_get_max_url_size(void)
{
    return APP_HTTP_MAX_URL_SIZE - 1;       // reserve 1 byte for null terminal
}

/**
 * @brief       Get current app_ http configuration
 * @retval      Pointer to current app_ configuration
 */
app_http_config_t *app_http_get_config(void);

/**
 * @brief       Clean up current configuration
 */
void app_http_cleanup(void);

/**
 * @brief       Start app_ http download
 * @param[in]   config : HTTP configuration
 * @retval      TRUE : Operation success
 *              FALSE : Operation failed
 */
bool app_http_start(app_http_config_t * config);

/**
 * @brief       Check http state 
 * @retval      TRUE http is idle
 *              FALSE other http task is running
 */
bool app_http_is_idle(void);

#endif /* APP_HTTP_H */



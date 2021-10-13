#include "app_http.h"
#include "app_debug.h"

#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/apps/http_client.h"
#include "lwip/dns.h"
#include "lwip/debug.h"
#include "lwip/mem.h"
#include "lwip/init.h"
#include "string.h"

#define HTTP_DOWNLOAD_BUFFER_SIZE 1024

static app_http_config_t m_http_cfg;
static uint32_t m_total_bytes_recv = 0;
static uint32_t m_content_length = 0;
static httpc_connection_t m_conn_settings_try;
static err_t httpc_file_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static httpc_state_t *m_http_connection_state;

app_http_config_t *app_http_get_config(void)
{
    return &m_http_cfg;
}

void app_http_cleanup(void)
{
    m_total_bytes_recv = 0;
    m_content_length = 0;
    memset(&m_http_cfg, 0, sizeof(m_http_cfg));
}

bool m_http_free = true;
bool app_http_is_idle(void)
{
    return m_http_free;
}
/**
 * @brief Result transfer done callback
 */
static void httpc_result_callback(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
    DEBUG_INFO("result: %d, content len: %d, status code: %d\r\n", httpc_result, rx_content_len, srv_res);
    switch (err)
    {
        case HTTPC_RESULT_OK: /** File successfully received */
        {
            DEBUG_INFO("HTTPC_RESULT_OK\r\n");
            bool status = false;

            if (m_content_length && (m_content_length == m_total_bytes_recv))
            {
                status = true;
            }

            if (status)
            {
                if (m_http_cfg.on_event_cb)
                    m_http_cfg.on_event_cb(APP_HTTP_EVENT_FINISH_SUCCESS, &m_total_bytes_recv);
            }
            else
            {
                if (m_http_cfg.on_event_cb)
                        m_http_cfg.on_event_cb(APP_HTTP_EVENT_FINISH_FAILED, &m_total_bytes_recv);
            }
        }
            break;

        case HTTPC_RESULT_ERR_UNKNOWN:     /** Unknown error */
                                           //break;
        case HTTPC_RESULT_ERR_CONNECT:     /** Connection to server failed */
                                           //break;
        case HTTPC_RESULT_ERR_HOSTNAME:    /** Failed to resolve server hostname */
                                           //break;
        case HTTPC_RESULT_ERR_CLOSED:      /** Connection unexpectedly closed by remote server */
                                           //break;
        case HTTPC_RESULT_ERR_TIMEOUT:     /** Connection timed out (server didn't respond in time) */
                                           //break;
        case HTTPC_RESULT_ERR_SVR_RESP:    /** Server responded with an error code */
                                           //break;
        case HTTPC_RESULT_ERR_MEM:         /** Local memory error */
                                           //break;
        case HTTPC_RESULT_LOCAL_ABORT:     /** Local abort */
                                           //break;
        case HTTPC_RESULT_ERR_CONTENT_LEN: /** Content length mismatch */
            DEBUG_ERROR("Error content length\r\n");
            if (m_http_cfg.on_event_cb)
                m_http_cfg.on_event_cb(APP_HTTP_EVENT_FINISH_FAILED, &m_total_bytes_recv);
            break;

        default:
            DEBUG_INFO("httpc_result_callback error %d\r\n", err);
            break;
    }
    
    m_http_free = true;
}

/**
 * @brief Header received done callback
 */
err_t httpc_headers_done_callback(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
    DEBUG_INFO("httpc_headers_callback, content length %d\r\n", content_len);
    
    m_content_length = content_len;
    if (content_len == 0xFFFFFFFF)
    {
        DEBUG_INFO("Invalid content length\r\n");
        if (m_http_cfg.on_event_cb)
            m_http_cfg.on_event_cb(APP_HTTP_EVENT_FINISH_FAILED, &m_content_length);
    }
    else
    {
        if (m_http_cfg.on_event_cb)
            m_http_cfg.on_event_cb(APP_HTTP_EVENT_CONNTECTED, &m_content_length);
    }

    return ERR_OK;
}

/** 
 * @brief Handle data connection incoming data
 * @param pointer to lwftp session data
 * @param pointer to PCB
 * @param pointer to incoming pbuf
 * @param state of incoming process
 */
uint32_t last_tick = 0;
static err_t httpc_file_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    //DEBUG_INFO("lwip http event cb\r\n");
    if (p)
    {
        struct pbuf *q;
        for (q = p; q; q = q->next)
        {
            DEBUG_INFO("HTTP data %.*s\r\n", q->len, q->payload); 
        }
        // MUST used 2 commands!
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
    }
    else
    {
        DEBUG_WARN("tcp_close\r\n");
        tcp_close(tpcb);
        return ERR_ABRT;
    }

    return ERR_OK;
}

bool app_http_start(app_http_config_t *config)
{
    // ASSERT(config);

    app_http_cleanup();
    sprintf(m_http_cfg.url, "%s", config->url);
    m_http_cfg.port = config->port;
    sprintf(m_http_cfg.file, "%s", config->file);
    m_http_cfg.on_event_cb = (void*)0;


    /* Init Http connection params */
    m_conn_settings_try.use_proxy = 0;
    m_conn_settings_try.headers_done_fn = httpc_headers_done_callback;
    m_conn_settings_try.result_fn = httpc_result_callback;
    
    DEBUG_INFO("HTTP url %s%s, port %d\r\n", m_http_cfg.url, m_http_cfg.file, m_http_cfg.port);
    err_t error = httpc_get_file_dns((const char*)m_http_cfg.url, 
                                    m_http_cfg.port, 
                                    m_http_cfg.file, 
                                    &m_conn_settings_try, 
                                    httpc_file_recv_callback, 
                                    NULL, 
                                    &m_http_connection_state);
    
    m_conn_settings_try.headers_done_fn = httpc_headers_done_callback;
    m_conn_settings_try.result_fn = httpc_result_callback;
    
    if (error != ERR_OK)
    {
        DEBUG_INFO("Cannot connect HTTP server, error %d\r\n", error);
        return false;
    }
    
    if (m_http_cfg.on_event_cb)
        m_http_cfg.on_event_cb(APP_HTTP_EVENT_START, (void*)0);
    m_http_free = false;
    return true;
}


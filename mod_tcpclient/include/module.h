#ifndef __MODULE_H
#define __MODULE_H

#define KEY_SIZE 256
#define SL_RCVFIFO "Fifo_ingress"

typedef struct _config_endpoints
{
    int port;
    char ip[16];
} end_peers;

typedef struct _config_params
{
    int num_worker_threads;
    int num_peers;
    end_peers *peers;
} configurator;


/* Structure to hold the request */
typedef struct __XMLRequest
{
        char *req_buf;
        int msg_len;
        int picked_up;
        int no_transaction;
        char id[KEY_SIZE];
}request_t;

/* Structure to hold the response */
typedef struct __XMLResponse
{
        char id[KEY_SIZE];
        int retcode;
        char *query_res;
}response_t;


/* Initialize the module by passing configuration object */
int mod_init(configurator cfg);

/* Get total number of peers */
int get_peers();

/* Function pointer which takes arguments: 
 * is_timeout: 0; no timeout
 * is_timeout: 1; timeout occurred
 * void pointer to param blob
 * response_t/request_t pointer pointing to response /request structure respectively
 * time difference in milliseconds
 */
typedef void (TransactionCallback_Res_f)(int is_timeout, void *param, response_t *res, long elapsed_msecs);
typedef void (TransactionCallback_Req_f)(int is_timeout, void *param, request_t *req, long elapsed_msecs);

/* Function to be called by the IML by passing the following arguments:
 * Function pointer to TransactionCallback_Res_f
 * void pointer to callback param blob
 */
int register_resp_cb(TransactionCallback_Res_f *callback_f,void *callback_param);

/* Function to be called by the IML for receiving the requests from Peers/Kamailio(s) by passing the following arguments:
 * Function pointer to TransactionCallback_Req_f
 * void pointer to callback param blob
 */
int register_req_cb(TransactionCallback_Req_f *callback_f,void *callback_param);
#endif

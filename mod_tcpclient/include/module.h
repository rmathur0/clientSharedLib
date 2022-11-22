#ifndef __MODULE_H
#define __MODULE_H


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



/* Initialize the module by passing configuration object */
int mod_init(configurator cfg);

/* Get total number of peers */
int get_peers();

#endif

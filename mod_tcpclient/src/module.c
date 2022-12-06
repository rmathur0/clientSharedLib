#include "../include/headers.h"
#include "../include/module.h"
#include "../include/work.h"

configurator gconfig;

int mod_init(configurator cfg)
{
    int i = 0, rc = -1;
    gconfig = cfg;
    openlog("IML_SharedLib", LOG_PID, LOG_USER);
    syslog (LOG_INFO,"\n\n Inside Shared Lib\n"); 
    //end_peers *peers
    gconfig.peers = (end_peers*)malloc(sizeof(end_peers)*cfg.num_peers);
    for (i = 0; i < cfg.num_peers; i++)
    {
        //memcpy(gconfig.peers[i], cfg.peers[i], sizeof(end_peers));
        strcpy(gconfig.peers[i].ip, cfg.peers[i].ip);
        gconfig.peers[i].port = cfg.peers[i].port;
    }
    syslog (LOG_INFO,"\nNumber of worker threads:%d\n", gconfig.num_worker_threads);
    for (i = 0; i < gconfig.num_peers; i++)
    {
        syslog (LOG_INFO,"\nPeer[%d]\tIP[%s]:[%d]\n", i+1, gconfig.peers[i].ip, gconfig.peers[i].port);
    }
    rc = manage(&gconfig); 
    if (rc != 0)
    {
        syslog(LOG_INFO,"\nUnable to create threads. Exiting.\n");
        exit (1);
    }
    syslog (LOG_INFO,"\nThreads spawned.\n");
    return 0;    
}

int get_peers()
{
    return gconfig.num_peers;
}


int register_req_cb(TransactionCallback_Req_f *callback_f)
{
	return (register_callback_request(callback_f));
}

int sl_send_buf(request_t *req, TransactionCallback_Res_f *callback_f, void *callback_param)
{
	return (ready_to_send(req, callback_f, callback_param));
}

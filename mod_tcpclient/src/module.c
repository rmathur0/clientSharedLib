#include "../include/headers.h"
#include "../include/module.h"
#include "../include/work.h"

configurator gconfig;

int mod_init(configurator cfg)
{
    int i = 0, rc = -1;
    gconfig = cfg;
    printf ("\n\n Inside Shared Lib\n"); 
    //end_peers *peers
    gconfig.peers = (end_peers*)malloc(sizeof(end_peers)*cfg.num_peers);
    for (i = 0; i < cfg.num_peers; i++)
    {
        //memcpy(gconfig.peers[i], cfg.peers[i], sizeof(end_peers));
        strcpy(gconfig.peers[i].ip, cfg.peers[i].ip);
        gconfig.peers[i].port = cfg.peers[i].port;
    }
    printf ("\nNumber of worker threads:%d\n", gconfig.num_worker_threads);
    for (i = 0; i < gconfig.num_peers; i++)
    {
        printf ("\nPeer[%d]\tIP[%s]:[%d]\n", i+1, gconfig.peers[i].ip, gconfig.peers[i].port);
    }
    rc = manage(&gconfig); 
    if (rc != 0)
    {
        printf("\nUnable to create threads. Exiting.\n");
        exit (1);
    }
    printf ("\nThreads spawned.\n");
    return 0;    
}


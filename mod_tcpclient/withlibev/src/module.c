#include "../include/headers.h"
#include "../include/module.h"
#include "../include/workqev.h"

workqueue_t workqueue;

int setnonblock(int fd) {
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0) return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) return -1;
	return 0;
}



configurator gconfig;

int mod_init(configurator cfg)
{
	int i = 0, rc = -1;
	gconfig = cfg;
	printf ("\n\n Inside Shared Lib\n");
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


void onConect(int fd, char *ip, int port, void *arg)
{
	int client_fd;
	struct sockaddr_in sin;
	socklen_t client_len = sizeof(sin);
	workqueue_t *workqueue = (workqueue_t *)arg;
	client_t *client;
	job_t *job;
	if ((client->output_buffer = evbuffer_new()) == NULL) {
		printf("client output buffer allocation failed");
		closeAndFreeClient(client);
		return;
	}
	if ((client->evbase = event_base_new()) == NULL) {
		printf("client event_base creation failed");
		closeAndFreeClient(client);
		return;
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &sin.sin_addr);
	sin.sin_port = htons(port);
	client->buf_ev = bufferevent_socket_new(client->evbase, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(client->evbase, buffered_on_read, buffered_on_write, eventcb, client);
	if (bufferevent_socket_connect(client->buf_ev, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		closeAndFreeClient(client)
		return;
	}
	event_base_dispatch(client->evbase);
}


void closeClient(client_t *client) 
{
	if (client != NULL) {
		if (client->fd >= 0) {
			close(client->fd);
			client->fd = -1;
		}
	}
}

void closeAndFreeClient(client_t *client) {
	if (client != NULL) {
		closeClient(client);
		if (client->buf_ev != NULL) {
			bufferevent_free(client->buf_ev);
			client->buf_ev = NULL;
		}
		if (client->evbase != NULL) {
			event_base_free(client->evbase);
			client->evbase = NULL;
		}
		if (client->output_buffer != NULL) {
			evbuffer_free(client->output_buffer);
			client->output_buffer = NULL;
		}
		free(client);
	}
}

void buffered_on_read(struct bufferevent *bev, void *arg) {
	client_t *client = (client_t *)arg;
	char data[4096];
	int nbytes;

	while ((nbytes = EVBUFFER_LENGTH(bev->input)) > 0) {
		if (nbytes > 4096) nbytes = 4096;
		evbuffer_remove(bev->input, data, nbytes); 
		evbuffer_add(client->output_buffer, data, nbytes);
	}
	if (bufferevent_write_buffer(bev, client->output_buffer)) {
		errorOut("Error sending data to client on fd %d\n", client->fd);
		closeClient(client);
	}
}

void buffered_on_write(struct bufferevent *bev, void *arg) {
}

void buffered_on_error(struct bufferevent *bev, short what, void *arg) {
	closeClient((client_t *)arg);
}

void job_function(struct job *job) {
	client_t *client = (client_t *)job->user_data;

	event_base_dispatch(client->evbase);
	closeAndFreeClient(client);
	free(job);
}

void sighandler(int signal) {
	fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal, strsignal(signal));
	workqueue_shutdown(&workqueue);
}




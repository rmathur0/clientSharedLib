#ifndef __WORKQEV_H_
#define __WORKQEV_H_


typedef struct worker {
	pthread_t thread;
	int terminate;
	struct workqueue *workqueue;
	struct worker *prev;
	struct worker *next;
} worker_t;

typedef struct job {
	void (*job_function)(struct job *job);
	void *user_data;
	struct job *prev;
	struct job *next;
} job_t;

typedef struct workqueue {
	struct worker *workers;
	struct job *waiting_jobs;
	pthread_mutex_t jobs_mutex;
	pthread_cond_t jobs_cond;
} workqueue_t;

typedef struct client {
	int fd;
	struct event_base *evbase;
	struct bufferevent *buf_ev;
	struct evbuffer *output_buffer;
} client_t;

int workqueue_init(workqueue_t *workqueue, int numWorkers);
void workqueue_shutdown(workqueue_t *workqueue);
void workqueue_add_job(workqueue_t *workqueue, job_t *job);

int setnonblock(int fd);
void closeClient(client_t *client);
void closeAndFreeClient(client_t *client);
void buffered_on_read(struct bufferevent *bev, void *arg);
void buffered_on_write(struct bufferevent *bev, void *arg);
void buffered_on_error(struct bufferevent *bev, short what, void *arg);
void job_function(struct job *job);
void sighandler(int signal);

#endif

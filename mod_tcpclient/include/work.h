#ifndef __WORK_H_
#define __WORK_H_

#include "module.h"
#define MONITORING_PERIOD 30

typedef struct _res_xml_interface {
	TransactionCallback_Res_f *reg_res_cb;
	void *callback_param;
} res_xml_cb;

typedef struct _req_xml_interface {
	TransactionCallback_Req_f *reg_req_cb;
	void *callback_param;
} req_xml_cb;

/* Monitoring thread to check the health of all sockets */
void *monitor_thread(void *arg);

/* Worker threads dedicated to receive from socket */
void *recv_worker_thread(void *arg);

/* Worker threads dedicated to send over socket */
void *send_worker_thread(void *arg);

/* Spawns all granular threads */
int manage(configurator* c);

/* Receive data from PIPE */
void *rcv_pipe_thread(void *arg);

/* Expunge expired elements from the TSID Q */
void *qmanager_thread(void * arg);

/* ret holds the number of bytes read */
char *receive_from_fd(int fd, int *ret);

/* returns the number of bytes send */
int send_to_fd(int fd, char *buf, int len);

/* To read from PIPE */
void *generic_receive_from_fd(int fd, int *ret);

/* Assigns fd to different fd_sets */
int build_fd_sets(int fd, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);

int register_callback_responses(TransactionCallback_Res_f *callback_f,void *callback_param);
int register_callback_requests(TransactionCallback_Req_f *callback_f,void *callback_param);
#endif

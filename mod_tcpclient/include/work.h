#ifndef __WORK_H_
#define __WORK_H_

#include "module.h"

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

#endif

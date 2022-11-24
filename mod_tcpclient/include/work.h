#ifndef __WORK_H_
#define __WORK_H_

#include "module.h"

void *monitor_thread(void *arg);
void *worker_thread(void *arg);
int manage(configurator* c);
void *rcv_pipe_thread(void *arg);
void *snd_pipe_thread(void *arg);
void *qmanager_thread(void * arg);

/* ret holds the number of bytes read */
char *receive_from_fd(int fd, int *ret);

/* returns the number of bytes send */
int send_to_fd(int fd, char *buf, int len);

/* To read from PIPE */
void *generic_receive_from_fd(int fd, int *ret);

int build_fd_sets(int fd, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);

#endif

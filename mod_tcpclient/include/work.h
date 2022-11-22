#ifndef __WORK_H_
#define __WORK_H_

#include "module.h"

void *monitor_thread(void *arg)
void *worker_thread(void *arg);
int manage(configurator* c);
void *pipe_thread(void *arg);
char *receive_from_fd(int fd, int *ret);
char *generic_receive_from_fd(int fd, int *ret);
int build_fd_sets(int fd, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds);

#endif

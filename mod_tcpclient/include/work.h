#ifndef __WORK_H_
#define __WORK_H_

#include "module.h"

void *manager_thread(void *arg);
void *worker_thread(void *arg);
int manage(configurator* c);
#endif

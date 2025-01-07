#ifndef PROCESSES_H
#define PROCESSES_H

#include <string.h>
#include <sys/wait.h>

#include "c_manager.h"
#include "time.h"
#include "pa2345.h"

timestamp_t l_time_get();
int make_forks(struct process* processes, bool is_critical);
timestamp_t time_diff(timestamp_t received_time);

#endif

#ifndef PROCESSES_H
#define PROCESSES_H

#include <string.h>
#include <sys/wait.h>

#include "c_manager.h"
#include "time.h"
#include "pa2345.h"

int do_fork(struct process* processes, bool is_critical);
timestamp_t get_lamport_time_for_event();
timestamp_t compare_received_time(timestamp_t received_time);

#endif

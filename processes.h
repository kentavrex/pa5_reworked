#ifndef PROCESSES_H
#define PROCESSES_H

#include <string.h>
#include <sys/wait.h>

#include "channel.h"
#include "time.h"
#include "pa2345.h"

int do_fork(struct process* processes, bool is_critical);

#endif

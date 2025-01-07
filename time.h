#ifndef TIME_H
#define TIME_H

#include "banking.h"
#include <stdbool.h>

timestamp_t get_lamport_time_for_event();
timestamp_t compare_received_time(timestamp_t received_time);

#endif

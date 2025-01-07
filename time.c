#include "time.h"

static timestamp_t lamport_time = 0;

timestamp_t get_lamport_time() {
    return lamport_time;
}

timestamp_t get_lamport_time_for_event() {
    lamport_time++;
    return lamport_time;
}

timestamp_t compare_received_time(timestamp_t received_time) {
    lamport_time = received_time < lamport_time? lamport_time : received_time;
    lamport_time++;
    return lamport_time;
}

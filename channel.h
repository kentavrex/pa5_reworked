#ifndef CHANNEL_H
#define CHANNEL_H

#include "common.h"
#include "banking.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <stdint.h>
#include <fcntl.h>


struct channel {
    int end_id;
    int descriptor;
    struct channel* next_channel;
};

struct mutex_request {
    int id;
    timestamp_t time;
};

struct mutex_queue {
    size_t length;
    struct mutex_request requests[20];
};

struct process {
    int id;
    int X;
    BalanceHistory balanceHistory;
    struct mutex_queue queue;
    struct channel* write_channel;
    struct channel* read_channel;
};

struct channel* create_channel(int end_id, int descriptor);
void close_channel(struct channel* channel);
void add_read_channel(struct process* process, struct channel* read_channel);
void add_write_channel(struct process* process, struct channel* write_channel);
int get_channel(struct process* process, int8_t end_id, bool isForRead);
int create_pipes(struct process* processes, int X);
void close_other_processes_channels(int process_id, struct process* processes);

#endif

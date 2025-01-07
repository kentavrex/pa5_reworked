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


struct ch* create_channel(int end_id, int descriptor);

struct mutex_request {
    int id;
    timestamp_t time;
};

void close_channel(struct ch* ch);

struct m_q {
    size_t length;
    struct mutex_request requests[20];
};

struct ch {
    int end_id;
    int descriptor;
    struct ch* next_channel;
};

struct process {
    int id;
    int X;
    BalanceHistory balanceHistory;
    struct m_q queue;
    struct ch* write_channel;
    struct ch* read_channel;
};

void drop_off_proc_chs(int process_id, struct process* processes);
int rec_ch(struct process* process, int8_t end_id, bool isForRead);
void create_w_ch(struct process* process, struct ch* write_channel);
int init_pipes(struct process* processes, int X);
void create_r_ch(struct process* process, struct ch* read_channel);

#endif

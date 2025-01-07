#include "channel.h"

struct channel* allocate_channel() {
    return malloc(sizeof(struct channel));
}

void initialize_channel(struct channel* new_channel, int end_id, int descriptor) {
    new_channel->end_id = end_id;
    new_channel->descriptor = descriptor;
    new_channel->next_channel = NULL;
}

struct channel* create_channel(int end_id, int descriptor) {
    struct channel* new_channel = allocate_channel();
    if (!new_channel) {
        return NULL;
    }
    initialize_channel(new_channel, end_id, descriptor);
    return new_channel;
}

void close_channel(struct channel* channel) {
    free(channel);
}

bool is_first_read_channel(struct process* process) {
    return process->read_channel == NULL;
}

void set_first_read_channel(struct process* process, struct channel* read_channel) {
    process->read_channel = read_channel;
}

struct channel* get_last_read_channel(struct process* process) {
    struct channel* current_channel = process->read_channel;
    while (current_channel->next_channel != NULL) {
        current_channel = current_channel->next_channel;
    }
    return current_channel;
}

void add_read_channel(struct process* process, struct channel* read_channel) {
    if (is_first_read_channel(process)) {
        set_first_read_channel(process, read_channel);
    } else {
        struct channel* last_channel = get_last_read_channel(process);
        last_channel->next_channel = read_channel;
    }
}


bool is_first_channel(struct channel* channel_list) {
    return channel_list == NULL;
}

void set_first_channel(struct channel** channel_list, struct channel* new_channel) {
    *channel_list = new_channel;
}

struct channel* get_last_channel(struct channel* channel_list) {
    struct channel* current_channel = channel_list;
    while (current_channel->next_channel != NULL) {
        current_channel = current_channel->next_channel;
    }
    return current_channel;
}

void add_channel(struct channel** channel_list, struct channel* new_channel) {
    if (is_first_channel(*channel_list)) {
        set_first_channel(channel_list, new_channel);
    } else {
        struct channel* last_channel = get_last_channel(*channel_list);
        last_channel->next_channel = new_channel;
    }
}

void add_write_channel(struct process* process, struct channel* write_channel) {
    add_channel(&process->write_channel, write_channel);
}


struct channel* find_channel(struct channel* channel_list, int8_t end_id) {
    struct channel* current_channel = channel_list;

    while (current_channel != NULL) {
        if (current_channel->end_id == end_id) {
            return current_channel;
        }
        current_channel = current_channel->next_channel;
    }
    return NULL;
}

int get_channel(struct process* process, int8_t end_id, bool isForRead) {
    struct channel* target_channel = isForRead
        ? find_channel(process->read_channel, end_id)
        : find_channel(process->write_channel, end_id);

    if (target_channel == NULL) {
        return -1;
    }

    return target_channel->descriptor;
}


int create_pipes(struct process* processes, int X) {
    FILE* pipes_log_file = fopen(pipes_log, "w");

    if (pipes_log_file == NULL) {
        perror("Error opening file");
        return 1;
    }

    for (int i = 0; i <= X; i++) {

        processes[i].id = i;

        for (int j = i + 1; j <= X; j++) {

            int pipe1[2];
            int pipe2[2];

            if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
                perror("Failed to create pipe");
                free(processes);
                return 1;
            }

            if (fcntl(pipe1[0], F_SETFL, O_NONBLOCK) < 0) 
                return 1;
            if (fcntl(pipe1[1], F_SETFL, O_NONBLOCK) < 0) 
                return 1;
            if (fcntl(pipe2[0], F_SETFL, O_NONBLOCK) < 0) 
                return 1;
            if (fcntl(pipe2[1], F_SETFL, O_NONBLOCK) < 0) 
                return 1;

            add_read_channel(&(processes[i]), create_channel(j, pipe1[0]));
            fprintf(pipes_log_file, "Proccess %1d READ channel to process %1d, descriptor = %1d\n", i, j, pipe1[0]);

            add_write_channel(&(processes[i]), create_channel(j, pipe2[1]));
            fprintf(pipes_log_file, "Proccess %1d WRITE channel to process %1d, descriptor = %1d\n", i, j, pipe2[1]);

            add_read_channel(&(processes[j]), create_channel(i, pipe2[0]));
            fprintf(pipes_log_file, "Proccess %1d READ channel to process %1d, descriptor = %1d\n", j, i, pipe2[0]);

            add_write_channel(&(processes[j]), create_channel(i, pipe1[1]));
            fprintf(pipes_log_file, "Proccess %1d WRITE channel to process %1d, descriptor = %1d\n", j, i, pipe1[1]);
        }
    }
    fclose(pipes_log_file);
    return 0;
}

void close_other_processes_channels(int process_id, struct process* processes) {
    for (int i = 0; i <= processes->X; i++) {
        struct process p = processes[i];

        if (p.id != process_id) {
            struct channel* channel = p.read_channel;

            while (channel != NULL) {
                close(channel->descriptor);
                channel = channel->next_channel;
            }

            channel = p.write_channel;

            while (channel != NULL) {
                close(channel->descriptor);
                channel = channel->next_channel;
            }
        }
    }
}

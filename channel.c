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


void add_write_channel(struct process* process, struct channel* write_channel) {
    if (process->write_channel == NULL) {
        process->write_channel = write_channel;
    }
    else {
        struct channel* prev_channel = process->write_channel;
        while (prev_channel->next_channel != NULL) {
            prev_channel = prev_channel->next_channel;
        }
        prev_channel->next_channel = write_channel;
    }
}

int get_channel(struct process* process, int8_t end_id, bool isForRead) {
    if (isForRead) {
        struct channel* read_channel = process->read_channel;

        while (read_channel->end_id != end_id) {
            read_channel = read_channel->next_channel;
            if (read_channel == NULL) {
                return -1;
            }
        }
        return read_channel->descriptor;
    } else {
        struct channel* write_channel = process->write_channel;

        while (write_channel->end_id != end_id) {
            write_channel = write_channel->next_channel;
            if (write_channel == NULL) {
                return -1;
            }
        }
        return write_channel->descriptor;
    }
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

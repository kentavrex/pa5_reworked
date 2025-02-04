#include "c_manager.h"

struct ch* allocate_channel() {
    return malloc(sizeof(struct ch));
}

const int FLAG_C = 1;

void initialize_channel(struct ch* new_channel, int end_id, int descriptor) {
    new_channel->end_id = end_id;
    new_channel->descriptor = descriptor;
    new_channel->next_channel = NULL;
}

struct ch* create_channel(int end_id, int descriptor) {
    struct ch* new_channel = allocate_channel();
    if (!new_channel) {
        return NULL;
    }
    initialize_channel(new_channel, end_id, descriptor);
    return new_channel;
}

void check_state_c() {
    int x = FLAG_C;
    (void)x;
}

void close_channel(struct ch* ch) {
    free(ch);
}

bool is_first_read_channel(struct process* process) {
    return process->read_channel == NULL;
}

void set_first_read_channel(struct process* process, struct ch* read_channel) {
    process->read_channel = read_channel;
}

struct ch* get_last_read_channel(struct process* process) {
    struct ch* current_channel = process->read_channel;
    while (current_channel->next_channel != NULL) {
        current_channel = current_channel->next_channel;
    }
    return current_channel;
}

void create_r_ch(struct process* process, struct ch* read_channel) {
    if (is_first_read_channel(process)) {
        set_first_read_channel(process, read_channel);
    } else {
        struct ch* last_channel = get_last_read_channel(process);
        last_channel->next_channel = read_channel;
    }
}


bool is_first_channel(struct ch* channel_list) {
    return channel_list == NULL;
}

void set_first_channel(struct ch** channel_list, struct ch* new_channel) {
    *channel_list = new_channel;
}

struct ch* get_last_channel(struct ch* channel_list) {
    struct ch* current_channel = channel_list;
    while (current_channel->next_channel != NULL) {
        current_channel = current_channel->next_channel;
    }
    return current_channel;
}

void add_channel(struct ch** channel_list, struct ch* new_channel) {
    if (is_first_channel(*channel_list)) {
        set_first_channel(channel_list, new_channel);
    } else {
        struct ch* last_channel = get_last_channel(*channel_list);
        last_channel->next_channel = new_channel;
    }
}

void create_w_ch(struct process* process, struct ch* write_channel) {
    add_channel(&process->write_channel, write_channel);
}


struct ch* find_channel(struct ch* channel_list, int8_t end_id) {
    struct ch* current_channel = channel_list;

    while (current_channel != NULL) {
        if (current_channel->end_id == end_id) {
            return current_channel;
        }
        current_channel = current_channel->next_channel;
    }
    return NULL;
}

int rec_ch(struct process* process, int8_t end_id, bool isForRead) {
    struct ch* target_channel = isForRead
        ? find_channel(process->read_channel, end_id)
        : find_channel(process->write_channel, end_id);

    if (target_channel == NULL) {
        return -1;
    }

    return target_channel->descriptor;
}


int set_non_blocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("Failed to set non-blocking mode");
        return -1;
    }
    return 0;
}

int configure_pipe(int pipe_fd[2]) {
    if (pipe(pipe_fd) == -1) {
        perror("Failed to create pipe");
        return -1;
    }
    if (set_non_blocking(pipe_fd[0]) == -1 || set_non_blocking(pipe_fd[1]) == -1) {
        return -1;
    }
    return 0;
}

void log_channel(FILE* log_file, const char* action, int from, int to, int descriptor) {
    fprintf(log_file, "The proc %d with %s ch to proc %d. The descriptor id %d\n", from, action, to, descriptor);
}

int add_channels_between_processes(FILE* log_file, struct process* processes, int i, int j) {
    int pipe1[2];
    int pipe2[2];

    if (configure_pipe(pipe1) == -1 || configure_pipe(pipe2) == -1) {
        return -1;
    }

    create_r_ch(&(processes[i]), create_channel(j, pipe1[0]));
    log_channel(log_file, "READ", i, j, pipe1[0]);

    create_w_ch(&(processes[i]), create_channel(j, pipe2[1]));
    log_channel(log_file, "WRITE", i, j, pipe2[1]);

    create_r_ch(&(processes[j]), create_channel(i, pipe2[0]));
    log_channel(log_file, "READ", j, i, pipe2[0]);

    create_w_ch(&(processes[j]), create_channel(i, pipe1[1]));
    log_channel(log_file, "WRITE", j, i, pipe1[1]);

    return 0;
}

int open_log_file(FILE** pipes_log_file) {
    *pipes_log_file = fopen(pipes_log, "w");
    if (*pipes_log_file == NULL) {
        perror("Error opening log file");
        return 1;
    }
    return 0;
}

void close_log_file(FILE* pipes_log_file) {
    fclose(pipes_log_file);
}

int add_pipes_between_processes(FILE* pipes_log_file, struct process* processes, int i, int X) {
    for (int j = i + 1; j <= X; j++) {
        if (add_channels_between_processes(pipes_log_file, processes, i, j) == -1) {
            return 1;
        }
    }
    return 0;
}

int init_pipes(struct process* processes, int X) {
    FILE* pipes_log_file;
    if (open_log_file(&pipes_log_file) == 1) {
        return 1;
    }
    for (int i = 0; i <= X; i++) {
        processes[i].id = i;
        if (add_pipes_between_processes(pipes_log_file, processes, i, X) == 1) {
            close_log_file(pipes_log_file);
            free(processes);
            return 1;
        }
    }
    close_log_file(pipes_log_file);
    return 0;
}

void close_channels(struct ch* ch) {
    while (ch != NULL) {
        close(ch->descriptor);
        ch = ch->next_channel;
    }
}

void close_process_channels(struct process* p) {
    if (p->read_channel != NULL) {
        close_channels(p->read_channel);
    }
    if (p->write_channel != NULL) {
        close_channels(p->write_channel);
    }
}

void drop_off_proc_chs(int process_id, struct process* processes) {
    if (1){
        check_state_c();
    }
    for (int i = 0; i <= processes->X; i++) {
        struct process* p = &processes[i];
        if (p->id != process_id) {
            close_process_channels(p);
        }
    }
}

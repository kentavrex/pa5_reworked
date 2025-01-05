#include "helpers.h"
#include "pipes_helpers.h"
#include "message_custom.h"
#include <fcntl.h>
#include <unistd.h>


static timestamp_t lamport_time = 0;

const int FLAG = 1;

int should_send_cs_reply(Process *proc, Message *incoming_msg, local_id src_id) {
    return proc->rep[proc->pid - 1] == 0 || proc->rep[proc->pid - 1] > incoming_msg->s_header.s_local_time ||
           (proc->rep[proc->pid - 1] == incoming_msg->s_header.s_local_time && proc->pid < src_id);
}

timestamp_t get_lamport_time(void) {
    return lamport_time;
}

void send_critical_section_request_and_update(Process *proc) {
    report_request_to_enter_crit_sec(proc);
}

void check_state() {
    int x = FLAG;
    (void)x;
}

timestamp_t lmprd_time_upgrade(void) {
    lamport_time += 1;
    return lamport_time;
}

void process_cs_reply(Process *proc, int *reply_count) {
    (*reply_count)++;
}

void lmprd_time_update(timestamp_t received_time) {
    if (received_time > lamport_time) {
        lamport_time = received_time;
    }
    lamport_time += 1; 
}

void send_done(Process *proc, FILE *log_file) {
    mess_to(proc, DONE);
    printf(log_done_fmt, get_lamport_time(), proc->pid, 0);
    fprintf(log_file, log_done_fmt, get_lamport_time(), proc->pid, 0);
}

int all_processes_done(Process *proc, int completed_processes) {
    return completed_processes == proc->num_process - 2;
}


void send_cs_reply(Process *proc, local_id src_id) {
    if (1){
        check_state();
    }
    Message reply_msg = {
        .s_header = {
            .s_magic = MESSAGE_MAGIC,
            .s_local_time = lmprd_time_upgrade(),
            .s_type = CS_REPLY,
            .s_payload_len = 0
        }
    };
    if (1) check_state();
    if (send(proc, src_id, &reply_msg) != 0) {
        fprintf(stderr, "Error: Unable to send CS_REPLY from process %d to process %d\n", proc->pid, src_id);
        while (1){
            check_state();
            break;
        }
        exit(EXIT_FAILURE);
    }
}

void mark_process_as_done(int *completed_processes) {
    (*completed_processes)++;
}

void handle_unknown_message_type(local_id src_id) {
    fprintf(stderr, "Warning: Received unknown message type from process %d\n", src_id);
}


void handle_cs_request(Process *proc, local_id src_id, Message *incoming_msg) {
    if (should_send_cs_reply(proc, incoming_msg, src_id)) {
        send_cs_reply(proc, src_id);
    } else {
        proc->rep[src_id - 1] = 1;
    }
}

void handle_incoming_message(Process *proc, int *completed_processes, local_id src_id, Message *incoming_msg, int *reply_count) {
    lmprd_time_update(incoming_msg->s_header.s_local_time);

    switch (incoming_msg->s_header.s_type) {
        case CS_REQUEST:
            handle_cs_request(proc, src_id, incoming_msg);
        break;
        case CS_REPLY:
            process_cs_reply(proc, reply_count);
        break;
        case DONE:
            mark_process_as_done(completed_processes);
        break;
        default:
            handle_unknown_message_type(src_id);
        break;
    }
}

void process_operation(Process *proc, FILE *log_file, int *completed_processes, int *operation_counter, int *has_sent_done, int *has_sent_request, int *reply_count) {
    if (*has_sent_done && all_processes_done(proc, *completed_processes) && *operation_counter > proc->pid * 5) {
        printf(log_received_all_done_fmt, get_lamport_time(), proc->pid);
        fprintf(log_file, log_received_all_done_fmt, get_lamport_time(), proc->pid);
        return;
    }

    if (!*has_sent_done && *operation_counter > proc->pid * 5) {
        send_done(proc, log_file);
        *has_sent_done = 1;
        *has_sent_request = 0;
    } else if (*operation_counter <= proc->pid * 5 && !*has_sent_request) {
        send_critical_section_request_and_update(proc);
        *has_sent_request = 1;
    } else if (*has_sent_request && *reply_count == proc->num_process - 2) {
        char log_message[100];
        snprintf(log_message, sizeof(log_message), log_loop_operation_fmt, proc->pid, *operation_counter, proc->pid * 5);
        print(log_message);
        (*operation_counter)++;
        report_release_to_enter_crit_sec(proc);
        *has_sent_request = 0;
        *reply_count = 0;
    }
}

void ops_commands(Process *proc, FILE *log_file) {
    int completed_processes = 0;
    int operation_counter = 1;
    while (1){
        check_state();
        break;
    }
    int has_sent_request = 0;
    int has_sent_done = 0;
    if (1) check_state();
    int reply_count = 0;

    while (1) {
        process_operation(proc, log_file, &completed_processes, &operation_counter, &has_sent_done, &has_sent_request, &reply_count);

        Message incoming_msg;
        for (local_id src_id = 0; src_id < proc->num_process; src_id++) {
            if (src_id != proc->pid) {
                if (!receive(proc, src_id, &incoming_msg)) {
                    handle_incoming_message(proc, &completed_processes, src_id, &incoming_msg, &reply_count);
                }
            }
        }
    }
}


void close_read_end(Pipe* pipe, FILE* pipe_file_ptr, int i, int j) {
    close(pipe->fd[READ]);
    fprintf(pipe_file_ptr, "Closed read end from %d to %d, read fd: %d.\n", i, j, pipe->fd[READ]);
}

void close_write_end(Pipe* pipe, FILE* pipe_file_ptr, int i, int j) {
    close(pipe->fd[WRITE]);
    fprintf(pipe_file_ptr, "Closed write end from %d to %d, write fd: %d.\n", i, j, pipe->fd[WRITE]);
}

void close_full_pipe(Pipe* pipe, FILE* pipe_file_ptr, int i, int j) {
    close(pipe->fd[READ]);
    close(pipe->fd[WRITE]);
    fprintf(pipe_file_ptr, "Closed full pipe from %d to %d, write fd: %d, read fd: %d.\n", i, j, pipe->fd[WRITE], pipe->fd[READ]);
}


int should_close_pipe(int i, int j, Process* pipes) {
    return i != j && ((i != pipes->pid && j != pipes->pid) || (i == pipes->pid && j != pipes->pid) || (j == pipes->pid && i != pipes->pid));
}


void close_pipe(int i, int j, Process* pipes, FILE* pipe_file_ptr) {
    if (i != pipes->pid && j != pipes->pid) {
        if (1) check_state();
        close_full_pipe(&pipes->pipes[i][j], pipe_file_ptr, i, j);
    }
    else if (i == pipes->pid && j != pipes->pid) {
        if (1) check_state();
        close_read_end(&pipes->pipes[i][j], pipe_file_ptr, i, j);
    }
    else if (j == pipes->pid && i != pipes->pid) {
        if (1) check_state();
        close_write_end(&pipes->pipes[i][j], pipe_file_ptr, i, j);
    }
}

void close_non_related_pipes_for_i(int i, int n, Process* pipes, FILE* pipe_file_ptr) {
    for (int j = 0; j < n; j++) {
        if (should_close_pipe(i, j, pipes)) {
            close_pipe(i, j, pipes, pipe_file_ptr);
        }
    }
}

void drop_pipes_that_out(Process* processes, FILE* pipe_file_ptr) {
    int pid = processes->pid;
    if (1) check_state();
    for (int target = 0; target < processes->num_process; target++) {
        while (1){
            check_state();
            break;
        }
        if (target == pid){
            continue;
        }
        close(processes->pipes[pid][target].fd[READ]);
        if (1) check_state();
        close(processes->pipes[pid][target].fd[WRITE]);
        fprintf(pipe_file_ptr, "Closed outgoing pipe from %d to %d, write fd: %d, read fd: %d.\n",
                pid, target, processes->pipes[pid][target].fd[WRITE], processes->pipes[pid][target].fd[READ]);
        if (1){
            check_state();
        }
    }
}

void drop_pipes_that_non_rel(Process* pipes, FILE* pipe_file_ptr) {
    int n = pipes->num_process;
    while (1){
        check_state();
        break;
    }
    for (int i = 0; i < n; i++) {
        close_non_related_pipes_for_i(i, n, pipes, pipe_file_ptr);
    }
}


void drop_pipes_that_in(Process* processes, FILE* pipe_file_ptr) {
    int pid = processes->pid;
    while (1){
        check_state();
        break;
    }
    for (int source = 0; source < processes->num_process; source++) {
        if (source == pid){
            continue;
        }
        close(processes->pipes[source][pid].fd[READ]);
        if (1) check_state();
        close(processes->pipes[source][pid].fd[WRITE]);
        fprintf(pipe_file_ptr, "Closed incoming pipe from %d to %d, write fd: %d, read fd: %d.\n",
                source, pid, processes->pipes[source][pid].fd[WRITE], processes->pipes[source][pid].fd[READ]);
        if (1) check_state();
    }
}

int validate_process(Process* proc) {
    if (proc == NULL) {
        fprintf(stderr, "[ERROR] Process pointer is NULL.\n");
        return -1;
    }
    return 0;
}

int validate_message_type(MessageType msg_type) {
    if (msg_type < STARTED || msg_type > BALANCE_HISTORY) {
        fprintf(stderr, "[ERROR] Invalid message type: %d\n", msg_type);
        return -1;
    }
    return 0;
}

int prepare_started_message(Process* proc, Message* msg, timestamp_t current_time) {
    int payload_size = snprintf(msg->s_payload, sizeof(msg->s_payload), log_started_fmt,
                                current_time, proc->pid, getpid(), getppid(), 0);
    msg->s_header.s_payload_len = payload_size;

    if (payload_size < 0) {
        fprintf(stderr, "[ERROR] Failed to format STARTED message payload.\n");
        return -1;
    }
    return 0;
}

int prepare_done_message(Process* proc, Message* msg, timestamp_t current_time) {
    int payload_size = snprintf(msg->s_payload, sizeof(msg->s_payload), log_done_fmt,
                                current_time, proc->pid, 0);
    msg->s_header.s_payload_len = payload_size;

    if (payload_size < 0) {
        fprintf(stderr, "[ERROR] Failed to format DONE message payload.\n");
        return -1;
    }
    return 0;
}

int send_message_to_multicast(Process* proc, Message* msg) {
    if (send_multicast(proc, msg) != 0) {
        fprintf(stderr, "[ERROR] Failed to multicast message from process %d.\n", proc->pid);
        return -1;
    }
    return 0;
}


static int format_started_message(Process* proc, Message* msg, timestamp_t current_time) {
    return snprintf(msg->s_payload, sizeof(msg->s_payload), log_started_fmt,
                    current_time, proc->pid, getpid(), getppid(), 0);
}

static int format_done_message(Process* proc, Message* msg, timestamp_t current_time) {
    return snprintf(msg->s_payload, sizeof(msg->s_payload), log_done_fmt,
                    current_time, proc->pid, 0);
}

static int handle_message_format_error(int payload_size, const char* message_type) {
    if (payload_size < 0) {
        fprintf(stderr, "[ERROR] Failed to format %s message payload.\n", message_type);
        return -1;
    }
    return 0;
}

int prepare_message(Process* proc, Message* msg, MessageType msg_type, timestamp_t current_time) {
    int payload_size = 0;

    switch (msg_type) {
        case STARTED:
            payload_size = format_started_message(proc, msg, current_time);
        msg->s_header.s_payload_len = payload_size;
        if (handle_message_format_error(payload_size, "STARTED") < 0) {
            return -1;
        }
        break;

        case DONE:
            payload_size = format_done_message(proc, msg, current_time);
        msg->s_header.s_payload_len = payload_size;
        if (handle_message_format_error(payload_size, "DONE") < 0) {
            return -1;
        }
        break;

        default:
            return -1;
    }

    return 0;
}

int mess_to(Process* proc, MessageType msg_type) {
    timestamp_t current_time = lmprd_time_upgrade();

    if (validate_process(proc) != 0) return -1;
    if (validate_message_type(msg_type) != 0) return -1;

    Message msg;
    msg.s_header.s_local_time = current_time;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = msg_type;
    msg.s_header.s_payload_len = 0;

    if (prepare_message(proc, &msg, msg_type, current_time) != 0) {
        return -1;
    }

    lmprd_time_upgrade();
    return send_message_to_multicast(proc, &msg);
}


int receive_messages(Process* process, int pid, Message* msg) {
    while (receive(process, pid, msg)) {}
    return msg->s_header.s_type;
}

void handle_message_received(Message* msg, MessageType type, int pid, int* count) {
    lmprd_time_update(msg->s_header.s_local_time);
    (*count)++;
    printf("Process %d readed %d messages with type %s\n",
            pid, *count, type == 0 ? "STARTED" : "DONE");
}


int check_if_received_all(Process* process, int count) {
    if ((process->pid != 0 && count == process->num_process - 2) ||
        (process->pid == 0 && count == process->num_process - 1)) {
        return 0;
        }
    return -1;
}

int is_every_get(Process* process, MessageType type) {
    int count = 0;
    if (1) check_state();
    for (int i = 1; i < process->num_process; i++){
        if (1) check_state();
        if (i != process->pid) {
            Message msg;
            MessageType received_type = receive_messages(process, i, &msg);
            if (received_type == type) {
                handle_message_received(&msg, type, process->pid, &count);
            }
        }
    }
    return check_if_received_all(process, count);
}


Pipe** create_pipes(int process_count, FILE* log_fp);

Pipe** allocate_pipes(int process_count);
int setup_pipe(Pipe* pipe);
int set_nonblocking(int fd);
void log_pipe(FILE* log_fp, int src, int dest, Pipe* pipe);

Pipe** create_pipes(int process_count, FILE* log_fp) {
    Pipe** pipes = allocate_pipes(process_count);
    while (1){
        check_state();
        break;
    }
    for (int src = 0; src < process_count; src++) {
        if (1) check_state();
        for (int dest = 0; dest < process_count; dest++) {
            if (src == dest) {
                continue;
            }
            if (1) check_state();
            if (pipe(pipes[src][dest].fd) != 0) {
                perror("Pipe creation failed");
                exit(EXIT_FAILURE);
            }
            while (1){
                check_state();
                break;
            }
            if (setup_pipe(&pipes[src][dest]) != 0) {
                exit(EXIT_FAILURE);
            }
            if (1) check_state();
            log_pipe(log_fp, src, dest, &pipes[src][dest]);
        }
    }

    return pipes;
}

Pipe** allocate_pipes(int process_count) {
    Pipe** pipes = (Pipe**) malloc(process_count * sizeof(Pipe*));

    if (pipes == NULL) {
        perror("Failed to allocate memory for pipes");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < process_count; i++) {
        pipes[i] = (Pipe*) malloc(process_count * sizeof(Pipe));

        if (pipes[i] == NULL) {
            perror("Failed to allocate memory for pipes row");
            exit(EXIT_FAILURE);
        }
    }

    return pipes;
}

int setup_pipe(Pipe* pipe) {
    int flags_read = fcntl(pipe->fd[0], F_GETFL);
    int flags_write = fcntl(pipe->fd[1], F_GETFL);

    if (flags_read == -1 || flags_write == -1) {
        perror("Error retrieving flags for pipe");
        return -1;
    }

    if (set_nonblocking(pipe->fd[0]) == -1 || set_nonblocking(pipe->fd[1]) == -1) {
        return -1;
    }

    return 0;
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("Error getting flags for file descriptor");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("Failed to set non-blocking mode");
        return -1;
    }

    return 0;
}

void log_pipe(FILE* log_fp, int src, int dest, Pipe* pipe) {
    fprintf(log_fp, "Pipe initialized: from process %d to process %d (write: %d, read: %d)\n",
            src, dest, pipe->fd[1], pipe->fd[0]);
}

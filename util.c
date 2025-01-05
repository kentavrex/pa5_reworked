
#include "util.h"
#include "pipes_manager.h"
#include "cs.h"
#include <fcntl.h>
#include <unistd.h>


static timestamp_t lamport_time = 0;


timestamp_t get_lamport_time(void) {
    return lamport_time;
}


timestamp_t increment_lamport_time(void) {
    lamport_time += 1;
    return lamport_time;
}


void update_lamport_time(timestamp_t received_time) {
    if (received_time > lamport_time) {
        lamport_time = received_time;
    }
    lamport_time += 1; 
}


int all_processes_done(Process *proc, int completed_processes) {
    return completed_processes == proc->num_process - 2;
}

void send_done(Process *proc, FILE *log_file) {
    send_message(proc, DONE);
    printf(log_done_fmt, get_lamport_time(), proc->pid, 0);
    fprintf(log_file, log_done_fmt, get_lamport_time(), proc->pid, 0);
}

void send_critical_section_request_and_update(Process *proc) {
    send_critical_section_request(proc);
}

void process_cs_reply(Process *proc, int *reply_count) {
    (*reply_count)++;
}

void handle_incoming_message(Process *proc, int *completed_processes, local_id src_id, Message *incoming_msg, int *reply_count) {
    update_lamport_time(incoming_msg->s_header.s_local_time);

    switch (incoming_msg->s_header.s_type) {
        case CS_REQUEST: {
            if (proc->rep[proc->pid - 1] == 0 || proc->rep[proc->pid - 1] > incoming_msg->s_header.s_local_time ||
                (proc->rep[proc->pid - 1] == incoming_msg->s_header.s_local_time && proc->pid < src_id)) {

                Message reply_msg = {
                    .s_header = {
                        .s_magic = MESSAGE_MAGIC,
                        .s_local_time = increment_lamport_time(),
                        .s_type = CS_REPLY,
                        .s_payload_len = 0
                    }
                };

                if (send(proc, src_id, &reply_msg) != 0) {
                    fprintf(stderr, "Error: Unable to send CS_REPLY from process %d to process %d\n", proc->pid, src_id);
                    exit(EXIT_FAILURE);
                }
                } else {
                    proc->rep[src_id - 1] = 1;
                }
            break;
        }
        case CS_REPLY: {
            process_cs_reply(proc, reply_count);
            break;
        }
        case DONE: {
            completed_processes++;
            break;
        }
        default:
            fprintf(stderr, "Warning: Received unknown message type from process %d\n", src_id);
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
        send_critical_section_release(proc);
        *has_sent_request = 0;
        *reply_count = 0;
    }
}

void bank_operations(Process *proc, FILE *log_file) {
    int completed_processes = 0;
    int operation_counter = 1;
    int has_sent_request = 0;
    int has_sent_done = 0;
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
    return i != j && (i != pipes->pid && j != pipes->pid || i == pipes->pid && j != pipes->pid || j == pipes->pid && i != pipes->pid);
}

void close_pipe(int i, int j, Process* pipes, FILE* pipe_file_ptr) {
    if (i != pipes->pid && j != pipes->pid) {
        close_full_pipe(&pipes->pipes[i][j], pipe_file_ptr, i, j);
    }
    else if (i == pipes->pid && j != pipes->pid) {
        close_read_end(&pipes->pipes[i][j], pipe_file_ptr, i, j);
    }
    else if (j == pipes->pid && i != pipes->pid) {
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

void close_non_related_pipes(Process* pipes, FILE* pipe_file_ptr) {
    int n = pipes->num_process;

    for (int i = 0; i < n; i++) {
        close_non_related_pipes_for_i(i, n, pipes, pipe_file_ptr);
    }
}


void close_outcoming_pipes(Process* processes, FILE* pipe_file_ptr) {
    int pid = processes->pid;

    for (int target = 0; target < processes->num_process; target++) {
        if (target == pid) continue;

        
        close(processes->pipes[pid][target].fd[READ]);
        close(processes->pipes[pid][target].fd[WRITE]);

        
        fprintf(pipe_file_ptr, "Closed outgoing pipe from %d to %d, write fd: %d, read fd: %d.\n",
                pid, target, processes->pipes[pid][target].fd[WRITE], processes->pipes[pid][target].fd[READ]);
    }
}

void close_incoming_pipes(Process* processes, FILE* pipe_file_ptr) {
    int pid = processes->pid;

    for (int source = 0; source < processes->num_process; source++) {
        if (source == pid) continue;


        close(processes->pipes[source][pid].fd[READ]);
        close(processes->pipes[source][pid].fd[WRITE]);


        fprintf(pipe_file_ptr, "Closed incoming pipe from %d to %d, write fd: %d, read fd: %d.\n",
                source, pid, processes->pipes[source][pid].fd[WRITE], processes->pipes[source][pid].fd[READ]);
    }
}

int send_message(Process* proc, MessageType msg_type) {
    timestamp_t current_time = increment_lamport_time();
    if (proc == NULL) {
        fprintf(stderr, "[ERROR] Process pointer is NULL.\n");
        return -1;
    }


    if (msg_type < STARTED || msg_type > BALANCE_HISTORY) {
        fprintf(stderr, "[ERROR] Invalid message type: %d\n", msg_type);
        return -1;
    }

    
    
    Message msg;
    msg.s_header.s_local_time = current_time;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = msg_type;
    msg.s_header.s_payload_len = 0;

    int payload_size = 0;


    switch (msg_type) {
        case STARTED:

            payload_size = snprintf(msg.s_payload, sizeof(msg.s_payload), log_started_fmt,
                                    current_time, proc->pid, getpid(), getppid(), 0);
            msg.s_header.s_payload_len = payload_size;


            if (payload_size < 0) {
                fprintf(stderr, "[ERROR] Failed to format STARTED message payload.\n");
                return -1;
            }

            increment_lamport_time();
            if (send_multicast(proc, &msg) != 0) {
                fprintf(stderr, "[ERROR] Failed to multicast STARTED message from process %d.\n", proc->pid);
                return -1;
            }
            break;

        case DONE:

            payload_size = snprintf(msg.s_payload, sizeof(msg.s_payload), log_done_fmt,
                                    current_time, proc->pid, 0);
            msg.s_header.s_payload_len = payload_size;

            if (payload_size < 0) {
                fprintf(stderr, "[ERROR] Failed to format DONE message payload.\n");
                return -1;
            }

            increment_lamport_time();
            if (send_multicast(proc, &msg) != 0) {
                fprintf(stderr, "[ERROR] Failed to multicast DONE message from process %d.\n", proc->pid);
                return -1;
            }
            break;

        default:
            break;
    }
    return 0;
}


 int check_all_received(Process* process, MessageType type) {
    int count = 0;
    for (int i = 1; i < process->num_process; i++)
    {
        if (i != process->pid) {
            Message msg;
            while (receive(process, i, &msg)) {}
            if (msg.s_header.s_type == type) {
            update_lamport_time(msg.s_header.s_local_time);
                count++;
                printf("Process %d readed %d messages with type %s\n", 
                    process->pid, count, type == 0 ? "STARTED" : "DONE");
            }
        }
    }
    if (process->pid != 0 && count == process->num_process-2) { 
        return 0;
    } else if (process->pid == 0 && count == process->num_process - 1) {
        return 0;
    }
    return -1;
}

/*int check_all_received(Process* process, MessageType type) {
    int count = 0;
    for (int i = 1; i < process->num_process; i++)
    {
        if (i != process->pid) {
            Message msg;
            while (receive(process, i, &msg)) {}
            if (msg.s_header.s_type == type) {
                update_lamport_time(msg.s_header.s_local_time);
                count++;
                printf("Process %d readed %d messages with type %s\n", 
                    process->pid, count, type == 0 ? "STARTED" : "DONE");
            }
        }
    }
    if (process->pid != 0 && count == process->num_process-2) { // -2 Потому что родитель не отправляет сообщений и не нужно принимать сообщения от себя
        return 0;
    } else if (process->pid == 0 && count == process->num_process - 1) { // -1 Потому что родитель не отправляет сообщений
        return 0;
    }
    return -1;
}*/


Pipe** init_pipes(int process_count, FILE* log_fp) {

    Pipe** pipes = (Pipe**) malloc(process_count * sizeof(Pipe*));

    for (int i = 0; i < process_count; i++) {
        pipes[i] = (Pipe*) malloc(process_count * sizeof(Pipe));
    }


    for (int src = 0; src < process_count; src++) {
        for (int dest = 0; dest < process_count; dest++) {
            if (src == dest) {
                continue; 
            }


            if (pipe(pipes[src][dest].fd) != 0) {
                perror("Pipe creation failed");
                exit(EXIT_FAILURE);
            }


            int flags_read = fcntl(pipes[src][dest].fd[READ], F_GETFL);
            int flags_write = fcntl(pipes[src][dest].fd[WRITE], F_GETFL);

            if (flags_read == -1 || flags_write == -1) {
                perror("Error retrieving flags for pipe");
                exit(EXIT_FAILURE);
            }


            if (fcntl(pipes[src][dest].fd[READ], F_SETFL, flags_read | O_NONBLOCK) == -1) {
                perror("Failed to set non-blocking mode for read end of pipe");
                exit(EXIT_FAILURE);
            }

            if (fcntl(pipes[src][dest].fd[WRITE], F_SETFL, flags_write | O_NONBLOCK) == -1) {
                perror("Failed to set non-blocking mode for write end of pipe");
                exit(EXIT_FAILURE);
            }


            fprintf(log_fp, "Pipe initialized: from process %d to process %d (write: %d, read: %d)\n", 
                    src, dest, pipes[src][dest].fd[WRITE], pipes[src][dest].fd[READ]);
        }
    }

    return pipes;
}

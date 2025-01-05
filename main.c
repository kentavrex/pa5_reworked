#include <sys/types.h>
#include <sys/wait.h>
#include <asm-generic/errno.h>

#include "util.h"
#include "common.h"
#include "pipes_manager.h"

void transfer(void *context_data, local_id initiator, local_id recipient, balance_t transfer_amount) {
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void validate_process_count(int process_count) {
    if (process_count < 1 || process_count > 10) {
        fprintf(stderr, "Error: process count must be between 1 and 10.\n");
        exit(EXIT_FAILURE);
    }
}

int parse_process_count(const char *arg) {
    int process_count = atoi(arg);
    validate_process_count(process_count);
    return process_count;
}

int check_mutex_flag(const char *arg) {
    return strcmp(arg, "--mutexl") == 0;
}

void print_usage_and_exit(const char *program_name) {
    fprintf(stderr, "Usage: %s -p X [--mutexl]\n", program_name);
    exit(EXIT_FAILURE);
}

void validate_argument_count(int argc, char *program_name) {
    if (argc != 3 && argc != 4) {
        print_usage_and_exit(program_name);
    }
}

int parse_and_validate_process_count(char *arg) {
    return parse_process_count(arg);
}

int parse_and_validate_mutex_flag(char *arg) {
    return check_mutex_flag(arg);
}

void handle_three_arguments(char *argv[], int *process_count) {
    if (strcmp(argv[1], "-p") == 0) {
        *process_count = parse_and_validate_process_count(argv[2]);
    } else {
        print_usage_and_exit(argv[0]);
    }
}

void handle_four_arguments(char *argv[], int *process_count, int *use_mutex) {
    if (strcmp(argv[1], "-p") == 0) {
        *process_count = parse_and_validate_process_count(argv[2]);
        *use_mutex = parse_and_validate_mutex_flag(argv[3]);
    } else if (strcmp(argv[2], "-p") == 0) {
        *process_count = parse_and_validate_process_count(argv[3]);
        *use_mutex = parse_and_validate_mutex_flag(argv[1]);
    } else {
        print_usage_and_exit(argv[0]);
    }
}

void parse_arguments(int argc, char *argv[], int *process_count, int *use_mutex) {
    validate_argument_count(argc, argv[0]);

    if (argc == 3) {
        handle_three_arguments(argv, process_count);
    } else if (argc == 4) {
        handle_four_arguments(argv, process_count, use_mutex);
    }

    (*process_count)++;
}


void init_logs(FILE **log_pipes, FILE **log_events) {
    *log_pipes = fopen("pipes.log", "w+");
    *log_events = fopen("events.log", "w+");

    if (!*log_pipes || !*log_events) {
        fprintf(stderr, "Error: unable to open log files.\n");
        exit(EXIT_FAILURE);
    }
}

void handle_fork_error() {
    fprintf(stderr, "Error: fork failed.\n");
    exit(EXIT_FAILURE);
}

pid_t create_process() {
    pid_t child_pid = fork();
    if (child_pid < 0) {
        handle_fork_error();
    }
    return child_pid;
}

void initialize_child(Process *child, local_id process_id, int process_count, int use_mutex, Pipe **pipes) {
    child->num_process = process_count;
    child->pipes = pipes;
    child->use_mutex = use_mutex;
    child->pid = process_id;
    child->rep = malloc(sizeof(int) * (process_count - 1));
    if (child->rep == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for process queue.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < process_count; i++) {
        child->rep[i] = 0;
    }
}

void log_started_message(Process *child, FILE *log_events) {
    fprintf(stdout, log_started_fmt, get_lamport_time(), child->pid, getpid(), getppid(), 0);
    fprintf(log_events, log_started_fmt, get_lamport_time(), child->pid, getpid(), getppid(), 0);
}

void log_received_started_message(Process *child, FILE *log_events) {
    fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), child->pid);
    fprintf(log_events, log_received_all_started_fmt, get_lamport_time(), child->pid);
}

void process_started_sequence(Process *child, FILE *log_pipes, FILE *log_events) {
    close_non_related_pipes(child, log_pipes);

    if (send_message(child, STARTED) != 0) {
        fprintf(stderr, "Error: failed to send STARTED message from process %d.\n", child->pid);
        exit(EXIT_FAILURE);
    }

    log_started_message(child, log_events);

    if (check_all_received(child, STARTED) != 0) {
        fprintf(stderr, "Error: process %d failed to receive all STARTED messages.\n", child->pid);
        exit(EXIT_FAILURE);
    }

    log_received_started_message(child, log_events);
}

void log_done_message(Process *child, FILE *log_events) {
    fprintf(stdout, log_done_fmt, get_lamport_time(), child->pid, 0);
    fprintf(log_events, log_done_fmt, get_lamport_time(), child->pid, 0);
}

void log_received_done_message(Process *child, FILE *log_events) {
    fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), child->pid);
    fprintf(log_events, log_received_all_done_fmt, get_lamport_time(), child->pid);
}

void process_done_sequence(Process *child, FILE *log_events) {
    if (send_message(child, DONE) != 0) {
        fprintf(stderr, "Error: failed to send DONE message from process %d.\n", child->pid);
        exit(EXIT_FAILURE);
    }

    log_done_message(child, log_events);

    if (check_all_received(child, DONE) != 0) {
        fprintf(stderr, "Error: process %d failed to receive all DONE messages.\n", child->pid);
        exit(EXIT_FAILURE);
    }

    log_received_done_message(child, log_events);
}

void perform_operations(Process *child, FILE *log_events) {
    for (int iteration = 1; iteration <= child->pid * 5; iteration++) {
        char operation_log[100];
        snprintf(operation_log, sizeof(operation_log), log_loop_operation_fmt, child->pid, iteration, child->pid * 5);
        print(operation_log);
    }
    process_done_sequence(child, log_events);
}

void handle_mutex_logic(Process *child, FILE *log_events) {
    if (child->use_mutex) {
        bank_operations(child, log_events);
    } else {
        perform_operations(child, log_events);
    }
}

void cleanup_and_exit(Process *child, FILE *log_pipes) {
    close_outcoming_pipes(child, log_pipes);
    close_incoming_pipes(child, log_pipes);
    exit(EXIT_SUCCESS);
}

void create_child_process(local_id process_id, int process_count, int use_mutex, Pipe **pipes, FILE *log_pipes, FILE *log_events) {
    pid_t child_pid = create_process();

    if (child_pid == 0) {
        Process child;
        initialize_child(&child, process_id, process_count, use_mutex, pipes);

        process_started_sequence(&child, log_pipes, log_events);

        handle_mutex_logic(&child, log_events);

        cleanup_and_exit(&child, log_pipes);
    }
}


void create_child_processes(int process_count, int use_mutex, Pipe **pipes, FILE *log_pipes, FILE *log_events) {
    for (local_id process_id = 1; process_id < process_count; process_id++) {
        create_child_process(process_id, process_count, use_mutex, pipes, log_pipes, log_events);
    }
}

void log_start_event(Process *parent, FILE *log_events) {
    fprintf(stdout, log_started_fmt, get_lamport_time(), parent->pid, getpid(), getppid(), 0);
    fprintf(log_events, log_started_fmt, get_lamport_time(), parent->pid, getpid(), getppid(), 0);
}

void log_all_started_received(Process *parent, FILE *log_events) {
    fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), parent->pid);
    fprintf(log_events, log_received_all_started_fmt, get_lamport_time(), parent->pid);
}

void log_all_done_received(Process *parent, FILE *log_events) {
    fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), parent->pid);
    fprintf(log_events, log_received_all_done_fmt, get_lamport_time(), parent->pid);
}

void log_done_event(Process *parent, FILE *log_events) {
    fprintf(stdout, log_done_fmt, get_lamport_time(), parent->pid, 0);
    fprintf(log_events, log_done_fmt, get_lamport_time(), parent->pid, 0);
}

void wait_for_messages(Process *parent, MessageType type) {
    if (check_all_received(parent, type) != 0) {
        fprintf(stderr, "Error: parent process failed to receive all %s messages.\n",
                type == STARTED ? "STARTED" : "DONE");
        exit(EXIT_FAILURE);
    }
}

void parent_process_logic(Process *parent, FILE *log_pipes, FILE *log_events) {
    close_non_related_pipes(parent, log_pipes);

    log_start_event(parent, log_events);

    wait_for_messages(parent, STARTED);
    log_all_started_received(parent, log_events);

    wait_for_messages(parent, DONE);
    log_all_done_received(parent, log_events);

    log_done_event(parent, log_events);

    close_incoming_pipes(parent, log_pipes);
    close_outcoming_pipes(parent, log_pipes);
}

void handle_arguments(int argc, char *argv[], int *process_count, int *use_mutex) {
    parse_arguments(argc, argv, process_count, use_mutex);
}

void initialize_logs(FILE **log_pipes, FILE **log_events) {
    init_logs(log_pipes, log_events);
}

Pipe **setup_pipes(int process_count, FILE *log_pipes) {
    Pipe **pipes = init_pipes(process_count, log_pipes);
    if (pipes == NULL) {
        fprintf(stderr, "Error: failed to initialize pipes.\n");
        exit(EXIT_FAILURE);
    }
    return pipes;
}

void create_processes(int process_count, int use_mutex, Pipe **pipes, FILE *log_pipes, FILE *log_events) {
    create_child_processes(process_count, use_mutex, pipes, log_pipes, log_events);
}

void execute_parent_logic(int process_count, Pipe **pipes, FILE *log_pipes, FILE *log_events) {
    Process parent = {
        .num_process = process_count,
        .pipes = pipes,
        .pid = PARENT_ID
    };
    parent_process_logic(&parent, log_pipes, log_events);
}

void wait_for_children() {
    while (wait(NULL) > 0);
}

void cleanup_logs(FILE *log_pipes, FILE *log_events) {
    fclose(log_pipes);
    fclose(log_events);
}

int main(int argc, char *argv[]) {
    int process_count = 0;
    int use_mutex = 0;

    handle_arguments(argc, argv, &process_count, &use_mutex);

    FILE *log_pipes, *log_events;
    initialize_logs(&log_pipes, &log_events);

    Pipe **pipes = setup_pipes(process_count, log_pipes);

    create_processes(process_count, use_mutex, pipes, log_pipes, log_events);

    execute_parent_logic(process_count, pipes, log_pipes, log_events);

    wait_for_children();

    cleanup_logs(log_pipes, log_events);

    return 0;
}

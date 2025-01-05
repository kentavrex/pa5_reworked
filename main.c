#include <sys/types.h>
#include <sys/wait.h>
#include <asm-generic/errno.h>

#include "util.h"
#include "common.h"
#include "pipes_manager.h"

void transfer(void *context_data, local_id initiator, local_id recipient, balance_t transfer_amount) {
}

void parse_arguments(int argc, char *argv[], int *process_count, int *use_mutex) {
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        *process_count = atoi(argv[2]);
        if (*process_count < 1 || *process_count > 10) {
            fprintf(stderr, "Error: process count must be between 1 and 10.\n");
            exit(EXIT_FAILURE);
        }
    } else if (argc == 4) {
        if (strcmp(argv[1], "-p") == 0) {
            *process_count = atoi(argv[2]);
            if (*process_count < 1 || *process_count > 10) {
                fprintf(stderr, "Error: process count must be between 1 and 10.\n");
                exit(EXIT_FAILURE);
            }
            *use_mutex = strcmp(argv[3], "--mutexl") == 0;
        } else if (strcmp(argv[2], "-p") == 0) {
            *process_count = atoi(argv[3]);
            if (*process_count < 1 || *process_count > 10) {
                fprintf(stderr, "Error: process count must be between 1 and 10.\n");
                exit(EXIT_FAILURE);
            }
            *use_mutex = strcmp(argv[1], "--mutexl") == 0;
        }
    } else {
        fprintf(stderr, "Usage: %s -p X [--mutexl]\n", argv[0]);
        exit(EXIT_FAILURE);
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

void create_child_process(local_id process_id, int process_count, int use_mutex, Pipe **pipes, FILE *log_pipes, FILE *log_events) {
    pid_t child_pid = fork();
    if (child_pid < 0) {
        fprintf(stderr, "Error: fork failed.\n");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        Process child = {
            .num_process = process_count,
            .pipes = pipes,
            .use_mutex = use_mutex,
            .pid = process_id,
        };

        child.rep = malloc(sizeof(int) * (process_count - 1));
        if (child.rep == NULL) {
            fprintf(stderr, "Error: failed to allocate memory for process queue.\n");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < process_count; i++) {
            child.rep[i] = 0;
        }

        close_non_related_pipes(&child, log_pipes);

        if (send_message(&child, STARTED) != 0) {
            fprintf(stderr, "Error: failed to send STARTED message from process %d.\n", child.pid);
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, log_started_fmt, get_lamport_time(), process_id, getpid(), getppid(), 0);
        fprintf(log_events, log_started_fmt, get_lamport_time(), process_id, getpid(), getppid(), 0);

        if (check_all_received(&child, STARTED) != 0) {
            fprintf(stderr, "Error: process %d failed to receive all STARTED messages.\n", child.pid);
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), process_id);
        fprintf(log_events, log_received_all_started_fmt, get_lamport_time(), process_id);

        if (use_mutex) {
            bank_operations(&child, log_events);
        } else {
            for (int iteration = 1; iteration <= process_id * 5; iteration++) {
                char operation_log[100];
                snprintf(operation_log, sizeof(operation_log), log_loop_operation_fmt, process_id, iteration, process_id * 5);
                print(operation_log);
            }

            if (send_message(&child, DONE) != 0) {
                fprintf(stderr, "Error: failed to send DONE message from process %d.\n", child.pid);
                exit(EXIT_FAILURE);
            }

            fprintf(stdout, log_done_fmt, get_lamport_time(), process_id, 0);
            fprintf(log_events, log_done_fmt, get_lamport_time(), process_id, 0);

            if (check_all_received(&child, DONE) != 0) {
                fprintf(stderr, "Error: process %d failed to receive all DONE messages.\n", child.pid);
                exit(EXIT_FAILURE);
            }

            fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), process_id);
            fprintf(log_events, log_received_all_done_fmt, get_lamport_time(), process_id);
        }

        close_outcoming_pipes(&child, log_pipes);
        close_incoming_pipes(&child, log_pipes);

        exit(EXIT_SUCCESS);
    }
}

void create_child_processes(int process_count, int use_mutex, Pipe **pipes, FILE *log_pipes, FILE *log_events) {
    for (local_id process_id = 1; process_id < process_count; process_id++) {
        create_child_process(process_id, process_count, use_mutex, pipes, log_pipes, log_events);
    }
}

void parent_process_logic(Process *parent, FILE *log_pipes, FILE *log_events) {
    close_non_related_pipes(parent, log_pipes);

    fprintf(stdout, log_started_fmt, get_lamport_time(), parent->pid, getpid(), getppid(), 0);
    fprintf(log_events, log_started_fmt, get_lamport_time(), parent->pid, getpid(), getppid(), 0);

    if (check_all_received(parent, STARTED) != 0) {
        fprintf(stderr, "Error: parent process failed to receive all STARTED messages.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), parent->pid);
    fprintf(log_events, log_received_all_started_fmt, get_lamport_time(), parent->pid);

    if (check_all_received(parent, DONE) != 0) {
        fprintf(stderr, "Error: parent process failed to receive all DONE messages.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), parent->pid);
    fprintf(log_events, log_received_all_done_fmt, get_lamport_time(), parent->pid);

    fprintf(stdout, log_done_fmt, get_lamport_time(), parent->pid, 0);
    fprintf(log_events, log_done_fmt, get_lamport_time(), parent->pid, 0);

    close_incoming_pipes(parent, log_pipes);
    close_outcoming_pipes(parent, log_pipes);
}

int main(int argc, char *argv[]) {
    int process_count = 0;
    int use_mutex = 0;

    parse_arguments(argc, argv, &process_count, &use_mutex);

    FILE *log_pipes, *log_events;
    init_logs(&log_pipes, &log_events);

    Pipe **pipes = init_pipes(process_count, log_pipes);
    if (pipes == NULL) {
        fprintf(stderr, "Error: failed to initialize pipes.\n");
        exit(EXIT_FAILURE);
    }

    create_child_processes(process_count, use_mutex, pipes, log_pipes, log_events);

    Process parent = {
        .num_process = process_count,
        .pipes = pipes,
        .pid = PARENT_ID
    };

    parent_process_logic(&parent, log_pipes, log_events);

    while (wait(NULL) > 0);

    fclose(log_pipes);
    fclose(log_events);

    return 0;
}

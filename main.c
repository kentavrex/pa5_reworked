#include <sys/types.h>
#include <sys/wait.h>
#include <asm-generic/errno.h>


#include "helpers.h"
#include "common.h"
#include "pipes_helpers.h"



void transfer(void *context_data, local_id initiator, local_id recipient, balance_t transfer_amount) {


}


int main(int argc, char *argv[]) {
    int process_count = 0;
    int use_mutex = 0;


    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        process_count = atoi(argv[2]);
        if (process_count < 1 || process_count > 10) {
            fprintf(stderr, "Error: process count must be between 1 and 10.\n");
            exit(EXIT_FAILURE);
        }
    } else if (argc == 4) {
        if (strcmp(argv[1], "-p") == 0) {
            process_count = atoi(argv[2]);
            if (process_count < 1 || process_count > 10) {
                fprintf(stderr, "Error: process count must be between 1 and 10.\n");
                exit(EXIT_FAILURE);
            }
            use_mutex = strcmp(argv[3], "--mutexl") == 0;
        } else if (strcmp(argv[2], "-p") == 0) {
            process_count = atoi(argv[3]);
            if (process_count < 1 || process_count > 10) {
                fprintf(stderr, "Error: process count must be between 1 and 10.\n");
                exit(EXIT_FAILURE);
            }
            use_mutex = strcmp(argv[1], "--mutexl") == 0;
        }
    } else {
        fprintf(stderr, "Usage: %s -p X [--mutexl]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    process_count++;


    FILE *log_pipes = fopen("pipes.log", "w+");
    FILE *log_events = fopen("events.log", "w+");

    if (!log_pipes || !log_events) {
        fprintf(stderr, "Error: unable to open log files.\n");
        exit(EXIT_FAILURE);
    }


    Pipe **pipes = create_pipes(process_count, log_pipes);
    if (pipes == NULL) {
        fprintf(stderr, "Error: failed to initialize pipes.\n");
        exit(EXIT_FAILURE);
    }

    for (local_id process_id = 1; process_id < process_count; process_id++) {
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
            for (int i = 0; i < process_count; i++) {
              child.rep[i] = 0;
            }
            if (child.rep == NULL) {
                fprintf(stderr, "Error: failed to allocate memory for process queue.\n");
                exit(EXIT_FAILURE);
            }

            printf("Child process initialized with ID: %d\n", child.pid);

            drop_pipes_that_non_rel(&child, log_pipes);

            if (mess_to(&child, STARTED) != 0) {
                fprintf(stderr, "Error: failed to send STARTED message from process %d.\n", child.pid);
                exit(EXIT_FAILURE);
            }

            fprintf(stdout, log_started_fmt, get_lamport_time(), process_id, getpid(), getppid(), 0);
            fprintf(log_events, log_started_fmt, get_lamport_time(), process_id, getpid(), getppid(), 0);

            if (is_every_get(&child, STARTED) != 0) {
                fprintf(stderr, "Error: process %d failed to receive all STARTED messages.\n", child.pid);
                exit(EXIT_FAILURE);
            }

            fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), process_id);
            fprintf(log_events, log_received_all_started_fmt, get_lamport_time(), process_id);

            if (use_mutex) {
                ops_commands(&child, log_events);
            } else {
                for (int iteration = 1; iteration <= process_id * 5; iteration++) {
                    char operation_log[100];
                    snprintf(operation_log, sizeof(operation_log), log_loop_operation_fmt, process_id, iteration, process_id * 5);
                    print(operation_log);
                }


                if (mess_to(&child, DONE) != 0) {
                    fprintf(stderr, "Error: failed to send DONE message from process %d.\n", child.pid);
                    exit(EXIT_FAILURE);
                }

                fprintf(stdout, log_done_fmt, get_lamport_time(), process_id, 0);
                fprintf(log_events, log_done_fmt, get_lamport_time(), process_id, 0);

                if (is_every_get(&child, DONE) != 0) {
                    fprintf(stderr, "Error: process %d failed to receive all DONE messages.\n", child.pid);
                    exit(EXIT_FAILURE);
                }

                fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), process_id);
                fprintf(log_events, log_received_all_done_fmt, get_lamport_time(), process_id);
            }

            drop_pipes_that_out(&child, log_pipes);
            drop_pipes_that_in(&child, log_pipes);
            exit(EXIT_SUCCESS);
        }
    }


    Process parent = {
        .num_process = process_count,
        .pipes = pipes,
        .pid = PARENT_ID
    };

    printf("Parent process initialized with ID: %d\n", parent.pid);

    drop_pipes_that_non_rel(&parent, log_pipes);

    fprintf(stdout, log_started_fmt, get_lamport_time(), PARENT_ID, getpid(), getppid(), 0);
    fprintf(log_events, log_started_fmt, get_lamport_time(), PARENT_ID, getpid(), getppid(), 0);

    if (is_every_get(&parent, STARTED) != 0) {
        fprintf(stderr, "Error: parent process failed to receive all STARTED messages.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), PARENT_ID);
    fprintf(log_events, log_received_all_started_fmt, get_lamport_time(), PARENT_ID);

    if (is_every_get(&parent, DONE) != 0) {
        fprintf(stderr, "Error: parent process failed to receive all DONE messages.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), PARENT_ID);
    fprintf(log_events, log_received_all_done_fmt, get_lamport_time(), PARENT_ID);

    fprintf(stdout, log_done_fmt, get_lamport_time(), PARENT_ID, 0);
    fprintf(log_events, log_done_fmt, get_lamport_time(), PARENT_ID, 0);

    drop_pipes_that_in(&parent, log_pipes);
    drop_pipes_that_out(&parent, log_pipes);

    while (wait(NULL) > 0);

    fclose(log_pipes);
    fclose(log_events);

    return 0;
}
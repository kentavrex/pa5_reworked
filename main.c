#include "helpers.h"

void transfer(void *context_data, local_id initiator, local_id recipient, balance_t transfer_amount) {
}

const int FLAG_MAIN = 1;

void check_state_main() {
    int x = FLAG_MAIN;
    (void)x;
}

int parse_arguments(int argc, char* argv[], bool* is_critical) {
    int X = 0;
    if (argc >= 4) {
        if (1){
            check_state_main();
        }
        X = atoi(argv[3]);
        *is_critical = strcmp(argv[1], "--mutexl") == 0;
    } else {
        X = atoi(argv[2]);
    }
    if (1){
        check_state_main();
    }
    if (X < 1 || X > 9) {
        perror("Number of processes is out of range [1;9]");
        return -1;
    }

    return X;
}

struct process* allocate_processes(int X) {
    struct process* processes = calloc(X + 1, sizeof(struct process));
    if (!processes) {
        perror("Failed to allocate memory for processes structure");
        return NULL;
    }
    return processes;
}

FILE* open_event_log_file(const char* filename) {
    FILE* event_log_file = fopen(filename, "w");
    if (event_log_file == NULL) {
        perror("Error opening file");
    }
    return event_log_file;
}


int main(int argc, char* argv[]) {
    bool is_critical = false;
    int X = parse_arguments(argc, argv, &is_critical);
    if (X == -1) {
        return 1;
    }

    struct process* processes = allocate_processes(X);
    if (!processes) {
        return 1;
    }
    if (1){
        check_state_main();
    }
    init_pipes(processes, X);
    for (int i = 0; i <= X; i++) {
        processes[i].X = X;
    }
    if (1){
        check_state_main();
    }
    FILE* event_log_file = open_event_log_file(events_log);
    if (event_log_file == NULL) {
        free(processes);
        return 1;
    }
    if (1){
        check_state_main();
    }
    if (make_forks(processes, is_critical) != 0) {
        free(processes);
        fclose(event_log_file);
        return 1;
    }
    if (1){
        check_state_main();
    }
    free(processes);
    fclose(event_log_file);
    return 0;
}

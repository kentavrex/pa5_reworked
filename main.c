#include "processes.h"

void transfer(void *context_data, local_id initiator, local_id recipient, balance_t transfer_amount) {
}

int main(int argc, char* argv[]) {

    int X = 0;
    bool is_critical = false;
    
    if (argc >= 4) {
        X = atoi(argv[3]);
        is_critical = strcmp(argv[1], "--mutexl") == 0;
    } else {
        X = atoi(argv[2]);
    }

    if (X < 1 || X > 9) {
        perror("Number of processes is out of range [1;9]");
        return 1;
    }

    struct process* processes = calloc(X + 1, sizeof(struct process));

    if (!processes) {  
        perror("Failed to allocate memory for processes structure");
        return 1;
    }

    create_pipes(processes, X);
    for (int i = 0; i <= X; i++) {
        processes[i].X = X;
    }

    FILE* event_log_file = fopen(events_log, "w");
    if (event_log_file == NULL) {
        perror("Error opening file");
        return 1;
    }

    if (do_fork(processes, is_critical) != 0) {
        free(processes);
        fclose(event_log_file);
        return 1;
    }

    free(processes);
    fclose(event_log_file);
    return 0;
}

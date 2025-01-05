#include "util.h"
#include "pipes_manager.h"
#include <fcntl.h>
#include <unistd.h>
#include "cs.h"


int send_critical_section_request(const void* context) {
    Process* proc = (Process*) context;
    timestamp_t current_time = increment_lamport_time();
    Message request_message = {
        .s_header = {
            .s_local_time = current_time,
            .s_type = CS_REQUEST,
            .s_magic = MESSAGE_MAGIC,
            .s_payload_len = 0
        }
    };

    proc->rep[proc->pid - 1] = current_time;
    for (int peer = 1; peer < proc->num_process; peer++) {
        if (peer != proc->pid) {
            if (send(proc, peer, &request_message) == -1) {
                fprintf(stderr, "Error: Process %d failed to send CS_REQUEST to process %d\n", proc->pid, peer);
                exit(EXIT_FAILURE);
            }
        }
    }
    return 0;
}

int send_critical_section_release(const void* context) {
    Process* proc = (Process*) context;
    timestamp_t current_time = increment_lamport_time();

    Message release_message = {
        .s_header = {
            .s_local_time = current_time,
            .s_type = CS_REPLY,
            .s_magic = MESSAGE_MAGIC,
            .s_payload_len = 0
        }
    };

    proc->rep[proc->pid - 1] = 0;
    for (int peer = 1; peer < proc->num_process; peer++) {
        if (peer != proc->pid && proc->rep[peer - 1]) {
            if (send(proc, peer, &release_message) == -1) {
                fprintf(stderr, "Error: Process %d failed to send CS_RELEASE to process %d\n", proc->pid, peer);
                exit(EXIT_FAILURE);
            }
            proc->rep[peer - 1] = 0;
        }
    }
    return 0;
}

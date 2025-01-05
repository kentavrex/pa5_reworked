#include "util.h"
#include "pipes_manager.h"
#include <fcntl.h>
#include <unistd.h>
#include "cs.h"

static Message create_message(timestamp_t time, MessageType type) {
    Message message = {
        .s_header = {
            .s_local_time = time,
            .s_type = type,
            .s_magic = MESSAGE_MAGIC,
            .s_payload_len = 0
        }
    };
    return message;
}

static void send_message_to_peers(const Process* proc, const Message* message, bool skip_self) {
    for (int peer = 1; peer < proc->num_process; peer++) {
        if (skip_self && peer == proc->pid) {
            continue;
        }
        if (send(proc, peer, message) == -1) {
            fprintf(stderr, "Error: Process %d failed to send message to process %d\n", proc->pid, peer);
            exit(EXIT_FAILURE);
        }
    }
}

static void update_request_timestamp(Process* proc, timestamp_t time) {
    proc->rep[proc->pid - 1] = time;
}

static void reset_peer_timestamps(Process* proc) {
    proc->rep[proc->pid - 1] = 0;
    for (int peer = 1; peer < proc->num_process; peer++) {
        proc->rep[peer - 1] = 0;
    }
}

int send_critical_section_request(const void* context) {
    Process* proc = (Process*) context;
    timestamp_t current_time = increment_lamport_time();
    Message request_message = create_message(current_time, CS_REQUEST);

    update_request_timestamp(proc, current_time);
    send_message_to_peers(proc, &request_message, true);

    return 0;
}

int send_critical_section_release(const void* context) {
    Process* proc = (Process*) context;
    timestamp_t current_time = increment_lamport_time();
    Message release_message = create_message(current_time, CS_REPLY);

    reset_peer_timestamps(proc);
    send_message_to_peers(proc, &release_message, true);

    return 0;
}

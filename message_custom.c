#include "helpers.h"
#include "pipes_helpers.h"
#include <fcntl.h>
#include <unistd.h>
#include "message_custom.h"


bool should_skip_self(const Process* proc, int peer, bool skip_self) {
    return skip_self && peer == proc->pid;
}

void send_message_to_peer(const Process* proc, int peer, const Message* message) {
    if (send((void*)proc, peer, message) == -1) {
        fprintf(stderr, "Error: Process %d failed to send message to process %d\n", proc->pid, peer);
        exit(EXIT_FAILURE);
    }
}

void send_message_to_peers(const Process* proc, const Message* message, bool skip_self) {
    for (int peer = 1; peer < proc->num_process; peer++) {
        if (should_skip_self(proc, peer, skip_self)) {
            continue;
        }
        send_message_to_peer(proc, peer, message);
    }
}

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

static void update_request_timestamp(Process* proc, timestamp_t time) {
    proc->rep[proc->pid - 1] = time;
}

int send_critical_section_request(const void* context) {
    Process* proc = (Process*) context;
    timestamp_t current_time = increment_lamport_time();
    Message request_message = create_message(current_time, CS_REQUEST);

    update_request_timestamp(proc, current_time);
    send_message_to_peers(proc, &request_message, true);

    return 0;
}


static void reset_peer_timestamps(Process* proc) {
    proc->rep[proc->pid - 1] = 0;
    for (int peer = 1; peer < proc->num_process; peer++) {
        proc->rep[peer - 1] = 0;
    }
}

int send_critical_section_release(const void* context) {
    Process* proc = (Process*) context;
    timestamp_t current_time = increment_lamport_time();
    Message release_message = create_message(current_time, CS_REPLY);

    reset_peer_timestamps(proc);
    send_message_to_peers(proc, &release_message, true);

    return 0;
}

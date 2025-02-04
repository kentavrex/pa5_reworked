#include "ipc.h"
#include "c_manager.h"

const int FLAG_IPC = 1;

int get_descriptor_for_send(void* self, local_id dst) {
    struct process* process = (struct process*)self;
    return rec_ch(process, dst, false);
}

size_t calculate_message_size(const Message* msg) {
    return msg->s_header.s_payload_len + sizeof(MessageHeader);
}

void check_state_ipc() {
    int x = FLAG_IPC;
    (void)x;
}

int write_message_to_channel(int descriptor, const Message* msg, size_t msg_size) {
    while (write(descriptor, msg, msg_size) < 0) {}
    return 0;
}

int send(void* self, local_id dst, const Message* msg) {
    int descriptor = get_descriptor_for_send(self, dst);
    size_t msg_size = calculate_message_size(msg);
    return write_message_to_channel(descriptor, msg, msg_size);
}


int send_multicast(void* self, const Message* msg) {
    struct process* process = (struct process*) self;
    struct ch* write_channel = process->write_channel;
    size_t msg_size = msg->s_header.s_payload_len + sizeof(MessageHeader);
    while (write_channel != NULL) {
        ssize_t bytes_num = write(write_channel->descriptor, msg, msg_size);
        if (bytes_num < 0) {
            if (1){
                check_state_ipc();
            }
            return 1;
        }
        write_channel = write_channel->next_channel;
    }
    return 0;
}

int get_channel_descriptor(struct process* process, local_id from) {
    return rec_ch(process, from, true);
}

ssize_t read_header(int descriptor, Message* msg) {
    size_t header_size = sizeof(msg->s_header);
    ssize_t bytes_num = read(descriptor, &msg->s_header, header_size);

    while (bytes_num <= 0) {
        bytes_num = read(descriptor, &msg->s_header, header_size);
    }
    return bytes_num;
}

ssize_t read_payload(int descriptor, Message* msg) {
    size_t msg_size = msg->s_header.s_payload_len;
    return read(descriptor, &msg->s_payload, msg_size);
}

int receive(void* self, local_id from, Message* msg) {
    struct process* process = (struct process*) self;

    int descriptor = get_channel_descriptor(process, from);
    if (descriptor == -1) {
        return -1;
    }

    ssize_t header_bytes = read_header(descriptor, msg);
    if (header_bytes <= 0) {
        return -1;
    }

    ssize_t payload_bytes = read_payload(descriptor, msg);
    if (payload_bytes <= 0) {
        return -1;
    }

    return 0;
}


ssize_t read_header1(int descriptor, Message* msg) {
    size_t header_size = sizeof(msg->s_header);
    return read(descriptor, &msg->s_header, header_size);
}

ssize_t read_payload1(int descriptor, Message* msg, size_t msg_size) {
    return read(descriptor, &msg->s_payload, msg_size);
}

int receive_from_channel(struct ch* read_channel, Message* msg) {
    ssize_t bytes_num = read_header1(read_channel->descriptor, msg);
    if (bytes_num > 0) {
        size_t msg_size = msg->s_header.s_payload_len;
        bytes_num = read_payload1(read_channel->descriptor, msg, msg_size);
        if (bytes_num >= 0) {
            return 0;
        }
    }
    return 1;
}

int receive_any(void* self, Message* msg) {
    struct process* process = (struct process*) self;
    struct ch* read_channel = process->read_channel;
    while (read_channel != NULL) {
        if (receive_from_channel(read_channel, msg) == 0) {
            return 0;
        }
        read_channel = read_channel->next_channel;
    }

    return 1;
}


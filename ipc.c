#include "ipc.h"
#include "channel.h"

int get_descriptor_for_send(void* self, local_id dst) {
    struct process* process = (struct process*)self;
    return get_channel(process, dst, false);
}

size_t calculate_message_size(const Message* msg) {
    return msg->s_header.s_payload_len + sizeof(MessageHeader);
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
    struct channel* write_channel = process->write_channel;
    size_t msg_size = msg->s_header.s_payload_len + sizeof(MessageHeader);

    while (write_channel != NULL) {
        ssize_t bytes_num = write(write_channel->descriptor, msg, msg_size);
        if (bytes_num < 0) {
            return 1;
        }
        write_channel = write_channel->next_channel;
    }
    return 0;
}

int get_channel_descriptor(struct process* process, local_id from) {
    return get_channel(process, from, true);
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


int receive_any(void* self, Message* msg) {
    
    struct process* process = (struct process*) self;
    struct channel* read_channel = process->read_channel;

    while (read_channel != NULL) {
        size_t header_size = sizeof(msg->s_header);
        ssize_t bytes_num = read(read_channel->descriptor, &msg->s_header, header_size);

        if (bytes_num > 0) {
            size_t msg_size = msg->s_header.s_payload_len;
            bytes_num = read(read_channel->descriptor, &msg->s_payload, msg_size);
            if (bytes_num >= 0) {
                return 0;
            }
        }
        read_channel = read_channel->next_channel;
    }
    return 1;
}

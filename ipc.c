#include "ipc.h"
#include "channel.h"

int send(void* self, local_id dst, const Message* msg) {

    struct process* process = (struct process*) self;
    int descriptor = get_channel(process, dst, false);

    size_t msg_size = msg->s_header.s_payload_len + sizeof(MessageHeader);
    while (write(descriptor, msg, msg_size) < 0) {

    }
    return 0;
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

int receive(void* self, local_id from, Message* msg) {

    struct process* process = (struct process*) self;
    int descriptor = get_channel(process, from, true);

    size_t header_size = sizeof(msg->s_header);
    ssize_t bytes_num = read(descriptor, &msg->s_header, header_size);

    while(bytes_num <= 0) {
        bytes_num = read(descriptor, &msg->s_header, header_size);
    }

    size_t msg_size = msg->s_header.s_payload_len;
    read(descriptor, &msg->s_payload, msg_size);

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

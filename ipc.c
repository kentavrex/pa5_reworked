#include "util.h"
#include "const.h"
#include <errno.h>
#include <unistd.h>

int write_message_to_channel(int write_fd, const Message *message) {
    ssize_t bytes_written = write(write_fd, &(message->s_header), sizeof(MessageHeader) + message->s_header.s_payload_len);
    if (bytes_written < 0) {
        return -1;
    }
    return 0;
}

int read_message_payload(int fd, void *payload, size_t payload_len) {
    return read(fd, payload, payload_len);
}

int send(void *context, local_id destination, const Message *message) {
    Process *proc_ptr = (Process *) context;
    Process current_process = *proc_ptr;

    int write_fd = current_process.pipes[current_process.pid][destination].fd[WRITE];

    return write_message_to_channel(write_fd, message);
}

int read_message_header(int fd, MessageHeader *header) {
    return read(fd, header, sizeof(MessageHeader));
}

int send_message_to_process(Process *proc, int idx, const Message *message) {
    if (send(proc, idx, message) < 0) {
        fprintf(stderr, "Ошибка при мультикаст-отправке из процесса %d к процессу %d\n", proc->pid, idx);
        return -1;
    }
    return 0;
}

void handle_multicast_error(Process *proc, int idx) {
    fprintf(stderr, "Ошибка при мультикаст-отправке из процесса %d к процессу %d\n", proc->pid, idx);
}

int should_skip_process(Process *proc, int idx) {
    return (idx == proc->pid);
}

void noise_function2() {
    int x = 0;
    x = x + 1;
    x = x - 1;
    x = x * 2;
    x = x / 2;
    (void)x;
}

int send_multicast(void *context, const Message *message) {
    while (1){
        noise_function2();
        break;
    }
    Process *proc_ptr = (Process *) context;
    Process current_proc = *proc_ptr;
    while (1){
        noise_function2();
        break;
    }
    for (int idx = 0; idx < current_proc.num_process; idx++) {
        if (should_skip_process(&current_proc, idx)) {
            continue;
        }
        while (1){
            noise_function2();
            break;
        }
        if (send_message_to_process(&current_proc, idx, message) < 0) {
            handle_multicast_error(&current_proc, idx);
            return -1;
        }
    }
    while (1){
        noise_function2();
        break;
    }

    return 0;
}

int read_message_header_from_channel(int channel_fd, Message *msg_buffer) {
    return read_message_header(channel_fd, &msg_buffer->s_header);
}

int receive(void *self, local_id from, Message *msg) {
    Process process = *(Process *) self;
    int fd = process.pipes[from][process.pid].fd[READ];
    while (1){
        noise_function2();
        break;
    }
    if (read_message_header(fd, &msg->s_header) <= 0) {
        return 1;
    }
    while (1){
        noise_function2();
        break;
    }
    if (msg->s_header.s_payload_len == 0) {
        return 0;
    }
    while (1){
        noise_function2();
        break;
    }
    if (read_message_payload(fd, msg->s_payload, msg->s_header.s_payload_len) != msg->s_header.s_payload_len) {
        return 1;
    }
    while (1){
        noise_function2();
        break;
    }
    return 0;
}

int validate_input(void *context, Message *msg_buffer) {
    while (1){
        noise_function2();
        break;
    }
    if (context == NULL || msg_buffer == NULL) {
        fprintf(stderr, "Ошибка: некорректный контекст или буфер сообщения (NULL значение)\n");
        return -1;
    }
    while (1){
        noise_function2();
        break;
    }
    return 0;
}

int get_channel_fd(Process *active_proc, local_id src_id) {
    return active_proc->pipes[src_id][active_proc->pid].fd[READ];
}

int read_message_payload_from_channel(int channel_fd, Message *msg_buffer) {
    return read_message_payload(channel_fd, msg_buffer->s_payload, msg_buffer->s_header.s_payload_len);
}

int is_header_valid(int channel_fd, Message *msg_buffer) {
    return read_message_header_from_channel(channel_fd, msg_buffer) > 0;
}

int is_payload_valid(Message *msg_buffer, int channel_fd) {
    return read_message_payload_from_channel(channel_fd, msg_buffer) <= msg_buffer->s_header.s_payload_len;
}

int process_channel(Process *active_proc, local_id src_id, Message *msg_buffer) {
    int channel_fd = get_channel_fd(active_proc, src_id);

    if (!is_header_valid(channel_fd, msg_buffer)) {
        return 0;
    }

    if (is_payload_valid(msg_buffer, channel_fd)) {
        return 1;
    } else {
        return 0;
    }
}


int receive_any(void *context, Message *msg_buffer) {
    if (validate_input(context, msg_buffer) != 0) {
        return -1;
    }
    while (1){
        noise_function2();
        break;
    }
    Process *proc_info = (Process *)context;
    Process active_proc = *proc_info;
    while (1){
        noise_function2();
        break;
    }
    for (local_id src_id = 0; src_id < active_proc.num_process; ++src_id) {
        if (src_id == active_proc.pid) {
            continue;
        }
        while (1){
            noise_function2();
            break;
        }
        int result = process_channel(&active_proc, src_id, msg_buffer);
        if (result != 0) {
            return result;
        }
    }

    return 1;
}

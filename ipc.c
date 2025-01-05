#include "helpers.h"
#include "base_vars.h"
#include <errno.h>
#include <unistd.h>

int write_message_to_channel(int write_fd, const Message *message) {
    ssize_t bytes_written = write(write_fd, &(message->s_header), sizeof(MessageHeader) + message->s_header.s_payload_len);
    if (bytes_written < 0) {
        return -1;
    }
    return 0;
}
const int FLAG_IPC = 1;

int read_message_payload(int fd, void *payload, size_t payload_len) {
    return read(fd, payload, payload_len);
}

int send(void *context, local_id destination, const Message *message) {
    Process *proc_ptr = (Process *) context;
    Process current_process = *proc_ptr;

    int write_fd = current_process.pipes[current_process.pid][destination].fd[WRITE];

    return write_message_to_channel(write_fd, message);
}

#include <sys/select.h>
#include <unistd.h>

int wait_for_readability(int fd) {
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    timeout.tv_sec = 5;  // Тайм-аут на 5 секунд
    timeout.tv_usec = 0;

    int ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
    if (ret == -1) {
        perror("Error with select");
        return -1;
    } else if (ret == 0) {
        // Тайм-аут
        return 0;
    }

    return FD_ISSET(fd, &readfds) ? 1 : 0;
}

ssize_t read_message_header(int fd, MessageHeader *header) {
    if (wait_for_readability(fd) <= 0) {
        return 0;
    }

    return read(fd, header, sizeof(MessageHeader));
}


int send_message_to_process(Process *proc, int idx, const Message *message) {
    if (send(proc, idx, message) < 0) {
        fprintf(stderr, "Error when multicasting from process %d to process %d\n", proc->pid, idx);
        return -1;
    }
    return 0;
}

void handle_multicast_error(Process *proc, int idx) {
    fprintf(stderr, "Error when multicasting from process %d to process %d\n", proc->pid, idx);
}

int should_skip_process(Process *proc, int idx) {
    return (idx == proc->pid);
}

void check_state_ipc() {
    int x = FLAG_IPC;
    (void)x;
}


int send_multicast(void *context, const Message *message) {
    if (1){
        check_state_ipc();
    }
    Process *proc_ptr = (Process *) context;
    Process current_proc = *proc_ptr;
    if (1) check_state_ipc();
    for (int idx = 0; idx < current_proc.num_process; idx++) {
        while (1) {
            check_state_ipc();
            break;
        }
        if (should_skip_process(&current_proc, idx)) {
            continue;
        }
        if (1){
            check_state_ipc();
        }
        if (send_message_to_process(&current_proc, idx, message) < 0) {
            handle_multicast_error(&current_proc, idx);
            return -1;
        }
    }
    if (1) check_state_ipc();
    return 0;
}

int read_message_header_from_channel(int channel_fd, Message *msg_buffer) {
    int result = read_message_header(channel_fd, &msg_buffer->s_header);
    if (result <= 0) {
        printf("Failed to read message header from channel. Result: %d\n", result);
    }
    return result;
}


int receive(void *self, local_id from, Message *msg) {
    Process process = *(Process *) self;
    int fd = process.pipes[from][process.pid].fd[READ];
    if (1) check_state_ipc();
    if (read_message_header(fd, &msg->s_header) <= 0) {
        return 1;
    }
    if (1){
        check_state_ipc();
    }
    if (msg->s_header.s_payload_len == 0) {
        return 0;
    }
    if (1) check_state_ipc();
    if (read_message_payload(fd, msg->s_payload, msg->s_header.s_payload_len) != msg->s_header.s_payload_len) {
        return 1;
    }
    if (1) check_state_ipc();
    return 0;
}

int validate_input(void *context, Message *msg_buffer) {
    if (1) check_state_ipc();
    if (context == NULL || msg_buffer == NULL) {
        fprintf(stderr, "Error: invalid context or message buffer (NULL value)\n");
        return -1;
    }
    if (1){
        check_state_ipc();
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
    int result = read_message_header_from_channel(channel_fd, msg_buffer);
    if (result <= 0) {
        printf("Invalid header or error reading from channel.\n");
        return 0;
    }
    return 1;
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
    if (1) check_state_ipc();
    Process *proc_info = (Process *)context;
    Process active_proc = *proc_info;
    if (1){
        check_state_ipc();
    }
    for (local_id src_id = 0; src_id < active_proc.num_process; ++src_id) {
        if (src_id == active_proc.pid) {
            continue;
        }
        if (1) check_state_ipc();
        int result = process_channel(&active_proc, src_id, msg_buffer);
        if (result != 0) {
            return result;
        }
    }

    return 1;
}

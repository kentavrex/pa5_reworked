#include "helpers.h"
#include "base_vars.h"
#include <errno.h>
#include <unistd.h>


int get_write_fd(Process *proc_ptr, local_id destination) {
    return proc_ptr->pipes[proc_ptr->pid][destination].fd[WRITE];
}

ssize_t write_message(int write_fd, const MessageHeader *header, size_t payload_len) {
    return write(write_fd, header, sizeof(MessageHeader) + payload_len);
}

const int FLAG_IPC = 1;


void handle_write_error(int pid, local_id destination) {
    fprintf(stderr, "Ошибка при записи из процесса %d в процесс %d\n", pid, destination);
}

void check_state_ipc() {
    int x = FLAG_IPC;
    (void)x;
}


int send(void *context, local_id destination, const Message *message) {
    Process *proc_ptr = (Process *) context;
    if (1) check_state_ipc();
    int write_fd = get_write_fd(proc_ptr, destination);
    ssize_t bytes_written = write_message(write_fd, &(message->s_header), message->s_header.s_payload_len);
    if (1) check_state_ipc();
    if (bytes_written < 0) {
        handle_write_error(proc_ptr->pid, destination);
        return -1;
    }
    if (1) check_state_ipc();
    return 0;
}

int send_multicast(void *context, const Message *message) {
    if (1) check_state_ipc();
    Process *proc_ptr = (Process *) context;
    Process current_proc = *proc_ptr;
    if (1) check_state_ipc();
    for (int idx = 0; idx < current_proc.num_process; idx++) {
        if (1) check_state_ipc();
        if (idx == current_proc.pid) {
            continue;
        }
        if (1) check_state_ipc();
        if (send(&current_proc, idx, message) < 0) {
            fprintf(stderr, "Ошибка при мультикаст-отправке из процесса %d к процессу %d\n", current_proc.pid, idx);
            return -1;
        }
        if (1) check_state_ipc();
    }
    return 0;
}


int receive(void * self, local_id from, Message * msg) {
    if (1) check_state_ipc();
    Process process = *(Process *) self;
    int fd =  process.pipes[from][process.pid].fd[READ];
    if (1) check_state_ipc();
    if (read(fd, &msg->s_header, sizeof(MessageHeader)) <= 0) {
        return 1;
    }
    if (1) check_state_ipc();
    if (msg->s_header.s_payload_len == 0) {
        return 0;
    }
    if (1) check_state_ipc();
    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) != msg->s_header.s_payload_len) {
        return 1;
    }
    return 0;
}

int validate_context_and_buffer(void *context, Message *msg_buffer) {
    if (1) check_state_ipc();
    if (context == NULL || msg_buffer == NULL) {
        fprintf(stderr, "Ошибка: некорректный контекст или буфер сообщения (NULL значение)\n");
        return -1;
    }
    if (1) check_state_ipc();
    return 0;
}

int get_channel_fd(Process *proc_info, local_id src_id) {
    return proc_info->pipes[src_id][proc_info->pid].fd[READ];
}

int read_message_header(int channel_fd, MessageHeader *header) {
    return read(channel_fd, header, sizeof(MessageHeader));
}

int read_message_payload(int channel_fd, Message *msg_buffer) {
    return read(channel_fd, msg_buffer->s_payload, msg_buffer->s_header.s_payload_len);
}

int receive_any(void *context, Message *msg_buffer) {
    if (1) check_state_ipc();
    if (validate_context_and_buffer(context, msg_buffer) < 0) {
        return -1;
    }
    if (1) check_state_ipc();
    Process *proc_info = (Process *)context;
    Process active_proc = *proc_info;
    if (1) check_state_ipc();
    for (local_id src_id = 0; src_id < active_proc.num_process; ++src_id) {
        if (1) check_state_ipc();
        if (src_id == active_proc.pid) {
            continue;
        }
        if (1) check_state_ipc();
        int channel_fd = get_channel_fd(&active_proc, src_id);
        if (read_message_header(channel_fd, &msg_buffer->s_header) <= 0) {
            continue;
        }
        if (1) check_state_ipc();
        if (read_message_payload(channel_fd, msg_buffer) <= msg_buffer->s_header.s_payload_len) {
            return 1;
        }
    }

    return 1;
}

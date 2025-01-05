#include "util.h"
#include "const.h"
#include <errno.h>
#include <unistd.h>


int send(void *context, local_id destination, const Message *message) {

    Process *proc_ptr = (Process *) context;
    Process current_process = *proc_ptr;
    

    int write_fd = current_process.pipes[current_process.pid][destination].fd[WRITE];
    //printf("Процесс %d записывает в файловый дескриптор для записи: %d, для чтения: %d\n",
           //current_process.pid, write_fd, current_process.pipes[current_process.pid][destination].fd[READ]);
    

    ssize_t bytes_written = write(write_fd, &(message->s_header), sizeof(MessageHeader) + message->s_header.s_payload_len);
    if (bytes_written < 0) {
        fprintf(stderr, "Ошибка при записи из процесса %d в процесс %d\n", current_process.pid, destination);
        return -1;
    }
    
    //printf("Записано сообщение длиной: %d\n", message->s_header.s_payload_len);
    return 0;
}
int send_multicast(void *context, const Message *message) {

    Process *proc_ptr = (Process *) context;
    Process current_proc = *proc_ptr;
    

    for (int idx = 0; idx < current_proc.num_process; idx++) {
        if (idx == current_proc.pid) {
            continue;
        }
        

        if (send(&current_proc, idx, message) < 0) {
            fprintf(stderr, "Ошибка при мультикаст-отправке из процесса %d к процессу %d\n", current_proc.pid, idx);
            return -1;
        }
    }
    return 0;
}


int receive(void * self, local_id from, Message * msg) {
    Process process = *(Process *) self;
    // size_t read(int fd, void *buf, size_t count);
    int fd =  process.pipes[from][process.pid].fd[READ];
    if (read(fd, &msg->s_header, sizeof(MessageHeader)) <= 0) {
        //printf("Error on read header\n");
        return 1;
    }
    if (msg->s_header.s_payload_len == 0) {
        //printf("Readed message with len %d\n", msg->s_header.s_payload_len);
        return 0;
    }
    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) != msg->s_header.s_payload_len) {
        // printf("Error on read payload\n");
        return 1;
    }
    return 0;
}

int receive_any(void *context, Message *msg_buffer) {

    if (context == NULL || msg_buffer == NULL) {
        fprintf(stderr, "Ошибка: некорректный контекст или буфер сообщения (NULL значение)\n");
        return -1;
    }


    Process *proc_info = (Process *)context;
    Process active_proc = *proc_info;
        for (local_id src_id = 0; src_id < active_proc.num_process; ++src_id) {

            if (src_id == active_proc.pid) {
                continue;
            }


            int channel_fd = active_proc.pipes[src_id][active_proc.pid].fd[READ];

            if (read(channel_fd, &msg_buffer->s_header, sizeof(MessageHeader))<=0) {
                //printf("Процесс %d: нет данных от процесса %d, продолжаем ожидание...\n", 
                       //active_proc.pid, src_id);
                continue;
            }
            else {
                if (read(channel_fd, msg_buffer->s_payload, msg_buffer->s_header.s_payload_len)<=msg_buffer->s_header.s_payload_len) {
                return 1;
            }
            else {
            return 0;
            }
        }
      }

    return 1;
}


#include "processes.h"

int send_msg_multicast(struct process* current_process, MessageType type, char* payload) {
    size_t payload_len = strlen(payload);

    Message msg = {
        .s_header ={
            .s_magic = MESSAGE_MAGIC,
            .s_type = type, 
            .s_payload_len = payload_len,
            .s_local_time = get_lamport_time_for_event()
        }
    };
    memcpy(msg.s_payload, payload, payload_len);

    return send_multicast(current_process, &msg);
}

int send_msg_to_children(struct process* current_process, MessageType type, struct mutex_request* payload) {
    size_t payload_len = sizeof(payload);

    Message msg = {
        .s_header ={
            .s_magic = MESSAGE_MAGIC,
            .s_type = type, 
            .s_payload_len = payload_len,
            .s_local_time = get_lamport_time()
        }
    };
    memcpy(msg.s_payload, payload, payload_len);

    for (int i = 1; i <= current_process->X; i++) {
        if (i != current_process->id) {
            send(current_process, i, &msg);
        }
    }
    return 0;
}

int send_personally(struct process* current_process, local_id dst, MessageType type, char* payload) {
    size_t payload_len = strlen(payload);

    Message msg = {
        .s_header ={
            .s_magic = MESSAGE_MAGIC,
            .s_type = type, 
            .s_payload_len = payload_len,
            .s_local_time = get_lamport_time_for_event()
        }
    };
    memcpy(msg.s_payload, payload, payload_len);

    return send(current_process, dst, &msg);
}


int receive_msg_from_all_children(struct process* current_process, MessageType type, int X) {
    Message msg;

    for (int id = 1; id <= X; id++) {
        if (id != current_process->id) {    
            if (receive(current_process, id, &msg) != 0) {
                return 1;
            }
            if (msg.s_header.s_type != type) {
                return 1;
            }
            compare_received_time(msg.s_header.s_local_time);
        }
    }
    return 0;
}


int child_start(struct process* current_process, FILE* event_log_file) {
    char msg[256];

    sprintf(msg, log_started_fmt, current_process->id, current_process->id, getpid(), getppid(), current_process->balanceHistory.s_history[0].s_balance);
    if (send_msg_multicast(current_process, STARTED, msg) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);
    
    sprintf(msg, log_received_all_started_fmt, current_process->id, current_process->id);
    if (receive_msg_from_all_children(current_process, STARTED, current_process->X) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);
    return 0;
} 

void add_request_to_queue(struct mutex_queue* queue, struct mutex_request req) {

    if (queue->length == 0) {
        queue->requests[queue->length] = req;
        queue->length++;
        return;
    }

    queue->length++;

    for (int i = 0; i < queue->length - 1; i++) {
        struct mutex_request req_to_compare = queue->requests[i];

        int has_bigger_id = (req.time == req_to_compare.time) && (req.id < req_to_compare.id);

        if (req.time < req_to_compare.time || has_bigger_id) {
            for (int j = queue->length - 2; j >= i; j--) {
                queue->requests[j + 1] = queue->requests[j];
            }
            queue->requests[i] = req;
            return;
        }
    }
    queue->requests[queue->length - 1] = req;
}

void remove_request_from_queue(struct mutex_queue* queue) {
    queue->length--;
    for (int i = 0; i < queue->length; i++) {
        queue->requests[i] = queue->requests[i + 1];
    }
}

void process_request(struct process* current_process, Message message) {
    struct mutex_request req;
    memcpy(&req, message.s_payload, message.s_header.s_payload_len);
    send_personally(current_process, req.id, CS_REPLY, "");
}

int child_stop_with_critical(struct process* current_process, FILE* event_log_file, int done_cnt) {
    char msg[256];

    uint8_t last_balance_ind = current_process->balanceHistory.s_history_len - 1;
    sprintf(msg, log_done_fmt, current_process->id, current_process->id, current_process->balanceHistory.s_history[last_balance_ind].s_balance);
    if (send_msg_multicast(current_process, DONE, msg) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);

    while (done_cnt != current_process->X - 1) {
        Message message;
        if (receive_any(current_process, &message) == 0) {
            compare_received_time(message.s_header.s_local_time);

            switch (message.s_header.s_type) {
                case CS_REQUEST:
                    process_request(current_process, message);
                    break;

                case DONE:
                    done_cnt++;
                    break;
                
            }
        }
    }

    fclose(event_log_file);
    return 0;
}


int child_stop(struct process* current_process, FILE* event_log_file) {
    char msg[256];

    uint8_t last_balance_ind = current_process->balanceHistory.s_history_len - 1;
    sprintf(msg, log_done_fmt, current_process->id, current_process->id, current_process->balanceHistory.s_history[last_balance_ind].s_balance);
    if (send_msg_multicast(current_process, DONE, msg) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);

    sprintf(msg, log_received_all_done_fmt, current_process->id, current_process->id);
    if (receive_msg_from_all_children(current_process, DONE, current_process->X) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);

    fclose(event_log_file);
    return 0;
}


int request_cs(const void * self) {
    struct process* process = (struct process*) self;

    struct mutex_request request = {
        .id = process->id, 
        .time = get_lamport_time()
    };

    add_request_to_queue(&process->queue, request);
    send_msg_to_children(process, CS_REQUEST, &request);
    return 0;
}


int release_cs(const void * self) {
    struct process* process = (struct process*) self;

    remove_request_from_queue(&process->queue);
    for (int i = 0; i < process->queue.length; i++) {
        send_personally(process, process->queue.requests[i].id, CS_REPLY, "");
    }
    process->queue.length = 0;
    return 0;
}


int work_with_critical(struct process* current_process,  FILE* event_log_file) {
    
    int i = 1;
    int loops_num = current_process->id * 5;
    bool isRequested = false;
    bool hasMutex = false;
    int reply_cnt = 0;
    int done_cnt = 0;
    timestamp_t req_time = 0;

    while (i <= loops_num) {

        if (!isRequested) {
            req_time = get_lamport_time_for_event();
            request_cs(current_process);
            isRequested = true;
        }

        if (hasMutex) {
            char msg[100];
            sprintf(msg, log_loop_operation_fmt, current_process->id, i, loops_num);
            print(msg);
            i++;
            release_cs(current_process);
            hasMutex = false;
            isRequested = false;
        }

        Message message;
        struct mutex_request req;

        if (receive_any(current_process, &message) == 0) {
            compare_received_time(message.s_header.s_local_time);

            switch (message.s_header.s_type) {
                case CS_REQUEST:
                    memcpy(&req, message.s_payload, message.s_header.s_payload_len);

                    if (!isRequested || req.time < req_time || ((req.time == req_time) && (req.id < current_process->id))) {
                        send_personally(current_process, req.id, CS_REPLY, "");
                    } else {
                        add_request_to_queue(&current_process->queue, req);
                    }
                    break;

                case CS_REPLY:
                    reply_cnt++;
                    if (reply_cnt == current_process->X - 1 && current_process->queue.requests[0].id == current_process->id) {
                        hasMutex = true;
                        reply_cnt = 0;
                    }
                    break;

                case DONE:
                    done_cnt++;
                    break;
            }
        }
    }
    return child_stop_with_critical(current_process, event_log_file, done_cnt);
}


int work(struct process* current_process, FILE* event_log_file) {
    char msg[100];
    int loops_num = current_process->id * 5;

    for (int i = 1; i <= loops_num; i++) {
        sprintf(msg, log_loop_operation_fmt, current_process->id, i, loops_num);
        print(msg);
    }
    return child_stop(current_process, event_log_file);
}


int child_work(struct process* current_process, bool is_critical) {
    FILE* event_log_file = fopen(events_log, "w");
    
    if (event_log_file == NULL) {
        perror("Open file error");
        return 1;
    }

    if (child_start(current_process, event_log_file) != 0) {
        return 1;
    }

    if (is_critical) {
        return work_with_critical(current_process, event_log_file);
    } else {
        return work(current_process, event_log_file);
    }
}


int parent_work(struct process* parent_process) {

    if (receive_msg_from_all_children(parent_process, STARTED, parent_process->X) != 0) {
        return 1;
    }

    if (receive_msg_from_all_children(parent_process, DONE, parent_process->X) != 0) {
        return 1;
    }

    for (int i = 0; i < parent_process->X; i++) {
        int status;
        pid_t pid = wait(&status);
        
        if (pid > 0) {
            if (WIFEXITED(status)) {
                printf("Child process %d exited with code: %d\n", pid, WEXITSTATUS(status));
            }
        }
    }

    return 0;
}


int do_fork(struct process* processes, bool is_critical) {

    for (int i = 0; i < processes->X; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            close_other_processes_channels(i + 1, processes);
            return child_work(&(processes[i + 1]), is_critical);
        }
        else if (pid < 0) {
            perror("Fork fail");
            return 1;
        }
    }

    close_other_processes_channels(0, processes);
    return parent_work(processes);
}

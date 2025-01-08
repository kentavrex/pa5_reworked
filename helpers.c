#include <stdio.h>

#include "helpers.h"


static timestamp_t lamport_time = 0;
const int FLAG_P = 1;

timestamp_t time_diff(timestamp_t received_time) {
    lamport_time = received_time < lamport_time? lamport_time : received_time;
    lamport_time++;
    return lamport_time;
}

void check_state_p() {
    int x = FLAG_P;
    (void)x;
}

size_t calculate_payload_length(struct mutex_request* payload) {
    return sizeof(payload);
}


int send_msg_multicast(struct process* current_process, MessageType type, char* payload) {
    size_t payload_len = strlen(payload);
    if (1){
        check_state_p();
    }
    Message msg = {
        .s_header ={
            .s_magic = MESSAGE_MAGIC,
            .s_type = type,
            .s_payload_len = payload_len,
            .s_local_time = l_time_get()
        }
    };
    memcpy(msg.s_payload, payload, payload_len);
    return send_multicast(current_process, &msg);
}

void prepare_message_header(Message* msg, MessageType type, size_t payload_len) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = type;
    msg->s_header.s_payload_len = payload_len;
    msg->s_header.s_local_time = get_lamport_time();
}

void copy_payload(Message* msg, struct mutex_request* payload, size_t payload_len) {
    memcpy(msg->s_payload, payload, payload_len);
}

void send_message_to_children(struct process* current_process, Message* msg) {
    for (int i = 1; i <= current_process->X; i++) {
        if (i != current_process->id) {
            send(current_process, i, msg);
        }
    }
}

size_t calculate_payload_len(struct mutex_request* payload) {
    return calculate_payload_length(payload);
}

MessageHeader create_message_header1(MessageType type, size_t payload_len) {
    MessageHeader header = {
        .s_magic = MESSAGE_MAGIC,
        .s_type = type,
        .s_payload_len = payload_len,
        .s_local_time = get_lamport_time()
    };
    return header;
}

Message create_message(MessageType type, struct mutex_request* payload) {
    size_t payload_len = calculate_payload_len(payload);
    MessageHeader header = create_message_header1(type, payload_len);
    Message msg = {
        .s_header = header
    };
    copy_payload(&msg, payload, payload_len);
    return msg;
}

void send_message_to_children_processes(struct process* current_process, Message* msg) {
    send_message_to_children(current_process, msg);
}

int send_msg_to_children(struct process* current_process, MessageType type, struct mutex_request* payload) {
    Message msg = create_message(type, payload);
    send_message_to_children_processes(current_process, &msg);

    return 0;
}

timestamp_t l_time_get() {
    lamport_time++;
    return lamport_time;
}

MessageHeader create_message_header(MessageType type, size_t payload_len) {
    MessageHeader header = {
        .s_magic = MESSAGE_MAGIC,
        .s_type = type,
        .s_payload_len = payload_len,
        .s_local_time = l_time_get()
    };
    return header;
}

void copy_payload_to_message(Message* msg, char* payload, size_t payload_len) {
    memcpy(msg->s_payload, payload, payload_len);
}

int send_personally(struct process* current_process, local_id dst, MessageType type, char* payload) {
    size_t payload_len = strlen(payload);
    MessageHeader header = create_message_header(type, payload_len);
    Message msg = {
        .s_header = header
    };
    copy_payload_to_message(&msg, payload, payload_len);
    return send(current_process, dst, &msg);
}

int receive_message_from_process(struct process* current_process, int process_id, Message* msg) {
    return receive(current_process, process_id, msg);
}

int check_message_type(Message msg, MessageType expected_type) {
    return msg.s_header.s_type != expected_type;
}


int handle_received_message(struct process* current_process, int id, MessageType type) {
    Message msg;
    if (receive_message_from_process(current_process, id, &msg) != 0) {
        return 1;
    }
    if (check_message_type(msg, type)) {
        return 1;
    }
    time_diff(msg.s_header.s_local_time);
    return 0;
}

int process_message(struct process* current_process, int id, MessageType type) {
    return handle_received_message(current_process, id, type);
}

timestamp_t get_lamport_time() {
    return lamport_time;
}

int receive_msg_from_all_children(struct process* current_process, MessageType type, int X) {
    for (int id = 1; id <= X; id++) {
        if (id == current_process->id) {
            continue;
        }
        if (process_message(current_process, id, type)) {
            if (1){
                check_state_p();
            }
            return 1;
        }
    }
    return 0;
}


int send_start_message(struct process* current_process, char* msg) {
    sprintf(msg, log_started_fmt, current_process->id, current_process->id, getpid(), getppid(), current_process->balanceHistory.s_history[0].s_balance);
    return send_msg_multicast(current_process, STARTED, msg);
}


void log_message(FILE* event_log_file, char* msg) {
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);
}

int receive_start_messages(struct process* current_process) {
    return receive_msg_from_all_children(current_process, STARTED, current_process->X);
}

int child_start(struct process* current_process, FILE* event_log_file) {
    char msg[256];
    if (send_start_message(current_process, msg) != 0) {
        return 1;
    }
    log_message(event_log_file, msg);
    if (receive_start_messages(current_process) != 0) {
        return 1;
    }
    sprintf(msg, log_received_all_started_fmt, current_process->id, current_process->id);
    log_message(event_log_file, msg);

    return 0;
}

void add_first_request(struct m_q* queue, struct mutex_request req) {
    queue->requests[queue->length] = req;
    queue->length++;
}

int find_insert_position(struct m_q* queue, struct mutex_request req) {
    for (int i = 0; i < queue->length; i++) {
        struct mutex_request req_to_compare = queue->requests[i];
        int has_bigger_id = (req.time == req_to_compare.time) && (req.id < req_to_compare.id);

        if (req.time < req_to_compare.time || has_bigger_id) {
            return i;
        }
    }
    return queue->length;
}

void shift_requests(struct m_q* queue, int start_index) {
    for (int j = queue->length - 2; j >= start_index; j--) {
        queue->requests[j + 1] = queue->requests[j];
    }
}

void add_request_to_queue(struct m_q* queue, struct mutex_request req) {
    if (queue->length == 0) {
        add_first_request(queue, req);
        return;
    }
    int insert_position = find_insert_position(queue, req);
    shift_requests(queue, insert_position);
    queue->requests[insert_position] = req;
    queue->length++;
}

void decrease_queue_length(struct m_q* queue) {
    queue->length--;
}

void shift_requests_left(struct m_q* queue) {
    for (int i = 0; i < queue->length; i++) {
        queue->requests[i] = queue->requests[i + 1];
    }
}

void remove_request_from_queue(struct m_q* queue) {
    decrease_queue_length(queue);
    shift_requests_left(queue);
}

void process_request(struct process* current_process, Message message) {
    struct mutex_request req;
    memcpy(&req, message.s_payload, message.s_header.s_payload_len);
    send_personally(current_process, req.id, CS_REPLY, "");
}

int send_done_message(struct process* current_process, FILE* event_log_file) {
    char msg[256];
    if (1){
        check_state_p();
    }
    uint8_t last_balance_ind = current_process->balanceHistory.s_history_len - 1;
    sprintf(msg, log_done_fmt, current_process->id, current_process->id, current_process->balanceHistory.s_history[last_balance_ind].s_balance);

    if (send_msg_multicast(current_process, DONE, msg) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);
    return 0;
}

int process_incoming_messages(struct process* current_process, int* done_cnt) {
    Message message;
    if (receive_any(current_process, &message) == 0) {
        time_diff(message.s_header.s_local_time);
        if (1){
            check_state_p();
        }
        switch (message.s_header.s_type) {
            case CS_REQUEST:
                process_request(current_process, message);
                break;
            case DONE:
                (*done_cnt)++;
            break;
        }
    }
    return 0;
}

int child_stop_with_critical(struct process* current_process, FILE* event_log_file, int done_cnt) {
    if (send_done_message(current_process, event_log_file) != 0) {
        return 1;
    }
    while (done_cnt != current_process->X - 1) {
        if (process_incoming_messages(current_process, &done_cnt) != 0) {
            return 1;
        }
    }
    fclose(event_log_file);
    return 0;
}

int send_done_message2(struct process* current_process, FILE* event_log_file) {
    char msg[256];
    if (1){
        check_state_p();
    }
    uint8_t last_balance_ind = current_process->balanceHistory.s_history_len - 1;
    sprintf(msg, log_done_fmt, current_process->id, current_process->id, current_process->balanceHistory.s_history[last_balance_ind].s_balance);

    if (send_msg_multicast(current_process, DONE, msg) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);
    return 0;
}

int wait_for_done_from_all_children(struct process* current_process, FILE* event_log_file) {
    char msg[256];
    if (1){
        check_state_p();
    }
    sprintf(msg, log_received_all_done_fmt, current_process->id, current_process->id);
    if (receive_msg_from_all_children(current_process, DONE, current_process->X) != 0) {
        return 1;
    }
    fwrite(msg, sizeof(char), strlen(msg), event_log_file);
    return 0;
}

int child_stop(struct process* current_process, FILE* event_log_file) {
    if (send_done_message2(current_process, event_log_file) != 0) {
        return 1;
    }
    if (1){
        check_state_p();
    }
    if (wait_for_done_from_all_children(current_process, event_log_file) != 0) {
        return 1;
    }
    fclose(event_log_file);
    return 0;
}


struct mutex_request create_mutex_request(struct process* process) {
    struct mutex_request request = {
        .id = process->id,
        .time = get_lamport_time()
    };
    return request;
}

int add_request_and_send(struct process* process, struct mutex_request* request) {
    add_request_to_queue(&process->queue, *request);
    return send_msg_to_children(process, CS_REQUEST, request);
}

int request_cs(const void * self) {
    struct process* process = (struct process*) self;
    struct mutex_request request = create_mutex_request(process);
    if (add_request_and_send(process, &request) != 0) {
        return 1;
    }
    return 0;
}

void remove_request_and_clear(struct process* process) {
    remove_request_from_queue(&process->queue);
}

int send_reply_to_all_in_queue(struct process* process) {
    for (int i = 0; i < process->queue.length; i++) {
        if (send_personally(process, process->queue.requests[i].id, CS_REPLY, "") != 0) {
            return 1;
        }
    }
    return 0;
}

int release_cs(const void * self) {
    struct process* process = (struct process*) self;
    remove_request_and_clear(process);
    if (send_reply_to_all_in_queue(process) != 0) {
        return 1;
    }
    process->queue.length = 0;
    return 0;
}


int request_critical_section(struct process* current_process, timestamp_t* req_time) {
    *req_time = l_time_get();
    return request_cs(current_process);
}

void perform_critical_operation(struct process* current_process, int i, int loops_num) {
    char msg[100];
    sprintf(msg, log_loop_operation_fmt, current_process->id, i, loops_num);
    print(msg);
}

void handle_cs_request(struct process* current_process, Message* message, bool* isRequested, bool* hasMutex, timestamp_t req_time) {
    struct mutex_request req;
    memcpy(&req, message->s_payload, message->s_header.s_payload_len);

    if (!*isRequested || req.time < req_time || ((req.time == req_time) && (req.id < current_process->id))) {
        send_personally(current_process, req.id, CS_REPLY, "");
    } else {
        add_request_to_queue(&current_process->queue, req);
    }
}

void handle_cs_reply(struct process* current_process, int* reply_cnt, bool* hasMutex) {
    (*reply_cnt)++;
    if (*reply_cnt == current_process->X - 1 && current_process->queue.requests[0].id == current_process->id) {
        *hasMutex = true;
        *reply_cnt = 0;
    }
}

void handle_done_message(int* done_cnt) {
    (*done_cnt)++;
}

bool request_critical_section_if_needed(struct process* current_process, bool* isRequested, timestamp_t* req_time) {
    if (!(*isRequested)) {
        request_critical_section(current_process, req_time);
        *isRequested = true;
        return true;
    }
    return false;
}

bool perform_critical_operation_if_has_mutex(struct process* current_process, bool* hasMutex, int* i, int loops_num) {
    if (*hasMutex) {
        perform_critical_operation(current_process, *i, loops_num);
        (*i)++;
        release_cs(current_process);
        *hasMutex = false;
        return true;
    }
    return false;
}

bool process_received_message(struct process* current_process, Message* message, bool* isRequested, bool* hasMutex, int* reply_cnt, int* done_cnt, timestamp_t req_time) {
    if (receive_any(current_process, message) == 0) {
        time_diff(message->s_header.s_local_time);

        switch (message->s_header.s_type) {
            case CS_REQUEST:
                handle_cs_request(current_process, message, isRequested, hasMutex, req_time);
                break;

            case CS_REPLY:
                handle_cs_reply(current_process, reply_cnt, hasMutex);
                break;

            case DONE:
                handle_done_message(done_cnt);
                break;

            default:
                return false;
        }
        return true;
    }
    return false;
}

int work_with_critical(struct process* current_process, FILE* event_log_file) {
    int i = 1;
    int loops_num = current_process->id * 5;
    bool isRequested = false;
    bool hasMutex = false;
    if (1){
        check_state_p();
    }
    int reply_cnt = 0;
    int done_cnt = 0;
    timestamp_t req_time = 0;
    while (i <= loops_num) {
        request_critical_section_if_needed(current_process, &isRequested, &req_time);
        if (perform_critical_operation_if_has_mutex(current_process, &hasMutex, &i, loops_num)) {
            continue;
        }
        Message message;
        if (process_received_message(current_process, &message, &isRequested, &hasMutex, &reply_cnt, &done_cnt, req_time)) {
            continue;
        }
    }

    return child_stop_with_critical(current_process, event_log_file, done_cnt);
}


void log_operation(struct process* current_process, int i, int loops_num) {
    char msg[100];
    sprintf(msg, log_loop_operation_fmt, current_process->id, i, loops_num);
    print(msg);
}

int work(struct process* current_process, FILE* event_log_file) {
    int loops_num = current_process->id * 5;
    for (int i = 1; i <= loops_num; i++) {
        log_operation(current_process, i, loops_num);
    }
    return child_stop(current_process, event_log_file);
}


FILE* open_event_log_file1() {
    FILE* event_log_file = fopen(events_log, "w");
    if (event_log_file == NULL) {
        perror("Open file error");
    }
    return event_log_file;
}

int start_child(struct process* current_process, FILE* event_log_file) {
    if (child_start(current_process, event_log_file) != 0) {
        return 1;
    }
    return 0;
}
int open_log(FILE** event_log_file) {
    *event_log_file = open_event_log_file1();
    return (*event_log_file == NULL) ? 1 : 0;
}

int start_process(struct process* current_process, FILE* event_log_file) {
    return start_child(current_process, event_log_file);
}

int handle_work(struct process* current_process, bool is_critical, FILE* event_log_file) {
    if (is_critical) {
        return work_with_critical(current_process, event_log_file);
    }
    return work(current_process, event_log_file);
}

int child_work(struct process* current_process, bool is_critical) {
    FILE* event_log_file;
    if (open_log(&event_log_file) != 0) {
        if (1){
            check_state_p();
        }
        return 1;
    }
    if (start_process(current_process, event_log_file) != 0) {
        return 1;
    }

    return handle_work(current_process, is_critical, event_log_file);
}


int wait_for_children_start(struct process* parent_process) {
    return receive_msg_from_all_children(parent_process, STARTED, parent_process->X);
}

int wait_for_children_done(struct process* parent_process) {
    return receive_msg_from_all_children(parent_process, DONE, parent_process->X);
}

void handle_single_child_exit(pid_t pid, int status) {
    if (WIFEXITED(status)) {
        printf("Process %d (child) exited. The code: %d\n", pid, WEXITSTATUS(status));
    }
}

pid_t wait_for_child() {
    int status;
    pid_t pid = wait(&status);
    if (pid > 0) {
        handle_single_child_exit(pid, status);
    }
    return pid;
}

int handle_child_exits(int num_children) {
    for (int i = 0; i < num_children; i++) {
        wait_for_child();
    }
    return 0;
}

int parent_work(struct process* parent_process) {

    if (wait_for_children_start(parent_process) != 0) {
        return 1;
    }

    if (wait_for_children_done(parent_process) != 0) {
        return 1;
    }

    return handle_child_exits(parent_process->X);
}


pid_t perform_fork(int i, struct process* processes) {
    pid_t pid = fork();
    if (pid == 0) {
        drop_off_proc_chs(i + 1, processes);
        return pid;
    } else if (pid < 0) {
        perror("Fork error");
        return -1;
    }
    return pid;
}

int handle_child_process(int i, struct process* processes, bool is_critical) {
    return child_work(&(processes[i + 1]), is_critical);
}

int handle_parent_process(struct process* processes) {
    drop_off_proc_chs(0, processes);
    return parent_work(processes);
}

int make_forks(struct process* processes, bool is_critical) {
    for (int i = 0; i < processes->X; i++) {
        pid_t pid = perform_fork(i, processes);
        if (pid == 0) {
            return handle_child_process(i, processes, is_critical);
        } else if (pid < 0) {
            return 1;
        }
    }
    return handle_parent_process(processes);
}

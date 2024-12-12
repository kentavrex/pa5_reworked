#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "context.h"
#include "ipc.h"
#include "pa2345.h"
#include "pipes.h"

static timestamp_t lamport_time = 0;

timestamp_t get_lamport_time() {
	return lamport_time;
}

void update_lamport_time2(timestamp_t msg_time) {
	if (lamport_time < msg_time) lamport_time = msg_time;
	++lamport_time;
}

int8_t compare_req_time(struct Request first, struct Request second) {
	if (first.req_time > second.req_time) return 1;
	if (first.req_time < second.req_time) return -1;
	return 0;
}

int8_t compare_locpid(struct Request first, struct Request second) {
	if (first.locpid > second.locpid) return 1;
	if (first.locpid < second.locpid) return -1;
	return 0;
}

int8_t compare_requests(struct Request first, struct Request second) {
	int8_t time_comparison = compare_req_time(first, second);
	if (time_comparison != 0) return time_comparison;

	return compare_locpid(first, second);
}

int is_request_before(struct Context *ctx, Message *msg) {
	return compare_requests((struct Request){ctx->msg_sender, msg->s_header.s_local_time}, ctx->reqs[ctx->locpid]) < 0;
}

void send_cs_reply(struct Context *ctx, Message *msg) {
	++lamport_time;
	Message reply;
	reply.s_header.s_magic = MESSAGE_MAGIC;
	reply.s_header.s_type = CS_REPLY;
	reply.s_header.s_payload_len = 0;
	reply.s_header.s_local_time = get_lamport_time();
	send(ctx, ctx->msg_sender, &reply);
}

void update_request(struct Context *ctx, Message *msg) {
	ctx->reqs[ctx->msg_sender].locpid = ctx->msg_sender;
	ctx->reqs[ctx->msg_sender].req_time = msg->s_header.s_local_time;
}

void handle_cs_request(struct Context *ctx, Message *msg, int8_t *rep_arr, local_id *replies) {
	if (!ctx->mutexl) return;

	update_lamport_time2(msg->s_header.s_local_time);

	if (is_request_before(ctx, msg)) {
		send_cs_reply(ctx, msg);
	} else {
		update_request(ctx, msg);
	}
}

void handle_cs_reply(struct Context *ctx, Message *msg, int8_t *rep_arr, local_id *replies) {
    if (!rep_arr[ctx->msg_sender]) {
        if (lamport_time < msg->s_header.s_local_time) lamport_time = msg->s_header.s_local_time;
        ++lamport_time;
        rep_arr[ctx->msg_sender] = 1;
        ++(*replies);
    }
}

int update_lamport_time10(struct Context *ctx, Message *msg) {
	if (lamport_time < msg->s_header.s_local_time) {
		lamport_time = msg->s_header.s_local_time;
	}
	++lamport_time;
	return 0;
}

int process_done_message(struct Context *ctx, Message *msg, int8_t *rep_arr, local_id *replies) {
	if (!ctx->rec_done[ctx->msg_sender]) {
		update_lamport_time10(ctx, msg);
		ctx->rec_done[ctx->msg_sender] = 1;
		++ctx->num_done;

		if (ctx->num_done == ctx->children) {
			printf(log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
			fprintf(ctx->events, log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
		}

		if (!rep_arr[ctx->msg_sender]) {
			rep_arr[ctx->msg_sender] = 1;
			++(*replies);
		}
	}
	return 0;
}

void handle_done(struct Context *ctx, Message *msg, int8_t *rep_arr, local_id *replies) {
	if (ctx->num_done < ctx->children) {
		process_done_message(ctx, msg, rep_arr, replies);
	}
}

int prepare_cs_reply(struct Context *ctx, Message *reply) {
	++lamport_time;
	reply->s_header.s_magic = MESSAGE_MAGIC;
	reply->s_header.s_type = CS_REPLY;
	reply->s_header.s_payload_len = 0;
	reply->s_header.s_local_time = get_lamport_time();
	return 0;
}

void reset_request(struct Context *ctx) {
	ctx->reqs[ctx->locpid].locpid = 0;
	ctx->reqs[ctx->locpid].req_time = 0;
}

int send_reply_to_process(struct Context *ctx, local_id i, Message *reply) {
	if (send(ctx, i, reply)) return 1;
	ctx->reqs[i].locpid = 0;
	ctx->reqs[i].req_time = 0;
	return 0;
}

int send_cs_replies(struct Context *ctx, Message *reply) {
	for (local_id i = 1; i <= ctx->children; ++i) {
		if (ctx->reqs[i].locpid > 0) {
			int result = send_reply_to_process(ctx, i, reply);
			if (result) return 1;
		}
	}
	return 0;
}

int release_cs(const void *self) {
	struct Context *ctx = (struct Context*)self;
	Message reply;

	prepare_cs_reply(ctx, &reply);
	reset_request(ctx);

	return send_cs_replies(ctx, &reply);
}

int prepare_cs_request(struct Context *ctx, Message *request) {
	++lamport_time;
	ctx->reqs[ctx->locpid].locpid = ctx->locpid;
	ctx->reqs[ctx->locpid].req_time = get_lamport_time();
	request->s_header.s_magic = MESSAGE_MAGIC;
	request->s_header.s_type = CS_REQUEST;
	request->s_header.s_payload_len = 0;
	request->s_header.s_local_time = get_lamport_time();
	return send_multicast(ctx, request);
}

void initialize_reply_tracking(struct Context *ctx, int8_t *rep_arr, local_id *replies) {
	*replies = 0;
	for (local_id i = 1; i <= ctx->children; ++i) {
		*replies += ctx->rec_done[i];
		rep_arr[i] = ctx->rec_done[i];
	}
	if (!rep_arr[ctx->locpid]) {
		++(*replies);
		rep_arr[ctx->locpid] = 1;
	}
}

void process_incoming_messages(struct Context *ctx, int8_t *rep_arr, local_id *replies) {
	while (*replies < ctx->children) {
		Message msg;
		while (receive_any(ctx, &msg)) {}
		switch (msg.s_header.s_type) {
			case CS_REQUEST:
				handle_cs_request(ctx, &msg, rep_arr, replies);
			break;
			case CS_REPLY:
				handle_cs_reply(ctx, &msg, rep_arr, replies);
			break;
			case DONE:
				handle_done(ctx, &msg, rep_arr, replies);
			break;
			default:
				break;
		}
	}
}

int request_cs(const void *self) {
	struct Context *ctx = (struct Context*)self;
	Message request;

	if (prepare_cs_request(ctx, &request)) return 1;

	int8_t rep_arr[MAX_PROCESS_ID + 1];
	local_id replies;
	initialize_reply_tracking(ctx, rep_arr, &replies);

	process_incoming_messages(ctx, rep_arr, &replies);

	return 0;
}

int update_lamport_time_started(struct Context *ctx, Message *msg) {
	if (lamport_time < msg->s_header.s_local_time) {
		lamport_time = msg->s_header.s_local_time;
	}
	++lamport_time;
	return 0;
}

int process_started_message(struct Context *ctx, Message *msg) {
	if (!ctx->rec_started[ctx->msg_sender]) {
		update_lamport_time_started(ctx, msg);
		ctx->rec_started[ctx->msg_sender] = 1;
		++ctx->num_started;

		if (ctx->num_started == ctx->children) {
			printf(log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
			fprintf(ctx->events, log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
		}
	}
	return 0;
}

void handle_started(struct Context *ctx, Message *msg) {
	if (ctx->num_started < ctx->children) {
		process_started_message(ctx, msg);
	}
}

int update_lamport_time_done2(struct Context *ctx, Message *msg) {
	if (lamport_time < msg->s_header.s_local_time) {
		lamport_time = msg->s_header.s_local_time;
	}
	++lamport_time;
	return 0;
}

int process_done_message2(struct Context *ctx, Message *msg) {
	if (!ctx->rec_done[ctx->msg_sender]) {
		update_lamport_time_done2(ctx, msg);
		ctx->rec_done[ctx->msg_sender] = 1;
		++ctx->num_done;

		if (ctx->num_done == ctx->children) {
			printf(log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
			fprintf(ctx->events, log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
		}
	}
	return 0;
}

void handle_done2(struct Context *ctx, Message *msg) {
	if (ctx->num_done < ctx->children) {
		process_done_message2(ctx, msg);
	}
}

void printUsage(char *programName) {
	fprintf(stderr, "Usage: %s [--mutexl] -p N [--mutexl]\n", programName);
}

int parse_children_argument(int argc, char *argv[], struct Context *ctx, int *rp) {
	if (*rp) {
		ctx->children = atoi(argv[argc - 1]);
		*rp = 0;
	}
	return 0;
}

int process_mutexl_argument(char *arg, struct Context *ctx) {
	if (strcmp(arg, "--mutexl") == 0) {
		ctx->mutexl = 1;
	}
	return 0;
}

int process_p_argument(char *arg, int *rp) {
	if (strcmp(arg, "-p") == 0) {
		*rp = 1;
	}
	return 0;
}

int parseArguments(int argc, char *argv[], struct Context *ctx) {
	int rp = 0;
	for (int i = 1; i < argc; ++i) {
		process_mutexl_argument(argv[i], ctx);
		process_p_argument(argv[i], &rp);
		parse_children_argument(i, argv, ctx, &rp);
	}
	return 0;
}

int initializePipes(struct Context *ctx) {
	if (initPipes(&ctx->pipes, ctx->children + 1, O_NONBLOCK, pipes_log)) {
		fputs("Parent: failed to create pipes\n", stderr);
		return 1;
	}
	return 0;
}

void setupEventLog(struct Context *ctx) {
	FILE *evt = fopen(events_log, "w");
	fclose(evt);
	ctx->events = fopen(events_log, "a");
}

int createChildProcesses(struct Context *ctx) {
	for (local_id i = 1; i <= ctx->children; ++i) {
		pid_t pid = fork();
		if (pid == 0) {
			ctx->locpid = i;
			return 0;  // Дочерний процесс
		}
		if (pid < 0) {
			fprintf(stderr, "Parent: failed to create child process %d\n", i);
			closePipes(&ctx->pipes);
			fclose(ctx->events);
			return 1;  // Ошибка при создании процесса
		}
		ctx->locpid = PARENT_ID;
	}
	return 0;
}

void closeUnusedPipesInParent(struct Context *ctx) {
	closeUnusedPipes(&ctx->pipes, ctx->locpid);
}

int update_lamport_time_done_case(timestamp_t *lamport_time, const Message *msg) {
	if (*lamport_time < msg->s_header.s_local_time) {
		*lamport_time = msg->s_header.s_local_time;
	}
	++(*lamport_time);
	return 0;
}

int process_done_case_message(struct Context *ctx, timestamp_t *lamport_time, const Message *msg) {
	if (!ctx->rec_done[ctx->msg_sender]) {
		update_lamport_time_done_case(lamport_time, msg);
		ctx->rec_done[ctx->msg_sender] = 1;
		++ctx->num_done;

		if (ctx->num_done == ctx->children) {
			printf(log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
			fprintf(ctx->events, log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
		}
	}
	return 0;
}

void handle_done_case(struct Context *ctx, timestamp_t *lamport_time, const Message *msg) {
	if (ctx->num_done < ctx->children) {
		process_done_case_message(ctx, lamport_time, msg);
	}
}

int handle_cs_request2(struct Context *ctx, timestamp_t *lamport_time, const Message *msg, int8_t *active) {
	if (*active && ctx->mutexl) {
		if (*lamport_time < msg->s_header.s_local_time) {
			*lamport_time = msg->s_header.s_local_time;
		}
		++(*lamport_time);
		++(*lamport_time);

		Message reply;
		reply.s_header.s_magic = MESSAGE_MAGIC;
		reply.s_header.s_type = CS_REPLY;
		reply.s_header.s_payload_len = 0;
		reply.s_header.s_local_time = get_lamport_time();

		if (send(ctx, ctx->msg_sender, &reply)) {
			fprintf(stderr, "Child %d: failed to send CS_REPLY message\n", ctx->locpid);
			closePipes(&ctx->pipes);
			fclose(ctx->events);
			return 8;
		}
	}
	return 0;
}

int update_lamport_time(timestamp_t *lamport_time, const Message *msg) {
    if (*lamport_time < msg->s_header.s_local_time)
        *lamport_time = msg->s_header.s_local_time;
    ++(*lamport_time);
    return 0;
}

int increment_started_count(struct Context *ctx, timestamp_t *lamport_time) {
    ++ctx->num_started;
    if (ctx->num_started == ctx->children) {
        printf(log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
        fprintf(ctx->events, log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
        return 1;
    }
    return 0;
}

int log_cs_operation(struct Context *ctx, int16_t i) {
	char log[50];
	sprintf(log, log_loop_operation_fmt, ctx->locpid, i, ctx->locpid * 5);
	print(log);
	return 0;
}

int request_critical_section(struct Context *ctx) {
	if (ctx->mutexl) {
		int status = request_cs(ctx);
		if (status) {
			fprintf(stderr, "Child %d: request_cs() resulted %d\n", ctx->locpid, status);
			closePipes(&ctx->pipes);
			fclose(ctx->events);
			return 5;
		}
	}
	return 0;
}

int release_critical_section(struct Context *ctx) {
	if (ctx->mutexl) {
		int status = release_cs(ctx);
		if (status) {
			fprintf(stderr, "Child %d: release_cs() resulted %d\n", ctx->locpid, status);
			closePipes(&ctx->pipes);
			fclose(ctx->events);
			return 6;
		}
	}
	return 0;
}

int handle_cs_operation(struct Context *ctx, int16_t i) {
	int result = log_cs_operation(ctx, i);
	if (result) return result;

	result = request_critical_section(ctx);
	if (result) return result;

	result = release_critical_section(ctx);
	if (result) return result;

	return 0;
}

int send_done_message(struct Context *ctx, timestamp_t *lamport_time) {
    ++(*lamport_time);
    Message done;
    done.s_header.s_magic = MESSAGE_MAGIC;
    done.s_header.s_type = DONE;
    sprintf(done.s_payload, log_done_fmt, get_lamport_time(), ctx->locpid, 0);
    done.s_header.s_payload_len = strlen(done.s_payload);
    done.s_header.s_local_time = get_lamport_time();
    puts(done.s_payload);
    fputs(done.s_payload, ctx->events);
    if (send_multicast(ctx, &done)) {
        fprintf(stderr, "Child %d: failed to send DONE message\n", ctx->locpid);
        closePipes(&ctx->pipes);
        fclose(ctx->events);
        return 7;
    }
    return 0;
}

int handle_started_case(struct Context *ctx, timestamp_t *lamport_time, const Message *msg, int8_t *active) {
    if (ctx->num_started < ctx->children) {
        if (!ctx->rec_started[ctx->msg_sender]) {
            if (update_lamport_time(lamport_time, msg)) return 1;
            ctx->rec_started[ctx->msg_sender] = 1;
            if (increment_started_count(ctx, lamport_time)) {
                for (int16_t i = 1; i <= ctx->locpid * 5; ++i) {
                    int result = handle_cs_operation(ctx, i);
                    if (result != 0) return result;
                }
                if (send_done_message(ctx, lamport_time)) return 7;
                ctx->rec_done[ctx->locpid] = 1;
                ++ctx->num_done;
                if (ctx->num_done == ctx->children) {
                    printf(log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
                    fprintf(ctx->events, log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
                }
                *active = 0;
            }
        }
    }
    return 0;
}

int check_arguments(int argc, char *argv[], struct Context *ctx) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }
    ctx->children = 0;
    ctx->mutexl = 0;
    if (parseArguments(argc, argv, ctx)) return 1;
    return 0;
}

int initialize_and_setup(struct Context *ctx) {
    if (initializePipes(ctx)) return 2;
    setupEventLog(ctx);
    if (createChildProcesses(ctx)) return 3;
    closeUnusedPipesInParent(ctx);
    return 0;
}

void reset_started_done_flags(struct Context *ctx) {
    for (local_id i = 1; i <= ctx->children; ++i) ctx->rec_started[i] = 0;
    ctx->num_started = 0;
    for (local_id i = 1; i <= ctx->children; ++i) ctx->rec_done[i] = 0;
    ctx->num_done = 0;
}

int handle_parent(struct Context *ctx) {
    reset_started_done_flags(ctx);
    while (ctx->num_started < ctx->children || ctx->num_done < ctx->children) {
        Message msg;
        while (receive_any(ctx, &msg)) {}
        switch (msg.s_header.s_type) {
            case STARTED:
                handle_started(ctx, &msg);
                break;
            case DONE:
                handle_done2(ctx, &msg);
                break;
            default:
                break;
        }
        fflush(ctx->events);
    }
    for (local_id i = 0; i < ctx->children; ++i) wait(NULL);
    return 0;
}

void reset_child_flags(struct Context *ctx) {
    for (local_id i = 1; i <= ctx->children; ++i) {
        ctx->reqs[i].locpid = 0;
        ctx->reqs[i].req_time = 0;
    }
}

int send_started_message(struct Context *ctx) {
    ++lamport_time;
    Message started;
    started.s_header.s_magic = MESSAGE_MAGIC;
    started.s_header.s_type = STARTED;
    started.s_header.s_local_time = get_lamport_time();
    sprintf(started.s_payload, log_started_fmt, get_lamport_time(), ctx->locpid, getpid(), getppid(), 0);
    started.s_header.s_payload_len = strlen(started.s_payload);
    puts(started.s_payload);
    fputs(started.s_payload, ctx->events);
    if (send_multicast(ctx, &started)) {
        fprintf(stderr, "Child %d: failed to send STARTED message\n", ctx->locpid);
        closePipes(&ctx->pipes);
        fclose(ctx->events);
        return 4;
    }
    return 0;
}

void init_process_status(struct Context *ctx) {
	for (local_id i = 1; i <= ctx->children; ++i) {
		ctx->rec_started[i] = (i == ctx->locpid);
	}
	ctx->num_started = 1;
	for (local_id i = 1; i <= ctx->children; ++i) {
		ctx->rec_done[i] = 0;
	}
	ctx->num_done = 0;
}

int handle_started_message(struct Context *ctx, int8_t *active, Message *msg) {
	if (handle_started_case(ctx, &lamport_time, msg, active)) {
		return 5;
	}
	return 0;
}

int handle_cs_request_message(struct Context *ctx, int8_t *active, Message *msg) {
	if (handle_cs_request2(ctx, &lamport_time, msg, active)) {
		return 8;
	}
	return 0;
}

void handle_done_message(struct Context *ctx, Message *msg) {
	handle_done_case(ctx, &lamport_time, msg);
}

int process_messages(struct Context *ctx, int8_t *active) {
	while (*active || ctx->num_done < ctx->children) {
		Message msg;
		while (receive_any(ctx, &msg)) {}

		switch (msg.s_header.s_type) {
			case STARTED:
				if (handle_started_message(ctx, active, &msg)) {
					return 5;
				}
			break;
			case CS_REQUEST:
				if (handle_cs_request_message(ctx, active, &msg)) {
					return 8;
				}
			break;
			case DONE:
				handle_done_message(ctx, &msg);
			break;
			default:
				break;
		}
		fflush(ctx->events);
	}
	return 0;
}

int handle_child(struct Context *ctx, int8_t *active) {
	init_process_status(ctx);
	return process_messages(ctx, active);
}

int main(int argc, char *argv[]) {
    struct Context ctx;
    if (check_arguments(argc, argv, &ctx)) return 1;
    if (initialize_and_setup(&ctx)) return 2;

    if (ctx.locpid == PARENT_ID) {
        return handle_parent(&ctx);
    } else {
        reset_child_flags(&ctx);
        if (send_started_message(&ctx)) return 4;
        int8_t active = 1;
        return handle_child(&ctx, &active);
    }
    closePipes(&ctx.pipes);
    fclose(ctx.events);
    return 0;
}


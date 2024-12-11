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

int8_t compare_requests(struct Request first, struct Request second) {
    if (first.req_time > second.req_time) return 1;
    if (first.req_time < second.req_time) return -1;
    if (first.locpid > second.locpid) return 1;
    if (first.locpid < second.locpid) return -1;
    return 0;
}

int request_cs_init(struct Context *ctx) {
    ++lamport_time;
    ctx->reqs[ctx->locpid].locpid = ctx->locpid;
    ctx->reqs[ctx->locpid].req_time = get_lamport_time();
    Message request;
    request.s_header.s_magic = MESSAGE_MAGIC;
    request.s_header.s_type = CS_REQUEST;
    request.s_header.s_payload_len = 0;
    request.s_header.s_local_time = get_lamport_time();
    return send_multicast(ctx, &request);
}

int handle_cs_request(struct Context *ctx, Message *msg) {
    if (ctx->mutexl) {
        if (lamport_time < msg->s_header.s_local_time)
            lamport_time = msg->s_header.s_local_time;
        ++lamport_time;
        if (compare_requests((struct Request){ctx->msg_sender, msg->s_header.s_local_time}, ctx->reqs[ctx->locpid]) < 0) {
            ++lamport_time;
            Message reply;
            reply.s_header.s_magic = MESSAGE_MAGIC;
            reply.s_header.s_type = CS_REPLY;
            reply.s_header.s_payload_len = 0;
            reply.s_header.s_local_time = get_lamport_time();
            return send(ctx, ctx->msg_sender, &reply);
        } else {
            ctx->reqs[ctx->msg_sender].locpid = ctx->msg_sender;
            ctx->reqs[ctx->msg_sender].req_time = msg->s_header.s_local_time;
        }
    }
    return 0;
}

int handle_cs_reply(struct Context *ctx, Message *msg, int8_t *rep_arr, int *replies) {
    if (!rep_arr[ctx->msg_sender]) {
        if (lamport_time < msg->s_header.s_local_time)
            lamport_time = msg->s_header.s_local_time;
        ++lamport_time;
        rep_arr[ctx->msg_sender] = 1;
        ++(*replies);
    }
    return 0;
}

int handle_done(struct Context *ctx, Message *msg, int8_t *rep_arr, int *replies) {
    if (ctx->num_done < ctx->children) {
        if (!ctx->rec_done[ctx->msg_sender]) {
            if (lamport_time < msg->s_header.s_local_time)
                lamport_time = msg->s_header.s_local_time;
            ++lamport_time;
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
    }
    return 0;
}

int request_cs(struct Context *ctx) {
    if (request_cs_init(ctx)) return 1;

    local_id replies = 0;
    int8_t rep_arr[MAX_PROCESS_ID + 1];
    for (local_id i = 1; i <= ctx->children; ++i) {
        replies += ctx->rec_done[i];
        rep_arr[i] = ctx->rec_done[i];
    }

    if (!rep_arr[ctx->locpid]) {
        ++replies;
        rep_arr[ctx->locpid] = 1;
    }

    while (replies < ctx->children) {
        Message msg;
        while (receive_any(ctx, &msg)) {}
        switch (msg.s_header.s_type) {
            case CS_REQUEST:
                if (handle_cs_request(ctx, &msg)) return 2;
                break;
            case CS_REPLY:
                if (handle_cs_reply(ctx, &msg, rep_arr, &replies)) return 2;
                break;
            case DONE:
                if (handle_done(ctx, &msg, rep_arr, &replies)) return 2;
                break;
            default: break;
        }
    }
    return 0;
}

int send_reply_to_children(struct Context *ctx) {
	Message reply;
	reply.s_header.s_magic = MESSAGE_MAGIC;
	reply.s_header.s_type = CS_REPLY;
	reply.s_header.s_payload_len = 0;
	reply.s_header.s_local_time = get_lamport_time();

	for (local_id i = 1; i <= ctx->children; ++i) {
		if (ctx->reqs[i].locpid > 0) {
			if (send(ctx, i, &reply)) return 1;
			ctx->reqs[i].locpid = 0;
			ctx->reqs[i].req_time = 0;
		}
	}
	return 0;
}

int release_cs(const void *self) {
	struct Context *ctx = (struct Context*)self;
	ctx->reqs[ctx->locpid].locpid = 0;
	ctx->reqs[ctx->locpid].req_time = 0;
	++lamport_time;

	return send_reply_to_children(ctx);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_usage(char *program_name) {
    fprintf(stderr, "Usage: %s [--mutexl] -p N [--mutexl]\n", program_name);
}

int parse_arguments(int argc, char *argv[], struct Context *ctx) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    ctx->children = 0;
    ctx->mutexl = 0;
    int8_t rp = 0;

    for (int i = 1; i < argc; ++i) {
        if (rp) {
            ctx->children = atoi(argv[i]);
            rp = 0;
        }
        if (strcmp(argv[i], "--mutexl") == 0) ctx->mutexl = 1;
        else if (strcmp(argv[i], "-p") == 0) rp = 1;
    }
    return 0;
}

int create_pipes(struct Context *ctx) {
    if (initPipes(&ctx->pipes, ctx->children + 1, O_NONBLOCK, pipes_log)) {
        fputs("Parent: failed to create pipes\n", stderr);
        return 2;
    }
    return 0;
}

void close_event_file(FILE *evt) {
    fclose(evt);
}

void start_child_processes(struct Context *ctx) {
    for (local_id i = 1; i <= ctx->children; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            ctx->locpid = i;
            break;
        }
        if (pid < 0) {
            fprintf(stderr, "Parent: failed to create child process %d\n", i);
            closePipes(&ctx->pipes);
            fclose(ctx->events);
            exit(3);
        }
        ctx->locpid = PARENT_ID;
    }
}

void close_unused_pipes(struct Context *ctx) {
    closeUnusedPipes(&ctx->pipes, ctx->locpid);
}

void process_parent(struct Context *ctx) {
    for (local_id i = 1; i <= ctx->children; ++i) ctx->rec_started[i] = 0;
    ctx->num_started = 0;
    for (local_id i = 1; i <= ctx->children; ++i) ctx->rec_done[i] = 0;
    ctx->num_done = 0;

    while (ctx->num_started < ctx->children || ctx->num_done < ctx->children) {
        Message msg;
        while (receive_any(&ctx, &msg)) {}

        switch (msg.s_header.s_type) {
            case STARTED:
                handle_started_message(ctx, &msg);
                break;
            case DONE:
                handle_done_message(ctx, &msg);
                break;
            default: break;
        }
        fflush(ctx->events);
    }
}

void handle_started_message(struct Context *ctx, Message *msg) {
    if (ctx->num_started < ctx->children) {
        if (!ctx->rec_started[ctx->msg_sender]) {
            if (lamport_time < msg->s_header.s_local_time)
                lamport_time = msg->s_header.s_local_time;
            ++lamport_time;
            ctx->rec_started[ctx->msg_sender] = 1;
            ++ctx->num_started;
            if (ctx->num_started == ctx->children) {
                printf(log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
                fprintf(ctx->events, log_received_all_started_fmt, get_lamport_time(),
                        ctx->locpid);
            }
        }
    }
}

void handle_done_message(struct Context *ctx, Message *msg) {
    if (ctx->num_done < ctx->children) {
        if (!ctx->rec_done[ctx->msg_sender]) {
            if (lamport_time < msg->s_header.s_local_time)
                lamport_time = msg->s_header.s_local_time;
            ++lamport_time;
            ctx->rec_done[ctx->msg_sender] = 1;
            ++ctx->num_done;
            if (ctx->num_done == ctx->children) {
                printf(log_received_all_done_fmt, get_lamport_time(), ctx->locpid);
                fprintf(ctx->events, log_received_all_done_fmt, get_lamport_time(),
                        ctx->locpid);
            }
        }
    }
}

void process_child(struct Context *ctx) {
    for (local_id i = 1; i <= ctx->children; ++i) {
        ctx->reqs[i].locpid = 0;
        ctx->reqs[i].req_time = 0;
    }
    ++lamport_time;
    send_started_message(ctx);
    wait_for_started_messages(ctx);
    perform_operations(ctx);
}

void send_started_message(struct Context *ctx) {
    Message started;
    started.s_header.s_magic = MESSAGE_MAGIC;
    started.s_header.s_type = STARTED;
    started.s_header.s_local_time = get_lamport_time();
    sprintf(started.s_payload, log_started_fmt, get_lamport_time(), ctx->locpid, getpid(), getppid(), 0);
    started.s_header.s_payload_len = strlen(started.s_payload);
    puts(started.s_payload);
    fputs(started.s_payload, ctx->events);
    if (send_multicast(&ctx, &started)) {
        fprintf(stderr, "Child %d: failed to send STARTED message\n", ctx->locpid);
        closePipes(&ctx->pipes);
        fclose(ctx->events);
        exit(4);
    }
}

void wait_for_started_messages(struct Context *ctx) {
    for (local_id i = 1; i <= ctx->children; ++i) ctx->rec_started[i] = (i == ctx->locpid);
    ctx->num_started = 1;
    for (local_id i = 1; i <= ctx->children; ++i) ctx->rec_done[i] = 0;
    ctx->num_done = 0;
}

void perform_operations(struct Context *ctx) {
    int8_t active = 1;
    while (active || ctx->num_done < ctx->children) {
        Message msg;
        while (receive_any(&ctx, &msg)) {}

        switch (msg.s_header.s_type) {
            case STARTED:
                handle_started_message(ctx, &msg);
                break;
            case CS_REQUEST:
                handle_cs_request(ctx, &msg);
                break;
            case DONE:
                handle_done_message(ctx, &msg);
                break;
            default: break;
        }
        fflush(ctx->events);
    }
}

void handle_cs_request(struct Context *ctx, Message *msg) {
    if (ctx->mutexl) {
        if (lamport_time < msg->s_header.s_local_time) lamport_time = msg->s_header.s_local_time;
        ++lamport_time;
        Message reply;
        reply.s_header.s_magic = MESSAGE_MAGIC;
        reply.s_header.s_type = CS_REPLY;
        reply.s_header.s_payload_len = 0;
        reply.s_header.s_local_time = get_lamport_time();
        if (send(&ctx, ctx->msg_sender, &reply)) {
            fprintf(stderr, "Child %d: failed to send CS_REPLY message\n", ctx->locpid);
            closePipes(&ctx->pipes);
            fclose(ctx->events);
            exit(8);
        }
    }
}

int main(int argc, char *argv[]) {
    struct Context ctx;

    if (parse_arguments(argc, argv, &ctx)) return 1;

    if (create_pipes(&ctx)) return 2;

    FILE *evt = fopen(events_log, "w");
    close_event_file(evt);
    ctx.events = fopen(events_log, "a");

    start_child_processes(&ctx);
    close_unused_pipes(&ctx);

    if (ctx.locpid == PARENT_ID) {
        process_parent(&ctx);
    } else {
        process_child(&ctx);
    }

    closePipes(&ctx.pipes);
    fclose(ctx.events);
    return 0;
}

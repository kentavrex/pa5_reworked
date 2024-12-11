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

void handle_cs_request(struct Context *ctx, Message *msg, int8_t *rep_arr, local_id *replies) {
    if (ctx->mutexl) {
        if (lamport_time < msg->s_header.s_local_time) lamport_time = msg->s_header.s_local_time;
        ++lamport_time;
        if (compare_requests((struct Request){ctx->msg_sender, msg->s_header.s_local_time}, ctx->reqs[ctx->locpid]) < 0) {
            ++lamport_time;
            Message reply;
            reply.s_header.s_magic = MESSAGE_MAGIC;
            reply.s_header.s_type = CS_REPLY;
            reply.s_header.s_payload_len = 0;
            reply.s_header.s_local_time = get_lamport_time();
            if (send(ctx, ctx->msg_sender, &reply)) return;
        } else {
            ctx->reqs[ctx->msg_sender].locpid = ctx->msg_sender;
            ctx->reqs[ctx->msg_sender].req_time = msg->s_header.s_local_time;
        }
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

void handle_done(struct Context *ctx, Message *msg, int8_t *rep_arr, local_id *replies) {
    if (ctx->num_done < ctx->children) {
        if (!ctx->rec_done[ctx->msg_sender]) {
            if (lamport_time < msg->s_header.s_local_time) lamport_time = msg->s_header.s_local_time;
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
}

int release_cs(const void * self) {
	struct Context *ctx = (struct Context*)self;
	ctx->reqs[ctx->locpid].locpid = 0;
    ctx->reqs[ctx->locpid].req_time = 0;
	++lamport_time;
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

int request_cs(const void * self) {
	struct Context *ctx = (struct Context*)self;
	++lamport_time;
	ctx->reqs[ctx->locpid].locpid = ctx->locpid;
	ctx->reqs[ctx->locpid].req_time = get_lamport_time();
	Message request;
	request.s_header.s_magic = MESSAGE_MAGIC;
	request.s_header.s_type = CS_REQUEST;
	request.s_header.s_payload_len = 0;
	request.s_header.s_local_time = get_lamport_time();
	if (send_multicast(ctx, &request)) return 1;
	local_id replies = 0;
	int8_t rep_arr[MAX_PROCESS_ID+1];
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
				handle_cs_request(ctx, &msg, rep_arr, &replies);
			break;
			case CS_REPLY:
				handle_cs_reply(ctx, &msg, rep_arr, &replies);
			break;
			case DONE:
				handle_done(ctx, &msg, rep_arr, &replies);
			break;
			default:
				break;
		}
	}
	return 0;
}

void handle_started(struct Context *ctx, Message *msg) {
	if (ctx->num_started < ctx->children) {
		if (!ctx->rec_started[ctx->msg_sender]) {
			if (lamport_time < msg->s_header.s_local_time)
				lamport_time = msg->s_header.s_local_time;
			++lamport_time;
			ctx->rec_started[ctx->msg_sender] = 1;
			++ctx->num_started;
			if (ctx->num_started == ctx->children) {
				printf(log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
				fprintf(ctx->events, log_received_all_started_fmt, get_lamport_time(), ctx->locpid);
			}
		}
	}
}

void handle_done2(struct Context *ctx, Message *msg) {
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
		}
	}
}

void printUsage(char *programName) {
	fprintf(stderr, "Usage: %s [--mutexl] -p N [--mutexl]\n", programName);
}

int parseArguments(int argc, char *argv[], struct Context *ctx) {
	int8_t rp = 0;
	for (int i = 1; i < argc; ++i) {
		if (rp) {
			ctx->children = atoi(argv[i]);
			rp = 0;
		}
		if (strcmp(argv[i], "--mutexl") == 0) {
			ctx->mutexl = 1;
		} else if (strcmp(argv[i], "-p") == 0) {
			rp = 1;
		}
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

int main(int argc, char *argv[]) {
	struct Context ctx;
	if (argc < 3) {
		printUsage(argv[0]);
		return 1;
	}
	ctx.children = 0;
	ctx.mutexl = 0;
	if (parseArguments(argc, argv, &ctx)) return 1;
	if (initializePipes(&ctx)) return 2;
	setupEventLog(&ctx);
	if (createChildProcesses(&ctx)) return 3;
	closeUnusedPipesInParent(&ctx);
	if (ctx.locpid == PARENT_ID) {
		for (local_id i = 1; i <= ctx.children; ++i) ctx.rec_started[i] = 0;
		ctx.num_started = 0;
		for (local_id i = 1; i <= ctx.children; ++i) ctx.rec_done[i] = 0;
		ctx.num_done = 0;
		while (ctx.num_started < ctx.children || ctx.num_done < ctx.children) {
			Message msg;
			while (receive_any(&ctx, &msg)) {}
			switch (msg.s_header.s_type) {
				case STARTED:
					handle_started(&ctx, &msg);
					break;
				case DONE:
					handle_done2(&ctx, &msg);
					break;
				default:
					break;
			}
			fflush(ctx.events);
		}
		for (local_id i = 0; i < ctx.children; ++i) wait(NULL);
	} else {
		for (local_id i = 1; i <= ctx.children; ++i) {
            ctx.reqs[i].locpid = 0;
            ctx.reqs[i].req_time = 0;
        }
		++lamport_time;
		Message started;
		started.s_header.s_magic = MESSAGE_MAGIC;
		started.s_header.s_type = STARTED;
		started.s_header.s_local_time = get_lamport_time();
		sprintf(started.s_payload, log_started_fmt, get_lamport_time(), ctx.locpid, getpid(), getppid(), 0);
		started.s_header.s_payload_len = strlen(started.s_payload);
		puts(started.s_payload);
		fputs(started.s_payload, ctx.events);
		if (send_multicast(&ctx, &started)) {
			fprintf(stderr, "Child %d: failed to send STARTED message\n", ctx.locpid);
			closePipes(&ctx.pipes);
			fclose(ctx.events);
			return 4;
		}
		for (local_id i = 1; i <= ctx.children; ++i) ctx.rec_started[i] = (i == ctx.locpid);
		ctx.num_started = 1;
		for (local_id i = 1; i <= ctx.children; ++i) ctx.rec_done[i] = 0;
		ctx.num_done = 0;
		int8_t active = 1;
		while (active || ctx.num_done < ctx.children) {
			Message msg;
			while (receive_any(&ctx, &msg)) {}
			switch (msg.s_header.s_type) {
				case STARTED:
					if (ctx.num_started < ctx.children) {
						if (!ctx.rec_started[ctx.msg_sender]) {
							if (lamport_time < msg.s_header.s_local_time)
								lamport_time = msg.s_header.s_local_time;
							++lamport_time;
							ctx.rec_started[ctx.msg_sender] = 1;
							++ctx.num_started;
							if (ctx.num_started == ctx.children) {
								printf(log_received_all_started_fmt, get_lamport_time(), ctx.locpid);
								fprintf(ctx.events, log_received_all_started_fmt, get_lamport_time(),
										ctx.locpid);
								for (int16_t i = 1; i <= ctx.locpid * 5; ++i) {
									char log[50];
									sprintf(log, log_loop_operation_fmt, ctx.locpid, i, ctx.locpid * 5);
									if (ctx.mutexl) {
										int status = request_cs(&ctx);
										if (status) {
											fprintf(stderr,
												"Child %d: request_cs() resulted %d\n",
													ctx.locpid, status);
											closePipes(&ctx.pipes);
											fclose(ctx.events);
											return 5;
										}
									}
									print(log);
									if (ctx.mutexl) {
										int status = release_cs(&ctx);
										if (status) {
											fprintf(stderr,
												"Child %d: release_cs() resulted %d\n",
													ctx.locpid, status);
											closePipes(&ctx.pipes);
											fclose(ctx.events);
											return 6;
										}
									}
								}
								++lamport_time;
								Message done;
								done.s_header.s_magic = MESSAGE_MAGIC;
								done.s_header.s_type = DONE;
								sprintf(done.s_payload, log_done_fmt, get_lamport_time(), ctx.locpid, 0);
								done.s_header.s_payload_len = strlen(done.s_payload);
								done.s_header.s_local_time = get_lamport_time();
								puts(done.s_payload);
								fputs(done.s_payload, ctx.events);
								if (send_multicast(&ctx, &done)) {
									fprintf(stderr, "Child %d: failed to send DONE message\n",
											ctx.locpid);
									closePipes(&ctx.pipes);
									fclose(ctx.events);
									return 7;
								}
								ctx.rec_done[ctx.locpid] = 1;
								++ctx.num_done;
								if (ctx.num_done == ctx.children) {
									printf(log_received_all_done_fmt, get_lamport_time(), ctx.locpid);
									fprintf(ctx.events, log_received_all_done_fmt, get_lamport_time(),
											ctx.locpid);
								}
								active = 0;
							}
						}
					}
					break;
				case CS_REQUEST:
					if (active && ctx.mutexl) {
						if (lamport_time < msg.s_header.s_local_time) lamport_time = msg.s_header.s_local_time;
						++lamport_time;
						++lamport_time;
						Message reply;
						reply.s_header.s_magic = MESSAGE_MAGIC;
						reply.s_header.s_type = CS_REPLY;
						reply.s_header.s_payload_len = 0;
						reply.s_header.s_local_time = get_lamport_time();
						if (send(&ctx, ctx.msg_sender, &reply)) {
							fprintf(stderr, "Child %d: failed to send CS_REPLY message\n", ctx.locpid);
							closePipes(&ctx.pipes);
							fclose(ctx.events);
							return 8;
						}
					}
					break;
				case DONE:
					if (ctx.num_done < ctx.children) {
						if (!ctx.rec_done[ctx.msg_sender]) {
							if (lamport_time < msg.s_header.s_local_time)
								lamport_time = msg.s_header.s_local_time;
							++lamport_time;
							ctx.rec_done[ctx.msg_sender] = 1;
							++ctx.num_done;
							if (ctx.num_done == ctx.children) {
								printf(log_received_all_done_fmt, get_lamport_time(), ctx.locpid);
								fprintf(ctx.events, log_received_all_done_fmt, get_lamport_time(),
										ctx.locpid);
							}
						}
					}
					break;
				default: break;
			}
			fflush(ctx.events);
		}
	}
	closePipes(&ctx.pipes);
	fclose(ctx.events);
	return 0;
}

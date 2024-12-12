#include <unistd.h>
#include "context.h"
#include "ipc.h"
#include "pipes.h"

int write_message(Descriptor fd, const char *data, int64_t len) {
    return write(fd, data, len);
}

int get_message_length(const Message *msg) {
    return sizeof(MessageHeader) + msg->s_header.s_payload_len;
}

Descriptor get_send_pipe(struct Context *ctx, local_id dst) {
    return accessPipe(&ctx->pipes, (struct PipeDescriptor){ctx->locpid, dst, WRITING});
}

int send_message(Descriptor fd, const Message *msg, int64_t msg_len) {
    return write_message(fd, (const char*)msg, msg_len);
}

int send(void *self, local_id dst, const Message *msg) {
    struct Context *ctx = (struct Context*)self;
    int64_t msg_len = get_message_length(msg);
    Descriptor fd = get_send_pipe(ctx, dst);
    if (send_message(fd, msg, msg_len) < msg_len) return 1;
    return 0;
}

Descriptor get_multicast_send_pipe(struct Context *ctx, local_id dst) {
    return accessPipe(&ctx->pipes, (struct PipeDescriptor){ctx->locpid, dst, WRITING});
}

int send_message_to_pipe(Descriptor fd, const Message *msg, int64_t msg_len) {
    return send_message(fd, msg, msg_len);
}

int send_multicast(void *self, const Message *msg) {
    struct Context *ctx = (struct Context*)self;
    int64_t msg_len = get_message_length(msg);
    for (local_id i = 0; i <= ctx->children; ++i) {
        if (i != ctx->locpid) {
            Descriptor fd = get_multicast_send_pipe(ctx, i);
            if (send_message_to_pipe(fd, msg, msg_len) < msg_len) return 1;
        }
    }
    return 0;
}

int read_message_header(Descriptor fd, Message *msg) {
    return read(fd, msg, sizeof(MessageHeader));
}

int is_valid_message(const Message *msg) {
    return msg->s_header.s_magic == MESSAGE_MAGIC;
}

int read_message_payload(Descriptor fd, Message *msg) {
    return read(fd, msg->s_payload, msg->s_header.s_payload_len);
}

Descriptor get_receive_pipe(struct Context *ctx, local_id from) {
    return accessPipe(&ctx->pipes, (struct PipeDescriptor){from, ctx->locpid, READING});
}

int read_full_message(Descriptor fd, Message *msg) {
    if ((int64_t)read_message_header(fd, msg) < (int64_t)sizeof(MessageHeader)) return 1;
    if (!is_valid_message(msg)) return 2;
    if ((int64_t)read_message_payload(fd, msg) < (int64_t)(msg->s_header.s_payload_len)) return 3;
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    struct Context *ctx = (struct Context*)self;
    Descriptor fd = get_receive_pipe(ctx, from);
    int result = read_full_message(fd, msg);
    if (result == 0) ctx->msg_sender = from;
    return result;
}

int read_message_from_pipe(struct Context *ctx, local_id i, Message *msg) {
    Descriptor fd = get_receive_pipe(ctx, i);
    int result = read_full_message(fd, msg);
    return result;
}

int process_received_message(struct Context *ctx, Message *msg, local_id i) {
    ctx->msg_sender = i;
    return 0;
}

int receive_any(void *self, Message *msg) {
    struct Context *ctx = (struct Context*)self;
    for (local_id i = 0; i <= ctx->children; ++i) {
        if (i != ctx->locpid) {
            int result = read_message_from_pipe(ctx, i, msg);
            if (result == 0) {
                return process_received_message(ctx, msg, i);
            }
        }
    }
    return 1;
}

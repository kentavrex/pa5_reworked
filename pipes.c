#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "pipes.h"

int createPipe(Descriptor *pipe_descs, int index) {
    return pipe(pipe_descs + 2 * index);
}

int setPipeFlags(Descriptor *pipe_descs, int index, int flags) {
    int status = fcntl(pipe_descs[2 * index], F_SETFL, fcntl(pipe_descs[2 * index], F_GETFL, 0) | flags);
    if (status) return status;

    return fcntl(pipe_descs[2 * index + 1], F_SETFL, fcntl(pipe_descs[2 * index + 1], F_GETFL, 0) | flags);
}

void logPipeDescriptor(struct Pipes *pipes, int i) {
    fprintf(pipes->plog, "Opened pipe descriptors %d and %d\n", pipes->pipe_descs[2 * i], pipes->pipe_descs[2 * i + 1]);
    fflush(pipes->plog);
}

int openPipe(Descriptor *pipe_descs, int index, int flags) {
    int status = createPipe(pipe_descs, index);
    if (status) return status;

    return setPipeFlags(pipe_descs, index, flags);
}

int initPipes(struct Pipes *pipes, local_id procnum, int flags, const char *log_file) {
    int status = 0;
    pipes->size = procnum;
    pipes->pipe_descs = malloc(2 * procnum * (procnum - 1) * sizeof(Descriptor));
    pipes->plog = fopen(log_file, "w");

    for (int i = 0; i < procnum * (procnum - 1); ++i) {
        status = openPipe(pipes->pipe_descs, i, flags);
        if (status) {
            closePipes(pipes);
            return status;
        }
        logPipeDescriptor(pipes, i);
    }
    return status;
}

int isValidAddress(const struct Pipes *pipes, struct PipeDescriptor address) {
    return address.from >= 0 && address.from < pipes->size && address.to >= 0 && address.to < pipes->size && address.from != address.to;
}

int calculatePipeIndex(const struct Pipes *pipes, struct PipeDescriptor address) {
    int index = address.from * (pipes->size - 1);
    return index + address.to - (address.from < address.to);
}

Descriptor accessPipe(const struct Pipes *pipes, struct PipeDescriptor address) {
    if (!isValidAddress(pipes, address)) return -1;
    int index = calculatePipeIndex(pipes, address);
    return pipes->pipe_descs[2 * index + address.mode];
}

void closePipeDescriptor(struct Pipes *pipes, local_id procid, Descriptor pipe_desc, const char *pipe_type) {
    close(pipe_desc);
    fprintf(pipes->plog, "Process %d closed pipe descriptor %d for %s\n", procid, pipe_desc, pipe_type);
}

void closeUnusedPipes(const struct Pipes *pipes, local_id procid) {
    for (local_id i = 0; i < pipes->size; ++i) {
        for (local_id j = 0; j < pipes->size; ++j) {
            if (i != j) {
                Descriptor rd = accessPipe(pipes, (struct PipeDescriptor){i, j, READING});
                Descriptor wr = accessPipe(pipes, (struct PipeDescriptor){i, j, WRITING});
                if (i != procid) closePipeDescriptor(pipes, procid, wr, "writing");
                if (j != procid) closePipeDescriptor(pipes, procid, rd, "reading");
            }
        }
    }
    fflush(pipes->plog);
}

int calculatePipeFromAndTo(int pipe_index, const struct Pipes *pipes, struct PipeDescriptor *res) {
    res->from = pipe_index / (pipes->size - 1);
    res->to = pipe_index % (pipes->size - 1);
    return 0;
}

void adjustPipeTo(struct PipeDescriptor *res) {
    if (res->from <= res->to) ++res->to;
}

struct PipeDescriptor describePipe(const struct Pipes *pipes, int desc_index) {
    struct PipeDescriptor res;
    int pipe_index = desc_index / 2;
    res.mode = desc_index % 2;
    calculatePipeFromAndTo(pipe_index, pipes, &res);
    adjustPipeTo(&res);
    return res;
}

void closePipes(struct Pipes *pipes) {
    for (int i = 0; i < 2 * pipes->size * (pipes->size - 1); ++i) close(pipes->pipe_descs[i]);
    free(pipes->pipe_descs);
    pipes->size = 0;
    fclose(pipes->plog);
}

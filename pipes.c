#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "pipes.h"

void closeDescriptor(Descriptor *desc) {
    close(*desc);
}

void logPipeOpening(FILE *plog, Descriptor pipe_desc1, Descriptor pipe_desc2) {
    fprintf(plog, "Opened pipe descriptors %d and %d\n", pipe_desc1, pipe_desc2);
    fflush(plog);
}

int setFlagsForPipe(Descriptor *pipe_desc, int flags) {
    return fcntl(*pipe_desc, F_SETFL, fcntl(*pipe_desc, F_GETFL, 0) | flags);
}

int openPipePair(Descriptor *pipe_desc1, Descriptor *pipe_desc2) {
    if (pipe(pipe_desc1) < 0 || pipe(pipe_desc2) < 0) {
        return -1;
    }
    return 0;
}

int setPipeNonBlocking(Descriptor *pipe_desc) {
    return setFlagsForPipe(pipe_desc, O_NONBLOCK);
}

void closeAllDescriptors(Descriptor *desc1, Descriptor *desc2) {
    closeDescriptor(desc1);
    closeDescriptor(desc2);
}

int initializePipe(Descriptor *pipe_desc1, Descriptor *pipe_desc2, int flags) {
    if (openPipePair(pipe_desc1, pipe_desc2) < 0) {
        return -1;
    }
    if (setPipeNonBlocking(pipe_desc1) < 0 || setPipeNonBlocking(pipe_desc2) < 0) {
        return -1;
    }
    return 0;
}

int initPipes(struct Pipes *pipes, local_id procnum, int flags, const char *log_file) {
    int status = 0;
    pipes->size = procnum;
    pipes->pipe_descs = malloc(2 * procnum * (procnum - 1) * sizeof(Descriptor));
    pipes->plog = fopen(log_file, "w");

    for (int i = 0; i < procnum * (procnum - 1); ++i) {
        status = initializePipe(&pipes->pipe_descs[2*i], &pipes->pipe_descs[2*i+1], flags);
        if (status) {
            closePipes(pipes);
            return status;
        }
        logPipeOpening(pipes->plog, pipes->pipe_descs[2*i], pipes->pipe_descs[2*i+1]);
    }

    fflush(pipes->plog);
    return status;
}

Descriptor getPipe(const struct Pipes *pipes, struct PipeDescriptor address) {
    int index = address.from * (pipes->size - 1);
    index += address.to - (address.from < address.to);
    return pipes->pipe_descs[2 * index + address.mode];
}

void closeUnusedPipe(Descriptor *desc, local_id procid, FILE *plog) {
    closeDescriptor(desc);
    fprintf(plog, "Process %d closed pipe descriptor %d\n", procid, *desc);
}

void closeUnusedPipes(const struct Pipes *pipes, local_id procid) {
    for (local_id i = 0; i < pipes->size; ++i) {
        for (local_id j = 0; j < pipes->size; ++j) {
            if (i != j) {
                Descriptor rd = getPipe(pipes, (struct PipeDescriptor){i, j, READING});
                Descriptor wr = getPipe(pipes, (struct PipeDescriptor){i, j, WRITING});
                if (i != procid) closeUnusedPipe(&wr, procid, pipes->plog);
                if (j != procid) closeUnusedPipe(&rd, procid, pipes->plog);
            }
        }
    }
    fflush(pipes->plog);
}

struct PipeDescriptor describePipe(const struct Pipes *pipes, int desc_index) {
    struct PipeDescriptor res;
    int pipe_index = desc_index / 2;
    res.mode = desc_index % 2;
    res.from = pipe_index / (pipes->size - 1);
    res.to = pipe_index % (pipes->size - 1);
    if (res.from <= res.to) ++res.to;
    return res;
}

void closePipes(struct Pipes *pipes) {
    for (int i = 0; i < 2 * pipes->size * (pipes->size - 1); ++i) {
        closeDescriptor(&pipes->pipe_descs[i]);
    }
    free(pipes->pipe_descs);
    pipes->size = 0;
    fclose(pipes->plog);
}

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "pipes.h"

void closePipes(struct Pipes *pipes) {
	for (int i = 0; i < 2*pipes->size*(pipes->size-1); ++i) close(pipes->pipe_descs[i]);
	free(pipes->pipe_descs);
	pipes->size = 0;
    fclose(pipes->plog);
}

int initPipes(struct Pipes *pipes, local_id procnum, int flags, const char *log_file) { // flags are appended to existing ones
	int status = 0;
	pipes->size = procnum;
	pipes->pipe_descs = malloc(2*procnum*(procnum-1)*sizeof(Descriptor));
    pipes->plog = fopen(log_file, "w");
	for (int i = 0; i < procnum*(procnum-1); ++i) {
		if ((status = pipe(pipes->pipe_descs+2*i))) {
			closePipes(pipes);
			return status;
		}
		if ((status = fcntl(pipes->pipe_descs[2*i], F_SETFL, fcntl(pipes->pipe_descs[2*i], F_GETFL, 0) | flags))) {
			closePipes(pipes);
			return status;
		}
		if ((status = fcntl(pipes->pipe_descs[2*i+1], F_SETFL, fcntl(pipes->pipe_descs[2*i+1], F_GETFL, 0) | flags))) {
			closePipes(pipes);
			return status;
		}
		fprintf(pipes->plog, "Opened pipe descriptors %d and %d\n", pipes->pipe_descs[2*i], pipes->pipe_descs[2*i+1]);
	}
	fflush(pipes->plog);
	return status;
}

Descriptor accessPipe(const struct Pipes *pipes, struct PipeDescriptor address) { // returns pipe descriptor
	if (address.from < 0 || address.from >= pipes->size || address.to < 0 || address.to >= pipes->size || address.from == address.to)
		return -1;
	int index = address.from * (pipes->size - 1);
	index += address.to - (address.from < address.to);
	return pipes->pipe_descs[2*index+address.mode];
}

void closeUnusedPipes(const struct Pipes *pipes, local_id procid) {
	for (local_id i = 0; i < pipes->size; ++i) {
		for (local_id j = 0; j < pipes->size; ++j) {
			if (i != j) {
                Descriptor rd = accessPipe(pipes, (struct PipeDescriptor){i, j, READING});
                Descriptor wr = accessPipe(pipes, (struct PipeDescriptor){i, j, WRITING});
				if (i != procid) {
                    close(wr);
                    fprintf(pipes->plog, "Process %d closed pipe descriptor %d\n", procid, wr);
                }
				if (j != procid) {
                    close(rd);
                    fprintf(pipes->plog, "Process %d closed pipe descriptor %d\n", procid, rd);
                }
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

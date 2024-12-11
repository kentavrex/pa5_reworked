#ifndef __PIPES__H
#define __PIPES__H

#include <stdio.h>

#include "ipc.h"

typedef int Descriptor;
typedef int8_t Mode;

static const Mode READING = 0;
static const Mode WRITING = 1;

struct PipeDescriptor {
	local_id from, to;
	Mode mode;
};

struct Pipes { // fully-connected network
	local_id size; // number of processes
	Descriptor *pipe_descs; // should not be accessed directly
	FILE *plog; // pipe logger
};

void closePipes(struct Pipes *pipes);
int initPipes(struct Pipes *pipes, local_id procnum, int flags, const char *log_file); // flags are appended to existing ones
Descriptor accessPipe(const struct Pipes *pipes, struct PipeDescriptor address); // returns pipe descriptor
void closeUnusedPipes(const struct Pipes *pipes, local_id procid);
struct PipeDescriptor describePipe(const struct Pipes *pipes, int desc_index);

#endif

#ifndef CS_H
#define CS_H


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <asm-generic/errno.h>

#include "pa2345.h"
#include "base_vars.h"



int send_critical_section_request(const void* context);

int send_critical_section_release(const void* context);

#endif

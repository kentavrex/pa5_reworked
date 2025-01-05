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


int report_request_to_enter_crit_sec(const void* context);

int report_release_to_enter_crit_sec(const void* context);

#endif

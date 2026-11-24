#ifndef STATS_H
#define STATS_H

#include "ipc.h"

void print_stats_console(ipc_resources_t *ipc);
void save_stats_snapshot(ipc_resources_t *ipc);

#endif
#include <stdio.h>
#include <string.h>
#include "server.h"

#define MAX_CPU        1024
#define MAX_RAM_MB     4194304
#define MAX_STORAGE_GB 1048576
#define MAX_GPU        64
#define MAX_BANDWIDTH  400000

/* CPU, RAM, storage zaroori (> 0). GPU aur bandwidth 0 ho sakte hain. */
static bool capacity_is_valid(ResourceVector v)
{
    if (v.cpu_cores      <= 0 || v.cpu_cores      > MAX_CPU)        return false;
    if (v.ram_mb         <= 0 || v.ram_mb         > MAX_RAM_MB)     return false;
    if (v.storage_gb     <= 0 || v.storage_gb     > MAX_STORAGE_GB) return false;
    if (v.gpu_units      <  0 || v.gpu_units      > MAX_GPU)        return false;
    if (v.bandwidth_mbps <  0 || v.bandwidth_mbps > MAX_BANDWIDTH)  return false;
    return true;
}

static bool is_zero(ResourceVector v)
{
    return v.cpu_cores == 0 && v.ram_mb == 0 && v.storage_gb == 0 &&
           v.gpu_units == 0 && v.bandwidth_mbps == 0;
}

status_t system_init(SystemState *s)
{
    if (s == NULL) return STATUS_INVALID_INPUT;

    memset(s, 0, sizeof(*s));
    s->next_vm_id = 1;
    s->next_request_id = 1;

    if (pthread_mutex_init(&s->lock, NULL) != 0)
        return STATUS_SYSTEM_ERROR;

    return STATUS_OK;
}

status_t server_init(SystemState *s, ResourceVector total)
{
    if (s == NULL) return STATUS_INVALID_INPUT;
    if (s->server.configured) return STATUS_INVALID_INPUT;
    if (!capacity_is_valid(total)) return STATUS_INVALID_INPUT;

    memset(&s->server, 0, sizeof(s->server));
    s->server.total = total;
    s->server.available = total;
    s->server.configured = true;
    return STATUS_OK;
}

status_t server_reset(SystemState *s)
{
    if (s == NULL) return STATUS_INVALID_INPUT;
    if (!is_zero(s->server.allocated)) return STATUS_INVALID_INPUT;

    memset(&s->server, 0, sizeof(s->server));
    return STATUS_OK;
}

void server_display(const SystemState *s)
{
    if (s == NULL || !s->server.configured) {
        printf("Server abhi configure nahi hua.\n");
        return;
    }

    const ResourceVector *t = &s->server.total;
    const ResourceVector *a = &s->server.allocated;
    const ResourceVector *v = &s->server.available;

    printf("\n=== Server Capacity ===\n");
    printf("%-18s %10s %10s %10s\n", "Resource", "Total", "Allocated", "Available");
    printf("%-18s %10d %10d %10d\n", "CPU (cores)",      t->cpu_cores,      a->cpu_cores,      v->cpu_cores);
    printf("%-18s %10d %10d %10d\n", "RAM (MB)",         t->ram_mb,         a->ram_mb,         v->ram_mb);
    printf("%-18s %10d %10d %10d\n", "Storage (GB)",     t->storage_gb,     a->storage_gb,     v->storage_gb);
    printf("%-18s %10d %10d %10d\n", "GPU (units)",      t->gpu_units,      a->gpu_units,      v->gpu_units);
    printf("%-18s %10d %10d %10d\n", "Bandwidth (Mbps)", t->bandwidth_mbps, a->bandwidth_mbps, v->bandwidth_mbps);
    printf("\n");
}

#include <stdio.h>
#include <string.h>
#include "vm.h"

const char *vm_status_str(VmStatus st)
{
    switch (st) {
        case VM_STOPPED:    return "Stopped";
        case VM_WAITING:    return "Waiting";
        case VM_RUNNING:    return "Running";
        case VM_TERMINATED: return "Terminated";
    }
    return "Unknown";
}

const char *vm_type_str(VmType t)
{
    switch (t) {
        case VM_AI_TRAINING: return "AI Training";
        case VM_DATABASE:    return "Database";
        case VM_WEB_HOSTING: return "Web Hosting";
        case VM_GENERAL:     return "General";
    }
    return "Unknown";
}

/* a ka har resource b se chhota ya barabar hai? */
static bool fits_within(ResourceVector a, ResourceVector b)
{
    return a.cpu_cores      <= b.cpu_cores      &&
           a.ram_mb         <= b.ram_mb         &&
           a.storage_gb     <= b.storage_gb     &&
           a.gpu_units      <= b.gpu_units      &&
           a.bandwidth_mbps <= b.bandwidth_mbps;
}

/* VM ko kam az kam CPU, RAM, storage chahiye; koi value negative nahi */
static bool claim_is_valid(ResourceVector v)
{
    if (v.cpu_cores <= 0 || v.ram_mb <= 0 || v.storage_gb <= 0) return false;
    if (v.gpu_units < 0 || v.bandwidth_mbps < 0) return false;
    return true;
}

status_t vm_create(SystemState *s, const char *name, const char *owner,
                   VmType type, ResourceVector max_claim, int priority,
                   int *out_id)
{
    if (s == NULL || name == NULL || owner == NULL || out_id == NULL)
        return STATUS_INVALID_INPUT;
    if (!s->server.configured)
        return STATUS_INVALID_INPUT;
    if (name[0] == '\0' || owner[0] == '\0')
        return STATUS_INVALID_INPUT;
    if (priority < 1 || priority > 5)
        return STATUS_INVALID_INPUT;
    if (!claim_is_valid(max_claim))
        return STATUS_INVALID_INPUT;
    if (!fits_within(max_claim, s->server.total))
        return STATUS_INSUFFICIENT_CAPACITY;   /* server kabhi poora nahi kar sakta */

    pthread_mutex_lock(&s->lock);

    if (s->vm_count >= MAX_VMS) {
        pthread_mutex_unlock(&s->lock);
        return STATUS_SYSTEM_ERROR;
    }

    VM *vm = &s->vms[s->vm_count];
    memset(vm, 0, sizeof(*vm));

    vm->id = s->next_vm_id++;
    strncpy(vm->name, name, MAX_NAME_LENGTH - 1);
    strncpy(vm->owner, owner, MAX_OWNER_LENGTH - 1);
    vm->type = type;
    vm->status = VM_STOPPED;
    vm->priority = priority;
    vm->max_claim = max_claim;
    /* allocation aur usage memset se zero hain */

    s->vm_count++;
    *out_id = vm->id;

    pthread_mutex_unlock(&s->lock);
    return STATUS_OK;
}

VM *vm_find(SystemState *s, int vm_id)
{
    if (s == NULL) return NULL;
    for (int i = 0; i < s->vm_count; i++) {
        if (s->vms[i].id == vm_id)
            return &s->vms[i];
    }
    return NULL;
}

void vm_list(const SystemState *s)
{
    if (s == NULL || s->vm_count == 0) {
        printf("Koi VM maujood nahi.\n");
        return;
    }

    printf("\n=== VM List ===\n");
    printf("%-4s %-16s %-12s %-12s %-11s %-4s\n",
           "ID", "Name", "Owner", "Type", "Status", "Prio");
    for (int i = 0; i < s->vm_count; i++) {
        const VM *v = &s->vms[i];
        printf("%-4d %-16s %-12s %-12s %-11s %-4d\n",
               v->id, v->name, v->owner,
               vm_type_str(v->type), vm_status_str(v->status), v->priority);
    }
    printf("\n");
}

status_t vm_details(SystemState *s, int vm_id)
{
    VM *v = vm_find(s, vm_id);
    if (v == NULL) return STATUS_NOT_FOUND;

    printf("\n=== VM %d: %s ===\n", v->id, v->name);
    printf("Owner    : %s\n", v->owner);
    printf("Type     : %s\n", vm_type_str(v->type));
    printf("Status   : %s\n", vm_status_str(v->status));
    printf("Priority : %d\n\n", v->priority);

    printf("%-18s %10s %10s\n", "Resource", "Max Claim", "Allocated");
    printf("%-18s %10d %10d\n", "CPU (cores)",      v->max_claim.cpu_cores,      v->allocation.cpu_cores);
    printf("%-18s %10d %10d\n", "RAM (MB)",         v->max_claim.ram_mb,         v->allocation.ram_mb);
    printf("%-18s %10d %10d\n", "Storage (GB)",     v->max_claim.storage_gb,     v->allocation.storage_gb);
    printf("%-18s %10d %10d\n", "GPU (units)",      v->max_claim.gpu_units,      v->allocation.gpu_units);
    printf("%-18s %10d %10d\n", "Bandwidth (Mbps)", v->max_claim.bandwidth_mbps, v->allocation.bandwidth_mbps);
    printf("\n");
    return STATUS_OK;
}

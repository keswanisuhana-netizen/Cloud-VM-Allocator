#include <stdio.h>
#include <string.h>
#include <time.h>
#include "allocator.h"
#include "vm.h"

static safety_fn g_safety = NULL;

void alloc_set_safety_check(safety_fn fn) { g_safety = fn; }

/* ---------- ResourceVector helpers ---------- */
static bool rv_fits(ResourceVector a, ResourceVector b)   /* a <= b (har resource) */
{
    return a.cpu_cores      <= b.cpu_cores      &&
           a.ram_mb         <= b.ram_mb         &&
           a.storage_gb     <= b.storage_gb     &&
           a.gpu_units      <= b.gpu_units      &&
           a.bandwidth_mbps <= b.bandwidth_mbps;
}

static bool rv_nonneg(ResourceVector v)
{
    return v.cpu_cores >= 0 && v.ram_mb >= 0 && v.storage_gb >= 0 &&
           v.gpu_units >= 0 && v.bandwidth_mbps >= 0;
}

static bool rv_is_zero(ResourceVector v)
{
    return v.cpu_cores == 0 && v.ram_mb == 0 && v.storage_gb == 0 &&
           v.gpu_units == 0 && v.bandwidth_mbps == 0;
}

static ResourceVector rv_add(ResourceVector a, ResourceVector b)
{
    ResourceVector r = { a.cpu_cores + b.cpu_cores, a.ram_mb + b.ram_mb,
                         a.storage_gb + b.storage_gb, a.gpu_units + b.gpu_units,
                         a.bandwidth_mbps + b.bandwidth_mbps };
    return r;
}

static ResourceVector rv_sub(ResourceVector a, ResourceVector b)
{
    ResourceVector r = { a.cpu_cores - b.cpu_cores, a.ram_mb - b.ram_mb,
                         a.storage_gb - b.storage_gb, a.gpu_units - b.gpu_units,
                         a.bandwidth_mbps - b.bandwidth_mbps };
    return r;
}

/* ---------- Internal (lock pehle se pakda hua hai) ---------- */

/* Ek request grant karne ki koshish. Sirf yahi jagah ledger se resources lete hain. */
static status_t try_grant(SystemState *s, VM *vm, ResourceRequest *rq)
{
    if (!rv_fits(rq->requested, s->server.available))
        return STATUS_INSUFFICIENT_CAPACITY;

    if (g_safety != NULL && !g_safety(s, vm->id, rq->requested))
        return STATUS_UNSAFE_REQUEST;

    /* Commit: VM allocation aur server ledger ek saath badalte hain */
    vm->allocation      = rv_add(vm->allocation, rq->requested);
    s->server.allocated = rv_add(s->server.allocated, rq->requested);
    s->server.available = rv_sub(s->server.available, rq->requested);
    rq->status = REQUEST_GRANTED;

    if (vm->status == VM_STOPPED || vm->status == VM_WAITING)
        vm->status = VM_RUNNING;

    return STATUS_OK;
}

static bool has_pending(const SystemState *s, int vm_id)
{
    for (int i = 0; i < s->request_count; i++) {
        if (s->requests[i].vm_id == vm_id && s->requests[i].status == REQUEST_QUEUED)
            return true;
    }
    return false;
}

/* Queue ko purani request se nayi ki taraf try karta hai.
   Jo request fit nahi hoti wo queued rehti hai, agli try hoti hai. */
static void retry_locked(SystemState *s)
{
    for (int i = 0; i < s->request_count; i++) {
        ResourceRequest *rq = &s->requests[i];
        if (rq->status != REQUEST_QUEUED) continue;

        VM *vm = vm_find(s, rq->vm_id);
        if (vm == NULL || vm->status == VM_TERMINATED) {
            rq->status = REQUEST_REJECTED;
            continue;
        }
        try_grant(s, vm, rq);
    }
}

/* ---------- Public API ---------- */

status_t alloc_request(SystemState *s, int vm_id, ResourceVector req,
                       int *out_request_id)
{
    if (s == NULL || !s->server.configured) return STATUS_INVALID_INPUT;

    pthread_mutex_lock(&s->lock);
    status_t result;

    VM *vm = vm_find(s, vm_id);
    if (vm == NULL) { result = STATUS_NOT_FOUND; goto done; }

    if (vm->status == VM_TERMINATED || !rv_nonneg(req) || rv_is_zero(req)) {
        result = STATUS_INVALID_INPUT; goto done;
    }

    /* Jo pehle se hai + ye nayi request, declared max claim se zyada nahi honi chahiye */
    if (!rv_fits(rv_add(vm->allocation, req), vm->max_claim)) {
        result = STATUS_INVALID_INPUT; goto done;
    }

    if (has_pending(s, vm_id)) { result = STATUS_INVALID_INPUT; goto done; }

    if (s->request_count >= MAX_REQUESTS) { result = STATUS_SYSTEM_ERROR; goto done; }

    ResourceRequest *rq = &s->requests[s->request_count++];
    memset(rq, 0, sizeof(*rq));
    rq->request_id = s->next_request_id++;
    rq->vm_id      = vm_id;
    rq->requested  = req;
    rq->priority   = vm->priority;
    rq->status     = REQUEST_PENDING;
    rq->timestamp  = time(NULL);
    if (out_request_id) *out_request_id = rq->request_id;

    if (try_grant(s, vm, rq) == STATUS_OK) {
        result = STATUS_OK;
    } else {
        rq->status = REQUEST_QUEUED;
        if (vm->status == VM_STOPPED) vm->status = VM_WAITING;
        result = STATUS_QUEUED;
    }

done:
    pthread_mutex_unlock(&s->lock);
    return result;
}

status_t alloc_release(SystemState *s, int vm_id)
{
    if (s == NULL) return STATUS_INVALID_INPUT;

    pthread_mutex_lock(&s->lock);
    status_t result = STATUS_OK;

    VM *vm = vm_find(s, vm_id);
    if (vm == NULL) { result = STATUS_NOT_FOUND; goto done; }

    /* Is VM ki waiting requests cancel */
    int cancelled = 0;
    for (int i = 0; i < s->request_count; i++) {
        if (s->requests[i].vm_id == vm_id && s->requests[i].status == REQUEST_QUEUED) {
            s->requests[i].status = REQUEST_REJECTED;
            cancelled++;
        }
    }

    bool holds = !rv_is_zero(vm->allocation);
    if (!holds && cancelled == 0) {          /* duplicate release */
        result = STATUS_INVALID_INPUT; goto done;
    }

    if (holds) {
        if (!rv_fits(vm->allocation, s->server.allocated)) {   /* ledger kharab */
            result = STATUS_SYSTEM_ERROR; goto done;
        }
        s->server.allocated = rv_sub(s->server.allocated, vm->allocation);
        s->server.available = rv_add(s->server.available, vm->allocation);
        memset(&vm->allocation, 0, sizeof(vm->allocation));
        memset(&vm->usage, 0, sizeof(vm->usage));
    }

    if (vm->status != VM_TERMINATED) vm->status = VM_STOPPED;

    retry_locked(s);      /* resources wapas aaye, waiting wale dekho */

done:
    pthread_mutex_unlock(&s->lock);
    return result;
}

status_t alloc_retry_queue(SystemState *s)
{
    if (s == NULL) return STATUS_INVALID_INPUT;
    pthread_mutex_lock(&s->lock);
    retry_locked(s);
    pthread_mutex_unlock(&s->lock);
    return STATUS_OK;
}

bool alloc_check_invariant(const SystemState *s)
{
    if (s == NULL || !s->server.configured) return false;

    ResourceVector sum = {0, 0, 0, 0, 0};
    for (int i = 0; i < s->vm_count; i++) {
        const VM *v = &s->vms[i];
        if (!rv_nonneg(v->allocation)) return false;
        sum = rv_add(sum, v->allocation);

        if (v->status == VM_RUNNING && rv_is_zero(v->allocation)) return false;
        if ((v->status == VM_STOPPED || v->status == VM_TERMINATED) &&
            !rv_is_zero(v->allocation)) return false;
    }

    ResourceVector expect_avail = rv_sub(s->server.total, sum);
    if (!rv_nonneg(s->server.available)) return false;
    if (memcmp(&s->server.allocated, &sum, sizeof(sum)) != 0) return false;
    if (memcmp(&s->server.available, &expect_avail, sizeof(sum)) != 0) return false;
    return true;
}

int alloc_queue_length(const SystemState *s)
{
    int n = 0;
    if (s == NULL) return 0;
    for (int i = 0; i < s->request_count; i++)
        if (s->requests[i].status == REQUEST_QUEUED) n++;
    return n;
}

void alloc_display_matrix(const SystemState *s)
{
    if (s == NULL || !s->server.configured) { printf("Server configure nahi hua.\n"); return; }

    printf("\n=== Allocation Matrix ===\n");
    printf("%-4s %-14s %-10s %5s %8s %8s %4s %6s\n",
           "ID", "VM", "Status", "CPU", "RAM(MB)", "Stor(GB)", "GPU", "BW");
    for (int i = 0; i < s->vm_count; i++) {
        const VM *v = &s->vms[i];
        printf("%-4d %-14s %-10s %5d %8d %8d %4d %6d\n",
               v->id, v->name, vm_status_str(v->status),
               v->allocation.cpu_cores, v->allocation.ram_mb, v->allocation.storage_gb,
               v->allocation.gpu_units, v->allocation.bandwidth_mbps);
    }
    const ResourceVector *a = &s->server.available;
    printf("%-4s %-14s %-10s %5d %8d %8d %4d %6d\n", "-", "AVAILABLE", "-",
           a->cpu_cores, a->ram_mb, a->storage_gb, a->gpu_units, a->bandwidth_mbps);
    printf("\n");
}

void alloc_display_queue(const SystemState *s)
{
    if (s == NULL) return;
    if (alloc_queue_length(s) == 0) { printf("Waiting queue khali hai.\n"); return; }

    printf("\n=== Waiting Queue ===\n");
    printf("%-6s %-5s %5s %8s %8s %4s %6s\n", "Req", "VM", "CPU", "RAM(MB)", "Stor(GB)", "GPU", "BW");
    for (int i = 0; i < s->request_count; i++) {
        const ResourceRequest *r = &s->requests[i];
        if (r->status != REQUEST_QUEUED) continue;
        printf("%-6d %-5d %5d %8d %8d %4d %6d\n", r->request_id, r->vm_id,
               r->requested.cpu_cores, r->requested.ram_mb, r->requested.storage_gb,
               r->requested.gpu_units, r->requested.bandwidth_mbps);
    }
    printf("\n");
}

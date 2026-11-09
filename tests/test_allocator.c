#include <stdio.h>
#include <assert.h>
#include "server.h"
#include "vm.h"
#include "allocator.h"

static bool g_safe = true;
static bool test_hook(const SystemState *s, int vm_id, ResourceVector req)
{
    (void)s; (void)vm_id; (void)req;
    return g_safe;
}

int main(void)
{
    SystemState s;
    ResourceVector cap = {8, 16384, 500, 2, 1000};
    int a, b, c, d, e, rid;

    assert(system_init(&s) == STATUS_OK);
    assert(server_init(&s, cap) == STATUS_OK);

    ResourceVector mA = {2, 4096, 50, 0, 100};
    ResourceVector mB = {4, 8192, 100, 0, 200};
    ResourceVector mC = {4, 6144, 100, 0, 200};
    ResourceVector mD = {4, 4096, 40, 0, 100};
    ResourceVector mE = {8, 1024, 10, 0, 10};
    vm_create(&s, "VM-A", "ali",  VM_WEB_HOSTING, mA, 3, &a);
    vm_create(&s, "VM-B", "ali",  VM_DATABASE,    mB, 3, &b);
    vm_create(&s, "VM-C", "sara", VM_GENERAL,     mC, 3, &c);
    vm_create(&s, "VM-D", "sara", VM_GENERAL,     mD, 3, &d);
    vm_create(&s, "VM-E", "sara", VM_GENERAL,     mE, 3, &e);

    /* 1. A aur B granted */
    assert(alloc_request(&s, a, mA, &rid) == STATUS_OK);
    assert(alloc_request(&s, b, mB, &rid) == STATUS_OK);
    assert(vm_find(&s, a)->status == VM_RUNNING);
    assert(s.server.available.cpu_cores == 2 && s.server.available.ram_mb == 4096);
    assert(alloc_check_invariant(&s));
    printf("[PASS] A and B granted, available = 2 cores / 4096 MB\n");

    /* 2. C fit nahi hota: queue, VM Waiting */
    assert(alloc_request(&s, c, mC, &rid) == STATUS_QUEUED);
    assert(vm_find(&s, c)->status == VM_WAITING);
    assert(alloc_queue_length(&s) == 1);
    assert(s.server.available.cpu_cores == 2);
    assert(alloc_check_invariant(&s));
    alloc_display_matrix(&s);
    alloc_display_queue(&s);
    printf("[PASS] C queued, VM Waiting, ledger unchanged\n");

    /* 3. A stop: resources wapas, C automatically grant */
    assert(alloc_release(&s, a) == STATUS_OK);
    assert(vm_find(&s, a)->status == VM_STOPPED);
    assert(vm_find(&s, c)->status == VM_RUNNING);
    assert(alloc_queue_length(&s) == 0);
    assert(s.server.available.cpu_cores == 0 && s.server.available.ram_mb == 2048);
    assert(alloc_check_invariant(&s));
    printf("[PASS] release A -> queue retried -> C granted\n");

    /* 4. duplicate release: resources dobara add nahi hote */
    ResourceVector before = s.server.available;
    assert(alloc_release(&s, a) == STATUS_INVALID_INPUT);
    assert(s.server.available.cpu_cores == before.cpu_cores);
    assert(s.server.available.ram_mb == before.ram_mb);
    assert(alloc_check_invariant(&s));
    printf("[PASS] duplicate release ignored\n");

    /* 5. invalid requests */
    ResourceVector zero = {0, 0, 0, 0, 0};
    ResourceVector neg  = {-1, 100, 10, 0, 0};
    ResourceVector over = {3, 1024, 10, 0, 10};     /* A ka max 2 cores */
    assert(alloc_request(&s, a, zero, &rid) == STATUS_INVALID_INPUT);
    assert(alloc_request(&s, a, neg,  &rid) == STATUS_INVALID_INPUT);
    assert(alloc_request(&s, a, over, &rid) == STATUS_INVALID_INPUT);
    assert(alloc_request(&s, 99, mA,  &rid) == STATUS_NOT_FOUND);
    assert(alloc_check_invariant(&s));
    printf("[PASS] zero/negative/over-max/unknown VM rejected\n");

    /* 6. B ke paas pehle se max claim hai, aur maangna reject */
    ResourceVector extra = {1, 1, 1, 0, 0};
    assert(alloc_request(&s, b, extra, &rid) == STATUS_INVALID_INPUT);
    printf("[PASS] request beyond max claim rejected\n");

    /* 7. D queue mein, dobara request reject */
    assert(alloc_request(&s, d, mD, &rid) == STATUS_QUEUED);
    assert(alloc_request(&s, d, mD, &rid) == STATUS_INVALID_INPUT);
    assert(alloc_queue_length(&s) == 1);
    printf("[PASS] duplicate pending request rejected\n");

    /* 8. E bhi queue mein; B release par D (purani) pehle grant, E waiting */
    assert(alloc_request(&s, e, mE, &rid) == STATUS_QUEUED);
    assert(alloc_queue_length(&s) == 2);
    assert(alloc_release(&s, b) == STATUS_OK);
    assert(vm_find(&s, d)->status == VM_RUNNING);
    assert(vm_find(&s, e)->status == VM_WAITING);
    assert(alloc_queue_length(&s) == 1);
    assert(alloc_check_invariant(&s));
    printf("[PASS] queue retried in order: D granted, E still waiting\n");

    /* 9. C aur D release hone par E (8 cores) grant, phir sab wapas */
    assert(alloc_release(&s, c) == STATUS_OK);
    assert(vm_find(&s, e)->status == VM_WAITING);
    assert(alloc_release(&s, d) == STATUS_OK);
    assert(vm_find(&s, e)->status == VM_RUNNING);
    assert(alloc_check_invariant(&s));
    assert(alloc_release(&s, e) == STATUS_OK);
    assert(s.server.available.cpu_cores == 8 && s.server.available.ram_mb == 16384);
    assert(s.server.allocated.cpu_cores == 0 && s.server.allocated.ram_mb == 0);
    assert(alloc_check_invariant(&s));
    printf("[PASS] all released, available == total\n");

    /* 10. safety hook: unsafe to queue, safe hone par retry se grant */
    alloc_set_safety_check(test_hook);
    g_safe = false;
    assert(alloc_request(&s, a, mA, &rid) == STATUS_QUEUED);
    assert(vm_find(&s, a)->status == VM_WAITING);
    g_safe = true;
    assert(alloc_retry_queue(&s) == STATUS_OK);
    assert(vm_find(&s, a)->status == VM_RUNNING);
    assert(alloc_check_invariant(&s));
    assert(alloc_release(&s, a) == STATUS_OK);
    alloc_set_safety_check(NULL);
    printf("[PASS] safety hook: unsafe queued, granted after safe\n");

    /* 11. terminated VM request reject */
    vm_find(&s, a)->status = VM_TERMINATED;
    assert(alloc_request(&s, a, mA, &rid) == STATUS_INVALID_INPUT);
    assert(alloc_check_invariant(&s));
    printf("[PASS] terminated VM cannot request\n");

    printf("\nSAARE ALLOCATOR TESTS PASS HO GAYE\n");
    return 0;
}

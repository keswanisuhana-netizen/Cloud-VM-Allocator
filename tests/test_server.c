#include <stdio.h>
#include <assert.h>
#include "server.h"

int main(void)
{
    SystemState s;
    ResourceVector good = {8, 16384, 500, 2, 1000};

    assert(system_init(&s) == STATUS_OK);
    assert(s.next_vm_id == 1 && s.next_request_id == 1);
    printf("[PASS] system_init\n");

    ResourceVector zero_cpu = {0, 16384, 500, 2, 1000};
    ResourceVector neg_ram  = {8, -5, 500, 2, 1000};
    ResourceVector huge_cpu = {99999, 16384, 500, 2, 1000};
    assert(server_init(&s, zero_cpu) == STATUS_INVALID_INPUT);
    assert(server_init(&s, neg_ram)  == STATUS_INVALID_INPUT);
    assert(server_init(&s, huge_cpu) == STATUS_INVALID_INPUT);
    assert(s.server.configured == false);
    printf("[PASS] invalid capacity rejected\n");

    assert(server_init(&s, good) == STATUS_OK);
    assert(s.server.configured == true);
    assert(s.server.available.cpu_cores == 8);
    assert(s.server.available.ram_mb == 16384);
    assert(s.server.allocated.cpu_cores == 0);
    printf("[PASS] valid capacity accepted, available == total\n");

    server_display(&s);

    assert(server_init(&s, good) == STATUS_INVALID_INPUT);
    printf("[PASS] double init rejected\n");

    s.server.allocated.cpu_cores = 2;
    assert(server_reset(&s) == STATUS_INVALID_INPUT);
    assert(s.server.configured == true);
    printf("[PASS] reset blocked while resources allocated\n");

    s.server.allocated.cpu_cores = 0;
    assert(server_reset(&s) == STATUS_OK);
    assert(s.server.configured == false);
    printf("[PASS] reset allowed when nothing allocated\n");

    printf("\nSAARE TESTS PASS HO GAYE\n");
    return 0;
}

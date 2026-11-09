#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "server.h"
#include "vm.h"

int main(void)
{
    SystemState s;
    ResourceVector cap   = {8, 16384, 500, 2, 1000};
    ResourceVector small = {2, 4096, 50, 0, 100};
    int id = 0;

    assert(system_init(&s) == STATUS_OK);

    /* 1. server configure nahi hai to VM nahi banegi */
    assert(vm_create(&s, "A", "ali", VM_WEB_HOSTING, small, 3, &id) == STATUS_INVALID_INPUT);
    printf("[PASS] create rejected before server configured\n");

    assert(server_init(&s, cap) == STATUS_OK);

    /* 2. pehli VM: ID 1, Stopped, allocation zero */
    assert(vm_create(&s, "VM-A", "ali", VM_WEB_HOSTING, small, 3, &id) == STATUS_OK);
    assert(id == 1);
    VM *v = vm_find(&s, 1);
    assert(v != NULL);
    assert(v->status == VM_STOPPED);
    assert(v->allocation.cpu_cores == 0 && v->allocation.ram_mb == 0);
    assert(strcmp(v->name, "VM-A") == 0);
    printf("[PASS] first VM: id=1, Stopped, no allocation\n");

    /* 3. dusri VM: ID 2 (unique) */
    assert(vm_create(&s, "VM-B", "sara", VM_DATABASE, small, 5, &id) == STATUS_OK);
    assert(id == 2 && s.vm_count == 2);
    printf("[PASS] second VM gets unique id=2\n");

    /* 4. galat inputs reject */
    ResourceVector toobig  = {16, 4096, 50, 0, 100};   /* 16 cores > 8 */
    ResourceVector zerocpu = {0, 4096, 50, 0, 100};
    assert(vm_create(&s, "X", "o", VM_GENERAL, toobig, 3, &id)  == STATUS_INSUFFICIENT_CAPACITY);
    assert(vm_create(&s, "X", "o", VM_GENERAL, zerocpu, 3, &id) == STATUS_INVALID_INPUT);
    assert(vm_create(&s, "",  "o", VM_GENERAL, small, 3, &id)   == STATUS_INVALID_INPUT);
    assert(vm_create(&s, "X", "",  VM_GENERAL, small, 3, &id)   == STATUS_INVALID_INPUT);
    assert(vm_create(&s, "X", "o", VM_GENERAL, small, 0, &id)   == STATUS_INVALID_INPUT);
    assert(vm_create(&s, "X", "o", VM_GENERAL, small, 6, &id)   == STATUS_INVALID_INPUT);
    assert(s.vm_count == 2);
    printf("[PASS] invalid VM requests rejected, count unchanged\n");

    /* 5. bohat lamba naam kat jata hai, overflow nahi hota */
    char longname[100];
    memset(longname, 'a', 99); longname[99] = '\0';
    assert(vm_create(&s, longname, "o", VM_GENERAL, small, 3, &id) == STATUS_OK);
    assert(strlen(vm_find(&s, id)->name) < MAX_NAME_LENGTH);
    printf("[PASS] long name truncated safely\n");

    /* 6. find / details */
    assert(vm_find(&s, 99) == NULL);
    assert(vm_details(&s, 99) == STATUS_NOT_FOUND);
    assert(vm_details(&s, 1) == STATUS_OK);
    printf("[PASS] find/details for missing and existing VM\n");

    vm_list(&s);

    /* 7. VM table bhar jaye to reject */
    while (s.vm_count < MAX_VMS)
        assert(vm_create(&s, "fill", "o", VM_GENERAL, small, 1, &id) == STATUS_OK);
    assert(vm_create(&s, "extra", "o", VM_GENERAL, small, 1, &id) == STATUS_SYSTEM_ERROR);
    printf("[PASS] full VM table rejected\n");

    printf("\nSAARE VM TESTS PASS HO GAYE\n");
    return 0;
}

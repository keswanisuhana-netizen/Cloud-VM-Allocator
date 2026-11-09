#ifndef VM_H
#define VM_H

#include "common.h"

/* Nayi VM banata hai (status = VM_STOPPED), unique ID out_id mein deta hai.
   Reject hota hai agar: server configure nahi, naam/owner khali, priority 1..5
   se bahar, max_claim galat (CPU/RAM/storage > 0 chahiye), ya VM table bhar gayi.
   max_claim server total se bada ho to STATUS_INSUFFICIENT_CAPACITY. */
status_t vm_create(SystemState *s, const char *name, const char *owner,
                   VmType type, ResourceVector max_claim, int priority,
                   int *out_id);

/* ID se VM dhundta hai. Na mile to NULL. */
VM *vm_find(SystemState *s, int vm_id);

/* Saari VMs ka table print karta hai */
void vm_list(const SystemState *s);

/* Ek VM ki poori detail print karta hai. Na mile to STATUS_NOT_FOUND. */
status_t vm_details(SystemState *s, int vm_id);

/* Enum ko text mein badalte hain (display ke liye) */
const char *vm_status_str(VmStatus st);
const char *vm_type_str(VmType t);

#endif

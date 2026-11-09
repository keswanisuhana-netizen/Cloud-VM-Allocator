#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "common.h"

/* Safety hook: Member 3 (Banker's) yahan apna function lagayega.
   true = safe, false = unsafe. NULL = koi safety check nahi.
   NOTE: hook ke andar alloc_* functions call mat karna (lock phasega). */
typedef bool (*safety_fn)(const SystemState *s, int vm_id, ResourceVector req);
void alloc_set_safety_check(safety_fn fn);

/* VM ke liye resources maangta hai (start ya extra request).
   Return:
     STATUS_OK            -> granted, VM Running
     STATUS_QUEUED        -> abhi nahi mil sakta, queue mein gaya, VM Waiting
     STATUS_NOT_FOUND     -> VM maujood nahi
     STATUS_INVALID_INPUT -> zero/negative request, max claim se zyada,
                             terminated VM, ya is VM ki request pehle se queue mein
     STATUS_SYSTEM_ERROR  -> request table bhar gayi
   out_request_id (NULL ho sakta hai) mein request ID milti hai. */
status_t alloc_request(SystemState *s, int vm_id, ResourceVector req,
                       int *out_request_id);

/* VM ke SAARE resources wapas karta hai, uski queued requests cancel karta hai,
   VM ko Stopped karta hai, phir waiting queue retry karta hai.
   Duplicate release (kuch hold nahi) -> STATUS_INVALID_INPUT, ledger nahi badalta. */
status_t alloc_release(SystemState *s, int vm_id);

/* Waiting queue dobara try karta hai. Return: STATUS_OK */
status_t alloc_retry_queue(SystemState *s);

/* Ledger sahi hai? available = total - sum(allocations), koi negative nahi,
   Running VM ke paas allocation, Stopped/Terminated ke paas zero. */
bool alloc_check_invariant(const SystemState *s);

int  alloc_queue_length(const SystemState *s);
void alloc_display_matrix(const SystemState *s);
void alloc_display_queue(const SystemState *s);

#endif

#ifndef SERVER_H
#define SERVER_H

#include "common.h"

/* SystemState zero karta hai, ID counters 1 se shuru, mutex init */
status_t system_init(SystemState *s);

/* Capacity set karta hai: available = total, allocated = 0.
   Galat values par STATUS_INVALID_INPUT. Pehle se configured ho to bhi reject. */
status_t server_init(SystemState *s, ResourceVector total);

/* Reset sirf tab jab koi resource allocated na ho */
status_t server_reset(SystemState *s);

/* Total / Allocated / Available ka table print karta hai */
void server_display(const SystemState *s);

#endif

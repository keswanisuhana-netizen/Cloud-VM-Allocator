#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define MAX_VMS 50
#define MAX_REQUESTS 100
#define MAX_NAME_LENGTH 50
#define MAX_OWNER_LENGTH 50

/* Common operation status */
typedef enum {
    STATUS_OK,
    STATUS_INVALID_INPUT,
    STATUS_INSUFFICIENT_CAPACITY,
    STATUS_UNSAFE_REQUEST,
    STATUS_QUEUED,
    STATUS_NOT_FOUND,
    STATUS_SYSTEM_ERROR
} status_t;

/* VM categories */
typedef enum {
    VM_AI_TRAINING,
    VM_DATABASE,
    VM_WEB_HOSTING,
    VM_GENERAL
} VmType;

/* VM lifecycle status */
typedef enum {
    VM_STOPPED,
    VM_WAITING,
    VM_RUNNING,
    VM_TERMINATED
} VmStatus;

/* Resource request status */
typedef enum {
    REQUEST_PENDING,
    REQUEST_QUEUED,
    REQUEST_GRANTED,
    REQUEST_REJECTED
} RequestStatus;

/* Units: CPU = cores, RAM = MB, Storage = GB, GPU = units, Bandwidth = Mbps */
typedef struct {
    int cpu_cores;
    int ram_mb;
    int storage_gb;
    int gpu_units;
    int bandwidth_mbps;
} ResourceVector;

/* Cloud server information */
typedef struct {
    ResourceVector total;
    ResourceVector available;
    ResourceVector allocated;
    bool configured;
} Server;

/* Virtual machine information */
typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    char owner[MAX_OWNER_LENGTH];

    VmType type;
    VmStatus status;

    int priority;                 /* 1 = low ... 5 = high */

    ResourceVector max_claim;     /* maximum VM may ever need */
    ResourceVector allocation;    /* currently held */
    ResourceVector usage;         /* simulated current usage */
} VM;

/* Resource request information */
typedef struct {
    int request_id;
    int vm_id;

    ResourceVector requested;

    int priority;
    RequestStatus status;
    time_t timestamp;
} ResourceRequest;

/* Complete application state */
typedef struct {
    Server server;

    VM vms[MAX_VMS];
    int vm_count;
    int next_vm_id;               /* starts at 1 */

    ResourceRequest requests[MAX_REQUESTS];
    int request_count;
    int next_request_id;          /* starts at 1 */

    pthread_mutex_t lock;         /* protects ledger (allocator owns it) */
} SystemState;

#endif

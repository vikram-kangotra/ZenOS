#ifndef _KERNEL_SYNC_H
#define _KERNEL_SYNC_H

#include <stdbool.h>
#include <stdint.h>

// Spinlock implementation
typedef struct {
    volatile uint32_t lock;
    const char* name;
} spinlock_t;

// Initialize a spinlock
void spinlock_init(spinlock_t* lock, const char* name);

// Acquire a spinlock
void spinlock_acquire(spinlock_t* lock);

// Release a spinlock
void spinlock_release(spinlock_t* lock);

// Try to acquire a spinlock (non-blocking)
bool spinlock_try_acquire(spinlock_t* lock);

// Mutex implementation
typedef struct {
    volatile uint32_t lock;
    const char* name;
    volatile uint32_t owner;
} mutex_t;

// Initialize a mutex
void mutex_init(mutex_t* mutex, const char* name);

// Acquire a mutex
void mutex_acquire(mutex_t* mutex);

// Release a mutex
void mutex_release(mutex_t* mutex);

// Try to acquire a mutex (non-blocking)
bool mutex_try_acquire(mutex_t* mutex);

// Semaphore implementation
typedef struct {
    volatile int32_t count;      // Current count of available resources
    const char* name;            // Name for debugging
    volatile uint32_t waiters;   // Number of waiting threads
} semaphore_t;

// Initialize a semaphore with initial count
void semaphore_init(semaphore_t* sem, int32_t initial_count, const char* name);

// Wait (P) operation - decrement count or block if count is 0
void semaphore_wait(semaphore_t* sem);

// Signal (V) operation - increment count and wake up a waiting thread if any
void semaphore_signal(semaphore_t* sem);

// Try to wait (non-blocking) - returns true if successful
bool semaphore_try_wait(semaphore_t* sem);

#endif // _KERNEL_SYNC_H 
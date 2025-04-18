#include <kernel/sync.h>
#include <arch/x86_64/io.h>
#include <kernel/kprintf.h>

// Spinlock implementation
void spinlock_init(spinlock_t* lock, const char* name) {
    lock->lock = 0;
    lock->name = name;
}

void spinlock_acquire(spinlock_t* lock) {
    while (__sync_lock_test_and_set(&lock->lock, 1)) {
        // Wait while the lock is held
        __asm__ volatile("pause");
    }
}

void spinlock_release(spinlock_t* lock) {
    __sync_lock_release(&lock->lock);
}

bool spinlock_try_acquire(spinlock_t* lock) {
    return !__sync_lock_test_and_set(&lock->lock, 1);
}

// Mutex implementation
void mutex_init(mutex_t* mutex, const char* name) {
    mutex->lock = 0;
    mutex->name = name;
    mutex->owner = 0;
}

void mutex_acquire(mutex_t* mutex) {
    while (__sync_lock_test_and_set(&mutex->lock, 1)) {
        // Wait while the lock is held
        __asm__ volatile("pause");
    }
    mutex->owner = 1; // TODO: Replace with actual thread ID when threading is implemented
}

void mutex_release(mutex_t* mutex) {
    if (mutex->owner != 1) { // TODO: Replace with actual thread ID check
        kprintf(ERROR, "Mutex %s released by non-owner\n", mutex->name);
        return;
    }
    mutex->owner = 0;
    __sync_lock_release(&mutex->lock);
}

bool mutex_try_acquire(mutex_t* mutex) {
    if (!__sync_lock_test_and_set(&mutex->lock, 1)) {
        mutex->owner = 1; // TODO: Replace with actual thread ID
        return true;
    }
    return false;
}

// Semaphore implementation
void semaphore_init(semaphore_t* sem, int32_t initial_count, const char* name) {
    sem->count = initial_count;
    sem->name = name;
    sem->waiters = 0;
}

void semaphore_wait(semaphore_t* sem) {
    while (1) {
        // Atomically check and decrement count if > 0
        int32_t old_count = sem->count;
        if (old_count > 0) {
            if (__sync_val_compare_and_swap(&sem->count, old_count, old_count - 1) == old_count) {
                return; // Successfully acquired the semaphore
            }
        }
        
        // If we get here, count was 0 or changed during our check
        __sync_fetch_and_add(&sem->waiters, 1);
        // In a real OS, we would block the thread here
        __asm__ volatile("pause");
        __sync_fetch_and_sub(&sem->waiters, 1);
    }
}

void semaphore_signal(semaphore_t* sem) {
    __sync_fetch_and_add(&sem->count, 1);
    // In a real OS, we would wake up a waiting thread here
}

bool semaphore_try_wait(semaphore_t* sem) {
    while (1) {
        int32_t old_count = sem->count;
        if (old_count <= 0) {
            return false; // No resources available
        }
        if (__sync_val_compare_and_swap(&sem->count, old_count, old_count - 1) == old_count) {
            return true; // Successfully acquired the semaphore
        }
    }
} 
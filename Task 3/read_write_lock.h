#ifndef READ_WRITE_LOCK_H
#define READ_WRITE_LOCK_H

#include <pthread.h>
#include <stdbool.h>

// lock priority mods
typedef enum
{
    READERS_PRIORITY,
    WRITERS_PRIORITY
} rw_priority;

// custom read-write lock type
typedef struct read_write_lock
{
    pthread_mutex_t mutex;
    pthread_cond_t readers_cond;
    pthread_cond_t writers_cond;
    int current_readers;
    bool is_there_any_writer;
    int waiting_readers;
    int waiting_writers;
    rw_priority priority;
} read_write_lock;

void read_write_lock_init(read_write_lock *lock, rw_priority priority);
void read_lock(read_write_lock *lock);
void write_lock(read_write_lock *lock);
void read_write_unlock(read_write_lock *lock);
void read_write_destroy(read_write_lock *lock);

#endif
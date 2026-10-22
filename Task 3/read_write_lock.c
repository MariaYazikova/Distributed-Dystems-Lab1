#include "read_write_lock.h"

// initialization of lock state and synchronization primitives
void read_write_lock_init(read_write_lock *lock, rw_priority priority)
{
    lock->priority = priority;
    lock->current_readers = 0;
    lock->is_there_any_writer = false;
    lock->waiting_readers = 0;
    lock->waiting_writers = 0;
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->readers_cond, NULL);
    pthread_cond_init(&lock->writers_cond, NULL);
}

// shared access for readers
void read_lock(read_write_lock *lock)
{
    pthread_mutex_lock(&lock->mutex);
    lock->waiting_readers++;
    while (lock->is_there_any_writer ||
           (lock->priority == WRITERS_PRIORITY &&
            lock->waiting_writers > 0))
    {
        pthread_cond_wait(&lock->readers_cond, &lock->mutex);
    }
    lock->waiting_readers--;
    lock->current_readers++;
    pthread_mutex_unlock(&lock->mutex);
}

// exclusive access for writers
void write_lock(read_write_lock *lock)
{
    pthread_mutex_lock(&lock->mutex);
    lock->waiting_writers++;
    while (lock->is_there_any_writer || lock->current_readers > 0)
    {
        pthread_cond_wait(&lock->writers_cond, &lock->mutex);
    }
    lock->waiting_writers--;
    lock->is_there_any_writer = true;
    pthread_mutex_unlock(&lock->mutex);
}

// release of acess and notification of waiting threads
void read_write_unlock(read_write_lock *lock)
{
    pthread_mutex_lock(&lock->mutex);
    if (lock->is_there_any_writer)
    {
        lock->is_there_any_writer = false;
        if (lock->priority == WRITERS_PRIORITY)
        {
            if (lock->waiting_writers > 0)
            {
                pthread_cond_signal(&lock->writers_cond);
            }
            else if (lock->waiting_readers > 0)
            {
                pthread_cond_broadcast(&lock->readers_cond);
            }
        }
        else
        {
            if (lock->waiting_readers > 0)
            {
                pthread_cond_broadcast(&lock->readers_cond);
            }
            else if (lock->waiting_writers > 0)
            {
                pthread_cond_signal(&lock->writers_cond);
            }
        }
    }
    else
    {
        lock->current_readers--;
        if (lock->current_readers == 0 && lock->waiting_writers > 0)
        {
            pthread_cond_signal(&lock->writers_cond);
        }
    }
    pthread_mutex_unlock(&lock->mutex);
}

// synchronization primitives cleanup
void read_write_destroy(read_write_lock *lock)
{
    pthread_cond_destroy(&lock->readers_cond);
    pthread_cond_destroy(&lock->writers_cond);
    pthread_mutex_destroy(&lock->mutex);
}
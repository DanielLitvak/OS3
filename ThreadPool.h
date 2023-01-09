#ifndef OS3_THREADPOOL_H
#define OS3_THREADPOOL_H

#include <sys/time.h>
#include <pthread.h>
#include "RingBuffer.h"
#include "defenitions.h"

typedef struct{
    Thread* array;
    int num_of_threads;
} ThreadPool;

typedef struct {
    Thread* thread;
    RingBuffer* ringbuffer;
} HandleRequestArgs;

ThreadPool* create_threadpool(int num_of_threads, RingBuffer* ringbuffer);

void* handle_request(void*);

#endif //OS3_THREADPOOL_H

#include <stdlib.h>
#include "segel.h"
#include "defenitions.h"
#include "ThreadPool.h"
#include "RingBuffer.h"
#include "request.h"


ThreadPool* create_threadpool(int num_of_threads, RingBuffer* ringbuffer)
{
    ThreadPool* new_threadpool = (ThreadPool*) malloc(sizeof(*new_threadpool));
    Thread* new_array = (Thread*) malloc(sizeof(Thread) * num_of_threads);
    new_threadpool->array = new_array;
    new_threadpool->num_of_threads = num_of_threads;
    for(int i = 0; i < num_of_threads; ++i)
    {
        HandleRequestArgs* arg = malloc(sizeof(HandleRequestArgs));
        arg->thread = &new_array[i];
        arg->ringbuffer = ringbuffer;
        pthread_create(&new_array[i].thread, NULL ,handle_request , (void*)arg);
        new_array[i].count = 0;
        new_array[i].static_count = 0;
        new_array[i].dynamic_count = 0;
        new_array[i].id = i;
    }
    return new_threadpool;
}

void* handle_request(void * _arg){
    HandleRequestArgs* arg = (HandleRequestArgs*)_arg;
    Thread* thread = arg->thread;
    RingBuffer* ringbuffer = arg->ringbuffer;
    struct timeval time;
    while(1)
    {
        //printf("thread %d is trying to lock mutex\n", thread->id);
        pthread_mutex_lock(&ringbuffer->lock);
        //printf("thread %d succeeded to lock mutex\n", thread->id);
        while(no_waiting_requests(ringbuffer)){
            //printf("thread %d is waiting for the buffer to have waiting requests\n", thread->id);
            pthread_cond_wait(&ringbuffer->not_empty ,&ringbuffer->lock);
        }
        //printf("thread %d is handling the task\n", thread->id);

		while(ringbuffer->array[ringbuffer->tail].is_running == 1)
			advance_tail(ringbuffer);
        ringbuffer->array[ringbuffer->tail].is_running = 1;
        Request request = ringbuffer->array[ringbuffer->tail];
        
		advance_tail(ringbuffer);
        ringbuffer->in_progress++;
        ringbuffer->waiting--;
        pthread_mutex_unlock(&ringbuffer->lock);

        gettimeofday(&time, NULL);
        timersub(&time, &request.arrival, &request.dispatch);
        request.thread_info = thread;

        requestHandle(&request);
        Close(request.fd);

        pthread_mutex_lock(&ringbuffer->lock);
        ringbuffer->in_progress--;
        pthread_cond_signal(&ringbuffer->not_full);
        pthread_mutex_unlock(&ringbuffer->lock);
    }
}

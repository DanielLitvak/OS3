#include <stdlib.h>
#include <assert.h>
#include "RingBuffer.h"
#include "defenitions.h"
#include <sys/time.h>
#include "segel.h"


 RingBuffer* create_ringbuffer(int max_size, enum _schedalg alg)
{
    RingBuffer* new_buffer = (RingBuffer*) malloc(sizeof(*new_buffer));
    Request* new_array = (Request*) malloc (sizeof(Request) * max_size);
    new_buffer->array = new_array;
    new_buffer->head = 0;
    new_buffer->tail = 0;
    new_buffer->waiting = 0;
    new_buffer->in_progress = 0;
    new_buffer->max_size = max_size;
    new_buffer->alg = alg;
    pthread_mutex_init(&new_buffer->lock, NULL);
    pthread_cond_init(&new_buffer->not_empty, NULL);
    pthread_cond_init(&new_buffer->not_full, NULL);
    return new_buffer;
}

void add_request(RingBuffer* ringbuffer, int fd)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    printf("main thread trying to access lock\n");
    pthread_mutex_lock(&ringbuffer->lock);
    printf("main thread got the lock\n");
    if (is_full(ringbuffer))
    {
        printf("buffer is full\n");
        if(!handle_overload(ringbuffer, fd))
            return;
    }
    printf("adding request\n");
    ringbuffer->array[ringbuffer->head].fd = fd;
    ringbuffer->array[ringbuffer->head].arrival = time;
    ringbuffer->head = (ringbuffer->head + 1) % ringbuffer->max_size;
    ringbuffer->waiting++;
    printf("signaling that the buffer is not empty\n");
    pthread_cond_signal(&ringbuffer->not_empty);
    pthread_mutex_unlock(&ringbuffer->lock);
}

void remove_request(RingBuffer* ringbuffer)
{
    pthread_mutex_lock(&ringbuffer->lock);
    advance_tail(ringbuffer);
    pthread_mutex_unlock(&ringbuffer->lock);
}

void advance_head(RingBuffer* ringbuffer)
{
    ringbuffer->head = (ringbuffer->head + 1) % ringbuffer->max_size;
}

void advance_tail(RingBuffer* ringbuffer)
{
//    pthread_mutex_lock(&ringbuffer->lock);
    ringbuffer->tail = (ringbuffer->tail + 1) % ringbuffer->max_size;
//    pthread_mutex_unlock(&ringbuffer->lock);
}

int is_full(RingBuffer* ringbuffer) // lock protected by outer function
{

    if ((ringbuffer->waiting + ringbuffer->in_progress) == ringbuffer->max_size)
    {
        printf("buffer is full, head: %d, tail: %d, waiting: %d, in progress: %d, max size: %d\n",ringbuffer->head,ringbuffer->tail, ringbuffer->waiting, ringbuffer->in_progress, ringbuffer->max_size);
        assert(ringbuffer->head == ringbuffer->tail);
        return 1;
    }
    else
        return 0;
}

int no_waiting_requests(RingBuffer* ringbuffer)
{
    if(ringbuffer->waiting == 0)
        return 1;
    else
        return 0;
}

int handle_overload(RingBuffer* ringbuffer, int fd) // lock protected by outer function
{
    switch(ringbuffer->alg){
        case BLOCK:
            while(is_full(ringbuffer)){
                printf("main thread is waiting for space in the buffer\n");
                pthread_cond_wait(&ringbuffer->not_full, &ringbuffer->lock);
            }
            printf("main thread received signal that there is space in the buffer\n");
            return 1;
        case DH:
            printf("dropping the head to make space\n");
            Close(ringbuffer->array[ringbuffer->tail].fd);
            return 1;
        case DT:
            printf("not enough space, dropping the the tail\n");
            Close(fd);
            return 0;
        case RANDOM:
            // TODO implement simple bonus
            printf("not yet implemented schedule algorithm please turn back");
            exit(1);
            break;
        default:
            printf("unknown schedule algorithm how did you get here?");
            exit(1);
            break;
    }
}
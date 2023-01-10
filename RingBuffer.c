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
    pthread_mutex_lock(&ringbuffer->lock);
    if (is_full(ringbuffer))
    {
        int status = handle_overload(ringbuffer, fd);
        if(!status)
        {
            pthread_cond_signal(&ringbuffer->not_empty);  // consider removing
            pthread_mutex_unlock(&ringbuffer->lock);
            return;
        }
        else if(status == 2) // drop head algorithm
        {
            if(ringbuffer->in_progress == ringbuffer->max_size)
            {
                Close(fd);
                pthread_cond_signal(&ringbuffer->not_empty);  // consider removing
                pthread_mutex_unlock(&ringbuffer->lock);
                return;
            }
            else{
                Close(ringbuffer->array[ringbuffer->tail].fd);
                ringbuffer->array[ringbuffer->tail].fd = fd;
                ringbuffer->array[ringbuffer->tail].arrival = time;
                ringbuffer->array[ringbuffer->tail].is_running = 0;
                do{
                    advance_tail(ringbuffer);
                } while(ringbuffer->array[ringbuffer->tail].is_running == 1);
                pthread_cond_signal(&ringbuffer->not_empty); // consider removing
                pthread_mutex_unlock(&ringbuffer->lock);
                return;
            }
        }
    }
    ringbuffer->array[ringbuffer->head].fd = fd;
    ringbuffer->array[ringbuffer->head].arrival = time;
    ringbuffer->array[ringbuffer->head].is_running = 0;
    advance_head(ringbuffer);
    ringbuffer->waiting++;
    pthread_cond_signal(&ringbuffer->not_empty);
    pthread_mutex_unlock(&ringbuffer->lock);
}

void remove_request(RingBuffer* ringbuffer)
{
    pthread_mutex_lock(&ringbuffer->lock);
    advance_tail(ringbuffer);
    pthread_mutex_unlock(&ringbuffer->lock);
}

void advance_head(RingBuffer* ringbuffer) // protected by outer function lock
{
    ringbuffer->head = (ringbuffer->head + 1) % ringbuffer->max_size;
}

void advance_tail(RingBuffer* ringbuffer) // protected by outer function lock
{
    ringbuffer->tail = (ringbuffer->tail + 1) % ringbuffer->max_size;
}

int is_full(RingBuffer* ringbuffer) // lock protected by outer function
{

    if ((ringbuffer->waiting + ringbuffer->in_progress) == ringbuffer->max_size)
    {
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
	int amount_to_drop = (ringbuffer->waiting / 2) + (ringbuffer->waiting % 2);
    switch(ringbuffer->alg){
        case BLOCK:
            while(is_full(ringbuffer)){
                //printf("main thread is waiting for space in the buffer\n");
                pthread_cond_wait(&ringbuffer->not_full, &ringbuffer->lock);
            }
            //printf("main thread received signal that there is space in the buffer\n");
            return 1;
        case DH:
            //printf("dropping the tail to make space\n");
            return 2;
        case DT:
            //printf("not enough space, dropping the incoming request\n");
            Close(fd);
            return 0;
        case RANDOM:
            if(ringbuffer->waiting == 0)
            {
				Close(fd);
				return 0;
			}
            for(int i = 0; i < amount_to_drop ; ++i)
            {
				int index_to_drop = rand() % ringbuffer->max_size;
				while(ringbuffer->array[index_to_drop].is_running == 1 || ringbuffer->array[index_to_drop].is_running == 2)
				{
					index_to_drop = rand() % ringbuffer->max_size;
				}
				Close(ringbuffer->array[index_to_drop].fd);
				ringbuffer->array[index_to_drop].is_running = 2;  // means request will be dropped
			}
			Request* new_array = (Request*) malloc(sizeof(Request) * ringbuffer->max_size);
			int j = 0; // new_array index
			for(int i = 0; i < ringbuffer->max_size; ++i)
			{
				if(ringbuffer->array[i].is_running != 2)
				{
					new_array[j] = ringbuffer->array[i];
					++j;
					ringbuffer->head = j;
				}
			}
			free(ringbuffer->array);
			ringbuffer->tail = 0;
			ringbuffer->array = new_array;
			ringbuffer->waiting -= amount_to_drop;
			return 1;
        default:
            //printf("unknown schedule algorithm how did you get here?");
            exit(1);
            break;
    }
}

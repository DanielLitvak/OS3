#ifndef OS3_RINGBUFFER_H
#define OS3_RINGBUFFER_H

#include "defenitions.h"

enum _schedalg{
    BLOCK,
    DT,
    DH,
    RANDOM
};

typedef struct{
    Request* array;
    int head;
    int tail;
    int max_size;
    int waiting;
    int in_progress;
    enum _schedalg alg;

    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} RingBuffer;

RingBuffer* create_ringbuffer(int max_size, enum _schedalg);

void add_request(RingBuffer* RB, int fd);

void remove_request(RingBuffer* RB);

void advance_head(RingBuffer* RB);

void advance_tail(RingBuffer* RB);

int is_full(RingBuffer* RB);

int no_waiting_requests(RingBuffer* RB);

int handle_overload(RingBuffer* ringbuffer, int fd);

#endif //OS3_RINGBUFFER_H

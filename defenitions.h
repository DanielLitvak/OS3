#ifndef OS3_DEFENITIONS_H
#define OS3_DEFENITIONS_H

#include <sys/time.h>
#include <pthread.h>

typedef struct{
    pthread_t thread;
    int id;
    struct timeval arrival;
    struct timeval dispatch;
    int count;
    int static_count;
    int dynamic_count;
} Thread;

typedef struct{
    int fd;
    struct timeval arrival;
    struct timeval dispatch;
    Thread* thread_info;
}Request;

#endif //OS3_DEFENITIONS_H

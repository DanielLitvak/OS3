#include "segel.h"
#include "ThreadPool.h"
#include "RingBuffer.h"

void getargs(int *port, int *_num_of_threads, int *queue_size, enum _schedalg *schedalg, int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *_num_of_threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    if (strcmp(argv[4], "block") == 0)
        *schedalg = BLOCK;
    else if (strcmp(argv[4], "dt") == 0)
        *schedalg = DT;
    else if (strcmp(argv[4], "dh") == 0)
        *schedalg = DH;
    else if (strcmp(argv[4], "random") == 0)
        *schedalg = RANDOM;
}


int main(int argc, char *argv[])
{
//    printf("server is running\n");
    int listenfd, num_of_threads, connfd, port, clientlen, queue_size;
    enum _schedalg alg;
    struct sockaddr_in clientaddr;

    getargs(&port, &num_of_threads, &queue_size, &alg, argc, argv);

    RingBuffer* ringbuffer = create_ringbuffer(queue_size, alg);
    ThreadPool* threadpool = create_threadpool(num_of_threads, ringbuffer);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        add_request(ringbuffer, connfd);
    }

}
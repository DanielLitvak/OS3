#include "segel.h"
#include "request.h"
#include "server.h"
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too

pthread_cond_t not_empty;
pthread_cond_t not_full;
pthread_mutex_t lock;
struct Thread* thread_array;
struct RingBuffer requests;
int num_of_threads;

enum _schedalg{
	BLOCK,
	DT,
	DH,
	RANDOM};
	
	
struct RingBuffer{
	struct Request* array;
	int consumer_idx, producer_idx;
	int size;
	int max_size;
	enum _schedalg alg;
};
	
void add_to_ringbuffer(struct RingBuffer* buffer, int val)
{
	//printf("add_to_ringbuffer: started\n");
	//printf("add_to_ringbuffer: at start, producer idx is %d\n", buffer->producer_idx);
	//printf("add_to_ringbuffer: at start, consumer idx is %d\n", buffer->consumer_idx);
	pthread_mutex_lock(&lock);
	struct timeval time;
	gettimeofday(&time, NULL);
	if (buffer->size == buffer->max_size)
	{
		if (buffer->alg == BLOCK)
		{
			while(buffer->max_size == buffer->size)
				pthread_cond_wait(&not_full, &lock);
			buffer->array[buffer->producer_idx].fd = val;
			buffer->array[buffer->producer_idx].arrival = time;
			buffer->size++;
			if(buffer->producer_idx == buffer->max_size)
				buffer->producer_idx = 0;  // signal sent at end of func
			
		}
		else if (buffer->alg == DT)
		{
			Close(val);
			pthread_cond_signal(&not_empty);
			pthread_mutex_unlock(&lock);
			return;
		}
		else if (buffer->alg == DH)
		{
			Close(buffer->array[buffer->consumer_idx].fd);
			buffer->consumer_idx++;
			buffer->array[buffer->producer_idx].fd = val;
			buffer->array[buffer->producer_idx].arrival = time;
			buffer->producer_idx++;
			if(buffer->producer_idx == buffer->max_size)
				buffer->producer_idx = 0;
			if(buffer->consumer_idx == buffer->max_size)
				buffer->consumer_idx = 0;  // signal sent at end of func
		}
		else if (buffer->alg == RANDOM)
		{
			pthread_cond_signal(&not_empty);
			pthread_mutex_unlock(&lock);
			return; // todo: implement random
		}
	}
	else
	{
		buffer->array[buffer->producer_idx].fd = val;
		buffer->array[buffer->producer_idx].arrival = time;
		buffer->producer_idx++;
		buffer->size++;
		if(buffer->producer_idx == buffer->max_size)
			buffer->producer_idx = 0;
	}
	pthread_cond_signal(&not_empty);  // end of func signal
	pthread_mutex_unlock(&lock);
	//printf("add_to_ringbuffer: at end, producer idx is %d\n", buffer->producer_idx);
	//printf("add_to_ringbuffer: at end, consumer idx is %d\n", buffer->consumer_idx);
	//printf("add to ringbuffer: ended\n");
}

void* do_request_handle(void* _requests)
{
	struct RingBuffer* requests = (struct RingBuffer*) _requests;
	while(1)
	{
		//printf("some thread loop\n");
		pthread_mutex_lock(&lock);
		while(requests->size == 0)
		{
			//printf("thread waiting on lock\n");
			pthread_cond_wait(&not_empty, &lock);
			//printf("thread waiting on lock after cond\n");
		}
			
		struct timeval time;
		gettimeofday(&time, NULL);
		
		requests->size--;
		struct Request request = requests->array[requests->consumer_idx];
		request.dispatch = time;
		
		pthread_t thread_handling_this_request = pthread_self();  // finds info about thread handling this request
		//printf("thread handling: %lu\n", thread_handling_this_request);
		for(int i = 0; i < num_of_threads; ++i)
			if(thread_array[i].thread == thread_handling_this_request)
			{
				request.thread_info = &thread_array[i];
				break;
			}
		
		requests->consumer_idx++;
		if(requests->consumer_idx == requests->max_size)
			requests->consumer_idx = 0;
		//printf("did manipulation on consumer idx\n");
		pthread_cond_signal(&not_full);
		pthread_mutex_unlock(&lock);
		//printf("started handling request, inner\n");
		requestHandle(request);
		//printf("finished handling request, inner\n");
		Close(request.fd);
		//printf("exitted do_request_handle\n");
	}
}

void getargs(int *port, int *num_of_threads, int *queue_size, enum _schedalg *schedalg, int argc, char *argv[])
{
    if (argc < 4) {
	fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
    *num_of_threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    if (strcmp(argv[4], "block") == 0)
		*schedalg = BLOCK;
	else if (strcmp(argv[4], "dt") == 0)
		*schedalg = DT;
	else if (strcmp(argv[4], "ht") == 0)
		*schedalg = DH;
	else if (strcmp(argv[4], "random") == 0)
		*schedalg = RANDOM;
}


int main(int argc, char *argv[])
{
    int listenfd, connfd, port, queue_size, clientlen;
    enum _schedalg schedalg;
    struct sockaddr_in clientaddr;
	pthread_mutex_init(&lock, NULL);
	thread_array = (struct Thread*) malloc(sizeof(struct Thread) * num_of_threads);
	
    getargs(&port, &num_of_threads, &queue_size, &schedalg, argc, argv);
	 
	// initializing the ring buffer
	requests.array = (struct Request*) malloc(sizeof(struct Request) * queue_size);
	requests.consumer_idx = 0;
	requests.producer_idx = 0;
	requests.size = 0;
	requests.max_size = queue_size;

	for (int i=0; i < num_of_threads; ++i)  // creating threads
	{
		pthread_create(&thread_array[i].thread, NULL,do_request_handle,&requests);
		//printf("thread pid as returned by create: %lu\n", thread_array[i].thread);
		thread_array[i].count = 0;
		thread_array[i].static_count = 0;
		thread_array[i].dynamic_count = 0;
	}
	
	for (int i=0; i < num_of_threads; ++i)  // debug info
	{
		printf("info on thread #%d: id = %lu, count = %d, static_count = %d, dynamic_count = %d\n", i, thread_array[i].thread, thread_array[i].count, thread_array[i].static_count, thread_array[i].dynamic_count);
	}
	
    listenfd = Open_listenfd(port);
    while (1) 
    {
		//printf("waiting on request\n");
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		//printf("request acquired: fd is %d\n", connfd);
		add_to_ringbuffer(&requests, connfd);
    }
}


    


 
#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
	struct thread_data *data = (struct thread_data *)thread_param;

	data->thread_complete_success = false;

	if (usleep((useconds_t)((unsigned int)data->wait_to_obtain_ms * 1000U)) != 0) {
		ERROR_LOG("usleep before mutex lock failed: %s", strerror(errno));
		return data;
	}

	if (pthread_mutex_lock(data->mutex) != 0) {
		ERROR_LOG("pthread_mutex_lock failed");
		return data;
	}

	if (usleep((useconds_t)((unsigned int)data->wait_to_release_ms * 1000U)) != 0) {
		ERROR_LOG("usleep after mutex lock failed: %s", strerror(errno));
		(void)pthread_mutex_unlock(data->mutex);
		return data;
	}

	if (pthread_mutex_unlock(data->mutex) != 0) {
		ERROR_LOG("pthread_mutex_unlock failed");
		return data;
	}

	data->thread_complete_success = true;
	return data;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	struct thread_data *data = malloc(sizeof(struct thread_data));

	if (data == NULL) {
		ERROR_LOG("malloc(thread_data) failed");
		return false;
	}

	data->mutex = mutex;
	data->wait_to_obtain_ms = wait_to_obtain_ms;
	data->wait_to_release_ms = wait_to_release_ms;
	data->thread_complete_success = false;

	if (pthread_create(thread, NULL, threadfunc, data) != 0) {
		ERROR_LOG("pthread_create failed");
		free(data);
		return false;
	}

	return true;
}

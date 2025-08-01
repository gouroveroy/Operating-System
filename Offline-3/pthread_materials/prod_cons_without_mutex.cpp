#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <unistd.h>

using namespace std;

// semaphore to control sleep and wake up
sem_t sem_empty;
sem_t sem_full;
queue<int> q;

void init_semaphore()
{
	sem_init(&sem_empty, 0, 5);
	sem_init(&sem_full, 0, 0);
}

void *ProducerFunc(void *arg)
{
	printf("%s\n", (char *)arg);
	int i;
	for (i = 1; i <= 10; i++)
	{
		sem_wait(&sem_empty);

		sleep(1);

		q.push(i);
		printf("producer produced item %d\n", i);

		sem_post(&sem_full);
	}
	return NULL;
}

void *ConsumerFunc(void *arg)
{
	printf("%s\n", (char *)arg);
	int i;
	for (i = 1; i <= 10; i++)
	{
		sem_wait(&sem_full);

		sleep(1);

		int item = q.front();
		q.pop();
		printf("consumer consumed item %d\n", item);

		sem_post(&sem_empty);
	}
	return NULL;
}

int main(void)
{
	pthread_t thread1;
	pthread_t thread2;

	init_semaphore();

	const char *message1 = "i am producer";
	const char *message2 = "i am consumer";

	pthread_create(&thread1, NULL, ProducerFunc, (void *)message1);
	pthread_create(&thread2, NULL, ConsumerFunc, (void *)message2);

	for (int i = 0; i < 2; i++)
	{
		pthread_join(i == 0 ? thread1 : thread2, NULL);
	}
	return 0;
}

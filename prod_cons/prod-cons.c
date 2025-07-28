#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 5
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 1

typedef struct
{
    int thread_id;
    int num_sales;
} producer_args;

double buffer;
int count = 0;
int in_idx = 0;
int out_idx = 0;

pthread_mutex_t mutex;

sem_t empty_slots;
sem_t full_slots;

pthread_cond_t buffer_full_cond;

int active_producers = NUM_PRODUCERS;

void *producer(void *args)
{
    int n = *((int *)args);
}

void *consumer(void *args)
{
    while (active_producers > 0 || count > 0)
    {
    }
}

int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    srand(time(NULL));

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&buffer_full_cond, NULL);

    sem_init(&empty_slots, 0, BUFFER_SIZE);
    sem_init(&full_slots, 0, 0);

    printf("--- Iniciando Simulação de Gerenciamento de Caixas ---\n");
    printf("Configuração: %d Produtores (Caixas), %d Consumidor (Gerente), Tamanho do Buffer: %d\n\n",
           NUM_PRODUCERS, NUM_CONSUMERS, BUFFER_SIZE);

    for (size_t i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_args *args = malloc(sizeof(producer_args));
        args->thread_id = i + 1;
        args->num_sales = 0;
        pthread_create(&producers[i], NULL, producer, (void *)args);
    }

    for (size_t i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_create(&consumers[i], NULL, consumer, NULL);
    }

    for (size_t i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }

    for (size_t i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&buffer_full_cond);
    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);

    printf("\n--- Simulação Concluída ---\n");

    return 0;
}
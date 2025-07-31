#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#define SIZE 2000000000
#define NUM_THREADS 8
#define PARTIAL_NUM_TERMS ((SIZE) / (NUM_THREADS))

long double result = 0;
pthread_mutex_t mutex;

double calcular_tempo()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1e9;
}

// se der pal dps tentar com void * e comversao de tipos pode erro de compilaçao
long double partialFormula(int start_term)
{

    const int num_terms = start_term + PARTIAL_NUM_TERMS;

    long double pi_approximation = 0;
    double signal = 1.0;

    for (int k = start_term; k < num_terms; k++)
    {
        pi_approximation += signal / (2 * k + 1);
        signal *= -1.0;
    }

    return pi_approximation;
}

void *partialProcessing(void *args)
{
    pthread_t tid = pthread_self();
    int first_therm = *((int *)args);
    free(args);

    double initial_time = calcular_tempo();             // comeca a contar o tempo de inicio
    long double sum = partialFormula((int)first_therm); // faz o calculo dos valores referentes a essa thread
    double end_time = calcular_tempo();                 // comeca a contar o tempo de fim
    double final_time = end_time - initial_time;        // calcula p tempo final

    pthread_mutex_lock(&mutex);
    result += 4 * sum;
    pthread_mutex_unlock(&mutex);

    printf("TID: %lu : %.2fs\n", (unsigned long)tid, final_time); // mostrar TID e tempo empregado na thread

    return NULL;
}

int main()
{

    // criar um mutex
    pthread_mutex_init(&mutex, NULL);
    // criar as threads
    pthread_t thread[NUM_THREADS];
    unsigned long args[NUM_THREADS];

    // dividir o tamanho total pelo número de threads
    long long remainder = SIZE % NUM_THREADS;

    // começamos a calcular o tempo de inicio do procesamento
    printf("Começando a calcular o valor de pi da série de Leibniz, com %d threads\n", NUM_THREADS);
    double total_start_time = calcular_tempo();

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        int *init = malloc(sizeof(int));
        *init = i * PARTIAL_NUM_TERMS;

        int terms_to_compute = PARTIAL_NUM_TERMS;

        // A última thread pega os termos restantes
        if (i == NUM_THREADS - 1)
        {
            terms_to_compute += SIZE % NUM_THREADS;
        }

        pthread_create(&thread[i], NULL, partialProcessing, (void *)init);
    }

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        pthread_join(thread[i], NULL);
    }

    double total_time_end = calcular_tempo();
    double total_final_time = total_time_end - total_start_time;

    /* Liberar o mutex */
    pthread_mutex_destroy(&mutex);

    printf("\nValor aproximado de pi: %.15Lf\n", result);
    printf("Tempo total de execução: %.2fs\n", total_final_time);

    return EXIT_SUCCESS;
}
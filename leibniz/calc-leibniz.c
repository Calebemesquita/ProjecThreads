#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

#define NUM_THREADS 2
#define SIZE 10000000

int array[SIZE];

unsigned long result = 0;

pthread_mutex_t mutex;


void * calcular(void *args){
    unsigned long first = * ((unsigned long*) args);

    unsigned long acc = 0;
    for(unsigned long i = first; i < SIZE; i += NUM_THREADS) {
        acc += array[i];
    }

    pthread_mutex_lock(&mutex);
    result += acc;
    pthread_mutex_unlock(&mutex);
}




int main (){


    for(int i = 0; i < SIZE; i++){
        array[i] = i;
    }
    
    // criar um mutex
    pthread_mutex_init(&mutex, NULL);

    // criar as threads
    pthread_t thread[NUM_THREADS];
    unsigned long args[NUM_THREADS];

    struct timeval inicio;
    gettimeofday(&inicio, NULL);

    for(int i = 0; i < NUM_THREADS; ++i) {
        
        args[i] = i * (SIZE / NUM_THREADS);

        pthread_create(&thread[i], NULL, &calcular, (void *) &args[i] );
    }


    for(int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thread[i], NULL);
    }

    struct timeval final;
    gettimeofday(&final, NULL);

    /* Liberar o mutex */
    pthread_mutex_destroy(&mutex);

    double time = (final.tv_sec - inicio.tv_sec) + (final.tv_usec - inicio.tv_usec)/1000000.0;

    printf("Resultado = %u | Tempo %lf segundos\n", result, time);

    return EXIT_SUCCESS;




}
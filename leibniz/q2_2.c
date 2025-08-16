/**
 * @file q2_2.c
 * @brief Calcula uma aproximação de Pi usando a fórmula de Leibniz com paralelismo de threads.
 *
 * Este programa divide o cálculo da série de Leibniz em várias threads para
 * acelerar o processamento. Cada thread calcula uma parte da série, e os
 * resultados parciais são somados de forma segura usando um mutex para produzir
 * a aproximação final de Pi. O tempo de execução de cada thread e o tempo total
 * são medidos e exibidos.
 */

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

/**
 * @def SIZE
 * @brief O número total de termos a serem calculados na série de Leibniz.
 */
#define SIZE 2000000000

#define NUM_THREADS 8

#define PARTIAL_NUM_TERMS ((SIZE) / (NUM_THREADS))

/**
 * @var result
 * @brief Variável global para armazenar a soma final de todas as aproximações parciais de Pi.
 *        O acesso a esta variável é protegido por um mutex para evitar condições de corrida.
 */
long double result = 0;

/**
 * @var mutex
 * @brief Mutex para garantir o acesso exclusivo à variável global `result` pelas threads.
 */
pthread_mutex_t mutex;

/**
 * @fn double calcular_tempo()
 * @brief Calcula o tempo atual do sistema com alta precisão.
 * @return O tempo atual em segundos, como um valor double. Utiliza CLOCK_MONOTONIC para
 *         medições de tempo que não são afetadas por mudanças no relógio do sistema.
 */
double calcular_tempo()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1e9;
}

/**
 * @fn long double partialFormula(int start_term)
 * @brief Calcula uma soma parcial da série de Leibniz.
 *
 * A fórmula de Leibniz para Pi é: π/4 = 1 - 1/3 + 1/5 - 1/7 + ...
 * Esta função calcula um segmento desta série, começando em `start_term` e
 * continuando por `PARTIAL_NUM_TERMS` iterações.
 *
 * @param start_term O índice inicial do termo na série a partir do qual o cálculo deve começar.
 * @return A soma parcial dos termos calculados como um `long double`.
 */
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

/**
 * @fn void *partialProcessing(void *args)
 * @brief A função de trabalho executada por cada thread.
 *
 * Esta função recebe o termo inicial para seu cálculo, chama `partialFormula` para
 * obter a soma parcial, mede o tempo de execução dessa tarefa, e então adiciona
 * seu resultado à variável global `result` de forma segura, usando um mutex.
 *
 * @param args Um ponteiro para um inteiro alocado dinamicamente que contém o termo inicial
 *             para o cálculo desta thread. A memória para `args` é liberada dentro da função.
 * @return NULL.
 */
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

/**
 * @fn int main()
 * @brief Ponto de entrada principal do programa.
 *
 * A função `main` inicializa o mutex, cria e gerencia as threads, mede o tempo
 * total de execução, e exibe o resultado final.
 * - Inicializa o mutex.
 * - Inicia a contagem do tempo total.
 * - Cria `NUM_THREADS` threads, passando a cada uma o seu termo inicial para o cálculo.
 *   A última thread é designada para calcular quaisquer termos remanescentes da divisão inteira.
 * - Aguarda a conclusão de todas as threads com `pthread_join`.
 * - Para a contagem do tempo total.
 * - Destrói o mutex.
 * - Imprime o valor aproximado de Pi e o tempo total de execução.
 *
 * @return EXIT_SUCCESS em caso de sucesso.
 */
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
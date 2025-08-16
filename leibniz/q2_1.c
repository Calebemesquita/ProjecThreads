#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#define SIZE 2000000000

/**
 * essa deve ser a versao sequencial do leibniz
 */

long double result = 0;

double calcular_tempo()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + (double)time.tv_nsec / 1e9;
}

long double calculationFormula(int start_therm)
{
    long double pi_approximation = 0;
    double signal = 1.0;

    for (int k = start_therm; k < SIZE; k++)
    {
        pi_approximation += signal / (2 * k + 1);
        signal *= -1.0;
    }

    return 4 * pi_approximation;
}

int main()
{

    double total_start_time = calcular_tempo();
    result = calculationFormula(0);
    double total_time_end = calcular_tempo();
    double total_final_time = total_time_end - total_start_time;

    printf("\nValor aproximado de pi: %.15Lf\n", result);
    printf("Tempo total de execução: %.2fs\n", total_final_time);

    return EXIT_SUCCESS;
}
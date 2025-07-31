/**
 * @file q1_2.c
 * @brief Simulação do problema Produtor-Consumidor com múltiplos produtores e consumidores.
 *
 * Este programa implementa uma solução para o problema clássico do Produtor-Consumidor
 * utilizando múltiplas threads para produtores (caixas de uma loja) e consumidores (gerentes).
 * A comunicação entre eles é feita através de um buffer circular compartilhado.
 *
 * A sincronização é gerenciada pelos seguintes primitivos:
 * - Mutex (`mutex`): Garante acesso exclusivo ao buffer compartilhado e às variáveis
 *   de controle (`count`, `in_idx`, `out_idx`, `active_producers`), prevenindo condições de corrida.
 * - Semáforo (`empty_slots`): Controla o número de posições vazias no buffer. Produtores
 *   esperam neste semáforo se o buffer estiver cheio.
 * - Semáforo (`full_slots`): Controla o número de itens disponíveis no buffer. Consumidores
 *   esperam neste semáforo se o buffer estiver vazio.
 *
 * A lógica de término é coordenada pela variável `active_producers`. Cada produtor, ao
 * concluir seu trabalho, decrementa este contador. O último produtor a terminar notifica
 * todas as threads consumidoras (via `sem_post`) para que elas possam verificar a condição
 * de término (não há produtores ativos e o buffer está vazio) e encerrar sua execução.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

/**
 * @def BUFFER_SIZE
 * @brief Define a capacidade máxima do buffer compartilhado.
 */
#define BUFFER_SIZE 5

/**
 * @def NUM_PRODUCERS
 * @brief Define o número de threads produtoras (caixas) a serem criadas.
 */
#define NUM_PRODUCERS 6

/**
 * @def NUM_CONSUMERS
 * @brief Define o número de threads consumidoras (gerentes) a serem criadas.
 */
#define NUM_CONSUMERS 2

/**
 * @struct producer_args
 * @brief Estrutura para encapsular os argumentos a serem passados para cada thread produtora.
 */
typedef struct
{
    int thread_id;
    int num_sales;
} producer_args;

/**
 * @struct consumer_args
 * @brief Estrutura para encapsular os argumentos a serem passados para cada thread consumidora.
 */
typedef struct
{
    int thread_id;
} consumer_args;

double buffer[BUFFER_SIZE];
int count = 0;
int in_idx = 0;
int out_idx = 0;

pthread_mutex_t mutex;
sem_t empty_slots;
sem_t full_slots;
pthread_cond_t buffer_empty_cond; // Usada para garantir o término correto

// Volatile para garantir que a leitura mais recente seja usada por todas as threads
volatile int active_producers = NUM_PRODUCERS;

/**
 * @fn void *producer(void *args)
 * @brief Função executada pelas threads produtoras.
 *
 * Cada produtor gera um número pré-definido de vendas (itens). Para cada venda,
 * ele aguarda um espaço livre no buffer (`sem_wait(&empty_slots)`), adquire o
 * bloqueio do mutex, insere o item, atualiza os contadores e libera o mutex.
 * Por fim, sinaliza que um novo item está disponível para consumo (`sem_post(&full_slots)`).
 * Ao final de sua produção, decrementa o contador `active_producers` e, se for o
 * último produtor, acorda as threads consumidoras para que possam encerrar.
 *
 * @param args Ponteiro para uma estrutura `producer_args` contendo o ID da thread e o número de vendas a produzir.
 * @return NULL.
 */
void *producer(void *args)
{
    producer_args *p_args = (producer_args *)args;
    int tid = p_args->thread_id;
    int sales_to_produce = p_args->num_sales;

    for (size_t i = 0; i < sales_to_produce; i++)
    {
        double sale_value = (rand() % 100000) / 100.0 + 1.0;

        sem_wait(&empty_slots); // Espera por um slot vazio

        pthread_mutex_lock(&mutex);
        buffer[in_idx] = sale_value;
        in_idx = (in_idx + 1) % BUFFER_SIZE;
        count++;
        printf("(P) TID %d | VENDA: R$ %.2f | Buffer: %d/%d\n",
               tid, sale_value, count, BUFFER_SIZE);
        pthread_mutex_unlock(&mutex);

        sem_post(&full_slots); // Sinaliza que um slot foi preenchido

        sleep((rand() % 3) + 1); // Pausa menor para aumentar a concorrência
    }

    // No final do producer
    pthread_mutex_lock(&mutex);
    active_producers--;
    printf(">>>> (P) Caixa %d finalizou. Produtores ativos: %d <<<<\n", tid, active_producers);
    if (active_producers == 0)
    {
        // Acorda TODOS os consumidores que possam estar esperando no sem_wait.
        // Eles irão acordar, verificar a condição de término e sair.
        for (int i = 0; i < NUM_CONSUMERS; i++)
        {
            sem_post(&full_slots);
        }
    }
    pthread_mutex_unlock(&mutex);

    free(p_args);
    pthread_exit(NULL);
}

/**
 * @fn void *consumer(void *args)
 * @brief Função executada pelas threads consumidoras.
 *
 * Cada consumidor opera em um loop infinito, tentando processar vendas. Ele aguarda
 * até que um item esteja disponível no buffer (`sem_wait(&full_slots)`). Após ser
 * acordado, ele verifica a condição de término: se não há mais produtores ativos e
 * o buffer está vazio. Se a condição for verdadeira, ele encerra. Caso contrário,
 * ele adquire o bloqueio do mutex, consome um item do buffer, atualiza os contadores,
 * libera o mutex e sinaliza que um espaço no buffer foi liberado (`sem_post(&empty_slots)`).
 *
 * @param args Ponteiro para uma estrutura `consumer_args` contendo o ID da thread.
 * @return NULL.
 */
void *consumer(void *args)
{
    consumer_args *c_args = (consumer_args *)args;
    int tid = c_args->thread_id;
    int sales_processed = 0;

    while (1)
    {
        // Espera por um item. Este é o ponto de bloqueio.
        sem_wait(&full_slots);

        // Após acordar, a primeira coisa é verificar se devemos terminar.
        // Bloqueamos o mutex para ler 'count' e 'active_producers' de forma segura.
        pthread_mutex_lock(&mutex);
        if (active_producers == 0 && count == 0)
        {
            // Não há mais produtores e o buffer está vazio. O trabalho acabou.
            // Precisamos liberar o mutex antes de sair.
            pthread_mutex_unlock(&mutex);

            // Como consumimos um 'sem_wait' para entrar aqui,
            // mas não vamos consumir um item, precisamos devolver o "ticket"
            // para que outra thread consumidora também possa sair.
            sem_post(&full_slots);

            break;
        }

        // Se chegamos aqui, há um item para consumir.
        double sale_value = buffer[out_idx];
        out_idx = (out_idx + 1) % BUFFER_SIZE;
        count--;
        sales_processed++;

        printf("    (C) TID %d | PROCESSOU: R$ %.2f | Buffer: %d/%d\n",
               tid, sale_value, count, BUFFER_SIZE);

        pthread_mutex_unlock(&mutex);

        // Libera um slot vazio para os produtores.
        sem_post(&empty_slots);
    }

    printf(">>>> (C) Gerente %d finalizou. Total de vendas processadas: %d <<<<\n", tid, sales_processed);
    free(c_args);
    pthread_exit(NULL);
}

/**
 * @fn int main()
 * @brief Ponto de entrada principal do programa.
 *
 * Inicializa os primitivos de sincronização (mutex e semáforos), cria as threads
 * produtoras e consumidoras, e aguarda a conclusão de todas elas usando `pthread_join`.
 * Após o término das threads, destrói os primitivos de sincronização e finaliza o programa.
 *
 * @return 0 em caso de sucesso, ou um código de erro em caso de falha.
 */
int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    srand(time(NULL));

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&buffer_empty_cond, NULL);

    // Inicializa semáforos
    sem_init(&empty_slots, 0, BUFFER_SIZE); // Começa com N slots vazios
    sem_init(&full_slots, 0, 0);            // Começa com 0 slots preenchidos

    printf("--- Iniciando Simulação com %d Produtores e %d Consumidores ---\n\n",
           NUM_PRODUCERS, NUM_CONSUMERS);

    // Cria as threads produtoras
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_args *args = malloc(sizeof(producer_args));
        args->thread_id = i + 1;
        args->num_sales = (rand() % 6) + 5; // Menos vendas para a simulação ser mais rápida
        pthread_create(&producers[i], NULL, producer, (void *)args);
    }

    // Cria as threads consumidoras
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        consumer_args *args = malloc(sizeof(consumer_args));
        args->thread_id = i + 1;
        pthread_create(&consumers[i], NULL, consumer, (void *)args);
    }

    // Espera todas as threads terminarem
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }

    // Após os produtores terminarem, precisamos garantir que os consumidores acordem
    // caso estejam esperando em sem_wait.
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        sem_post(&full_slots);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    // Destrói os primitivos de sincronização
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&buffer_empty_cond);
    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);

    printf("\n--- Simulação Concluída ---\n");

    return 0;
}

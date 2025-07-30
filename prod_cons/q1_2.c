#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

/**
 * @file prod-cons-multi.c
 * @brief Simulação do problema Produtor-Consumidor com múltiplos produtores e múltiplos consumidores.
 *
 * Esta versão adapta a solução para o cenário com 6 produtores (caixas) e 2 consumidores (gerentes).
 * A principal mudança está na lógica do consumidor. Em vez de um único gerente esperar o buffer
 * encher para processar um lote, agora múltiplos gerentes competem para processar vendas
 * individuais assim que elas se tornam disponíveis.
 *
 * A sincronização foi ajustada para o padrão clássico:
 * - **Mutex (`mutex`):** Continua protegendo o acesso ao buffer e às variáveis compartilhadas.
 * - **Semáforos (`empty_slots`, `full_slots`):** Agora, `full_slots` é o principal mecanismo
 * de sincronização para os consumidores. Um consumidor espera em `sem_wait(&full_slots)`
 * até que haja pelo menos um item para ser consumido. Isso permite o consumo paralelo.
 * - **Variável de Condição (`buffer_full_cond`):** Foi removida da lógica principal do consumidor,
 * pois eles não esperam mais o buffer encher. É mantida para o broadcast final, garantindo
 * que todas as threads acordem e terminem corretamente quando a produção cessar.
 */

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
 * @brief Função da thread produtora. Lógica praticamente inalterada.
 *
 * Produz um item, espera por um slot vazio, bloqueia o mutex, insere no buffer,
 * desbloqueia o mutex e sinaliza que um slot foi preenchido.
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

    pthread_mutex_lock(&mutex);
    active_producers--;
    printf(">>>> (P) Caixa %d finalizou. Produtores ativos: %d <<<<\n", tid, active_producers);
    // Se for o último produtor, acorda todos os consumidores para que possam terminar.
    if (active_producers == 0)
    {
        // Usamos broadcast para garantir que TODOS os consumidores que possam estar esperando
        // sejam acordados para verificar a condição de término.
        sem_post(&full_slots); // Post extra para caso um consumidor esteja preso no semáforo
        sem_post(&full_slots); // Um para cada consumidor
        pthread_cond_broadcast(&buffer_empty_cond);
    }
    pthread_mutex_unlock(&mutex);

    free(p_args);
    pthread_exit(NULL);
}

/**
 * @fn void *consumer(void *args)
 * @brief Função da thread consumidora, redesenhada para múltiplos consumidores.
 *
 * Em vez de esperar o buffer encher, cada consumidor agora espera por um único item
 * disponível usando `sem_wait(&full_slots)`. Isso permite que múltiplos consumidores
 * processem itens em paralelo. A lógica de "calcular média de 5" foi removida,
 * pois não é compatível com este modelo de consumo concorrente.
 */
void *consumer(void *args)
{
    consumer_args *c_args = (consumer_args *)args;
    int tid = c_args->thread_id;
    int sales_processed = 0;

    // Loop principal: continua enquanto houver produtores ativos OU itens no buffer.
    while (active_producers > 0 || count > 0)
    {
        // Espera até que haja pelo menos um item no buffer.
        // Este é o principal ponto de bloqueio e sincronização para o consumidor.
        sem_wait(&full_slots);

        // Se foi acordado, mas não há mais nada a fazer, sai.
        // Isso acontece no final, quando os produtores terminam.
        if (active_producers == 0 && count == 0) {
            break;
        }

        pthread_mutex_lock(&mutex);
        
        // Checagem dupla para evitar consumir de um buffer vazio no final da execução
        if(count > 0) {
            double sale_value = buffer[out_idx];
            out_idx = (out_idx + 1) % BUFFER_SIZE;
            count--;
            sales_processed++;

            printf("    (C) TID %d | PROCESSOU: R$ %.2f | Buffer: %d/%d\n",
                   tid, sale_value, count, BUFFER_SIZE);
        }

        pthread_mutex_unlock(&mutex);

        sem_post(&empty_slots); // Sinaliza que um slot ficou vazio
    }

    printf(">>>> (C) Gerente %d finalizou. Total de vendas processadas: %d <<<<\n", tid, sales_processed);
    free(c_args);
    pthread_exit(NULL);
}

int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    srand(time(NULL));

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&buffer_empty_cond, NULL);

    // Inicializa semáforos
    sem_init(&empty_slots, 0, BUFFER_SIZE); // Começa com N slots vazios
    sem_init(&full_slots, 0, 0);          // Começa com 0 slots preenchidos

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
    for (int i = 0; i < NUM_CONSUMERS; i++) {
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

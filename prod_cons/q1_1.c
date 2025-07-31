/**
 * @file q1_1.c
 * @brief Simulação do problema Produtor-Consumidor usando pthreads, semáforos e variáveis de condição.
 *
 * Este programa implementa uma solução para o problema clássico do produtor-consumidor.
 * Ele simula um cenário com múltiplos "caixas" (produtores) que geram vendas (valores de ponto flutuante)
 * e as colocam em um buffer circular compartilhado. Um único "gerente" (consumidor) aguarda
 * até que o buffer esteja completamente cheio para então processar todas as vendas de uma vez,
 * calculando o valor médio.
 *
 * A sincronização entre as threads é gerenciada da seguinte forma:
 * - **Mutex (`mutex`):** Garante o acesso exclusivo às seções críticas, protegendo o buffer
 *   e as variáveis compartilhadas (`count`, `in_idx`, `out_idx`, `active_producers`) contra condições de corrida.
 * - **Semáforos (`empty_slots`, `full_slots`):** `empty_slots` controla o número de posições vazias no buffer,
 *   fazendo com que os produtores esperem se o buffer estiver cheio. `full_slots` foi mantido para ilustrar
 *   a solução clássica, embora o consumidor neste exemplo específico não espere por um único item.
 * - **Variável de Condição (`buffer_full_cond`):** Permite que o consumidor (gerente) espere de forma eficiente
 *   sem consumir CPU (`pthread_cond_wait`) até que o buffer esteja cheio ou que todos os produtores
 *   tenham terminado seu trabalho. Os produtores sinalizam (`pthread_cond_signal`) quando o buffer enche,
 *   e um `broadcast` é usado no final para garantir que o consumidor acorde e termine.
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
#define NUM_PRODUCERS 3

/**
 * @def NUM_CONSUMERS
 * @brief Define o número de threads consumidoras (gerentes) a serem criadas.
 */
#define NUM_CONSUMERS 1

/**
 * @struct producer_args
 * @brief Estrutura para encapsular os argumentos a serem passados para cada thread produtora.
 *
 * @var producer_args::thread_id
 * Um identificador único para a thread do caixa.
 * @var producer_args::num_sales
 * O número total de vendas que esta thread de caixa deve produzir.
 */
typedef struct
{
    int thread_id;
    int num_sales;
} producer_args;

/**
 * @var buffer
 * @brief Array de doubles que funciona como o buffer circular compartilhado para armazenar os valores das vendas.
 */
double buffer[BUFFER_SIZE];

/**
 * @var count
 * @brief Contador que armazena o número atual de itens no buffer.
 */
int count = 0;

/**
 * @var in_idx
 * @brief Índice onde o próximo produtor irá inserir um item no buffer.
 */
int in_idx = 0;

/**
 * @var out_idx
 * @brief Índice de onde o próximo consumidor irá remover um item do buffer.
 */
int out_idx = 0;

/**
 * @var mutex
 * @brief Mutex para garantir o acesso atômico às variáveis compartilhadas e ao buffer.
 */
pthread_mutex_t mutex;

/**
 * @var empty_slots
 * @brief Semáforo que conta o número de posições vazias no buffer. Inicializado com BUFFER_SIZE.
 */
sem_t empty_slots;

/**
 * @var full_slots
 * @brief Semáforo que conta o número de posições preenchidas no buffer. Inicializado com 0.
 */
sem_t full_slots;

/**
 * @var buffer_full_cond
 * @brief Variável de condição usada para sinalizar ao consumidor que o buffer está cheio.
 */
pthread_cond_t buffer_full_cond;

/**
 * @var active_producers
 * @brief Contador para rastrear o número de threads produtoras que ainda estão em execução.
 */
int active_producers = NUM_PRODUCERS;

/**
 * @fn void *producer(void *args)
 * @brief Função executada pelas threads produtoras (caixas).
 *
 * Cada produtor gera um número pré-definido de vendas com valores aleatórios.
 * Para cada venda, ele aguarda por um slot vazio no buffer (`sem_wait`), bloqueia o mutex,
 * adiciona o valor da venda ao buffer, atualiza os contadores e o índice de entrada.
 * Se o buffer ficar cheio após a inserção, ele sinaliza a variável de condição `buffer_full_cond`
 * para acordar o gerente. Após produzir todas as suas vendas, decrementa o contador `active_producers`
 * e, se for o último produtor a terminar, envia um `broadcast` na variável de condição para garantir
 * que o consumidor processe os itens restantes e termine.
 *
 * @param args Um ponteiro para uma estrutura `producer_args` contendo o ID da thread e o número de vendas a produzir.
 * @return NULL.
 */
void *producer(void *args)
{
    producer_args *p_args = (producer_args *)args;
    int tid = p_args->thread_id;
    int sales_to_produce = p_args->num_sales;

    for (size_t i = 0; i < sales_to_produce; i++)
    {
        double sale_value = (rand() % 100000) / 100.0 + 1.0; // Gera um valor de venda aleatório entre 1.00 e 1000.00

        sem_wait(&empty_slots);

        pthread_mutex_lock(&mutex);

        buffer[in_idx] = sale_value;
        in_idx = (in_idx + 1) % BUFFER_SIZE;
        count++;

        printf("(P) TID %ld | Caixa %d | VENDA: R$ %.2f | ITERAÇÃO: %d/%d | Buffer: %d/%d\n",
               pthread_self(), tid, sale_value, i + 1, sales_to_produce, count, BUFFER_SIZE);

        if (count == BUFFER_SIZE)
        {
            printf("--- BUFFER CHEIO! Notificando o gerente. ---\n");
            pthread_cond_signal(&buffer_full_cond);
        }

        pthread_mutex_unlock(&mutex);

        sem_post(&full_slots);

        sleep((rand() % 5) + 1);
    }

    pthread_mutex_lock(&mutex);
    active_producers--;

    printf("(P) TID %ld | Caixa %d finalizou sua produção. Produtores ativos: %d\n",
           pthread_self(), tid, active_producers);

    if (active_producers == 0)
    {
        pthread_cond_broadcast(&buffer_full_cond);
    }
    pthread_mutex_unlock(&mutex);

    free(p_args);
    pthread_exit(NULL);
}

/**
 * @fn void *consumer(void *args)
 * @brief Função executada pela thread consumidora (gerente).
 *
 * O consumidor entra em um loop infinito para processar as vendas. Ele bloqueia o mutex e aguarda
 * na variável de condição (`pthread_cond_wait`) até que o buffer esteja cheio (`count == BUFFER_SIZE`)
 * ou não haja mais produtores ativos (`active_producers == 0`).
 * Quando acordado e a condição é satisfeita, ele processa *todos* os itens presentes no buffer,
 * calculando a soma e a média. Em seguida, ele zera o contador de itens e libera os slots
 * correspondentes no semáforo `empty_slots`.
 * O loop termina quando não há mais produtores ativos e o buffer está vazio.
 *
 * @param args Não utilizado (NULL).
 * @return NULL.
 */
void *consumer(void *args)
{
    int iteration = 1;

    while (1)
    {
        pthread_mutex_lock(&mutex);

        while (count < BUFFER_SIZE && active_producers > 0)
        {
            printf("(C) TID %ld | Gerente esperando o buffer encher (Atual: %d/%d)...\n",
                   pthread_self(), count, BUFFER_SIZE);
            pthread_cond_wait(&buffer_full_cond, &mutex);
        }

        if (active_producers == 0 && count == 0)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (count > 0)
        {
            printf("(C) TID %ld | Gerente iniciando processamento de %d vendas. ITERAÇÃO: %d\n",
                   pthread_self(), count, iteration);

            double total_sum = 0.0;
            int items_consumed = count;

            for (int i = 0; i < items_consumed; i++)
            {
                double sale_value = buffer[out_idx];
                total_sum += sale_value;
                out_idx = (out_idx + 1) % BUFFER_SIZE;
            }
            count = 0;

            double average = total_sum / items_consumed;
            printf("(C) TID %ld | MÉDIA das %d vendas: R$ %.2f | ITERAÇÃO: %d\n",
                   pthread_self(), items_consumed, average, iteration++);

            pthread_mutex_unlock(&mutex);

            for (int i = 0; i < items_consumed; i++)
            {
                sem_post(&empty_slots);
            }
        }
        else
        {
            pthread_mutex_unlock(&mutex);
        }
    }

    printf("(C) TID %ld | Gerente finalizou. Não há mais produtores nem vendas a processar.\n", pthread_self());
    pthread_exit(NULL);
}

/**
 * @fn int main()
 * @brief Ponto de entrada principal do programa.
 *
 * Inicializa o gerador de números aleatórios, o mutex, a variável de condição e os semáforos.
 * Cria o número especificado de threads produtoras e consumidoras, passando os argumentos necessários.
 * Aguarda a conclusão de todas as threads produtoras e consumidoras usando `pthread_join`.
 * Por fim, destrói os primitivos de sincronização (mutex, cond, semáforos) e exibe uma mensagem de conclusão.
 *
 * @return 0 em caso de sucesso.
 */
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
        args->num_sales = (rand() % 11) + 20; // Cada produtor fará entre 20 e 30 vendas
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
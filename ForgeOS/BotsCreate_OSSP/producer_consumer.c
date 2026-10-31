
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define DATA_SIZE 2000000
#define CHUNK_SIZE 10000
#define BUFFER_SIZE 16
#define DEFAULT_WORKERS 16

static long *g_data = NULL;

typedef struct {
    long start;
    long end;
} WorkItem;

typedef struct {
    WorkItem items[BUFFER_SIZE];
    int count;
    int in;
    int out;
    int producer_done;

    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

} BoundedBuffer;

static BoundedBuffer g_buffer;
static long long g_result = 0;

static pthread_mutex_t g_result_mutex =
    PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t g_print_mutex =
    PTHREAD_MUTEX_INITIALIZER;


// Calculate execution time
static double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec + ts.tv_nsec / 1e9;
}


// Initialize bounded buffer
static void buffer_init(BoundedBuffer *b)
{
    b->count = 0;
    b->in = 0;
    b->out = 0;
    b->producer_done = 0;

    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->not_full, NULL);
    pthread_cond_init(&b->not_empty, NULL);
}


// Destroy bounded buffer
static void buffer_destroy(BoundedBuffer *b)
{
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->not_full);
    pthread_cond_destroy(&b->not_empty);
}


// Producer function
static void *producer_fn(void *arg)
{
    (void)arg;

    long produced = 0;

    for (long start = 0;
         start < DATA_SIZE;
         start += CHUNK_SIZE)
    {
        long end = start + CHUNK_SIZE;

        if (end > DATA_SIZE)
            end = DATA_SIZE;

        WorkItem item = {start, end};

        pthread_mutex_lock(&g_buffer.mutex);

        while (g_buffer.count == BUFFER_SIZE)
        {
            pthread_cond_wait(
                &g_buffer.not_full,
                &g_buffer.mutex
            );
        }

        g_buffer.items[g_buffer.in] = item;

        g_buffer.in =
            (g_buffer.in + 1) % BUFFER_SIZE;

        g_buffer.count++;
        produced++;

        pthread_cond_signal(&g_buffer.not_empty);

        pthread_mutex_unlock(&g_buffer.mutex);
    }

    pthread_mutex_lock(&g_buffer.mutex);

    g_buffer.producer_done = 1;

    pthread_cond_broadcast(&g_buffer.not_empty);

    pthread_mutex_unlock(&g_buffer.mutex);

    pthread_mutex_lock(&g_print_mutex);

    printf(
        "\n[Producer] Finished. Total work items: %ld\n",
        produced
    );

    pthread_mutex_unlock(&g_print_mutex);

    return NULL;
}


// Worker arguments
typedef struct {
    int worker_id;
    long chunks_processed;
} WorkerArg;


// Worker function
static void *worker_fn(void *arg)
{
    WorkerArg *w = (WorkerArg *)arg;

    for (;;)
    {
        WorkItem item;

        pthread_mutex_lock(&g_buffer.mutex);

        while (
            g_buffer.count == 0 &&
            !g_buffer.producer_done
        )
        {
            pthread_cond_wait(
                &g_buffer.not_empty,
                &g_buffer.mutex
            );
        }

        if (
            g_buffer.count == 0 &&
            g_buffer.producer_done
        )
        {
            pthread_mutex_unlock(&g_buffer.mutex);
            break;
        }

        item = g_buffer.items[g_buffer.out];

        g_buffer.out =
            (g_buffer.out + 1) % BUFFER_SIZE;

        g_buffer.count--;

        pthread_cond_signal(&g_buffer.not_full);

        pthread_mutex_unlock(&g_buffer.mutex);

        long long partial = 0;

        for (
            long i = item.start;
            i < item.end;
            i++
        )
        {
            partial += g_data[i] % 97;
        }

        pthread_mutex_lock(&g_result_mutex);

        g_result += partial;

        pthread_mutex_unlock(&g_result_mutex);

        w->chunks_processed++;

        pthread_mutex_lock(&g_print_mutex);

        printf(
            "[Worker %d | tid=%lu] "
            "Processed [%ld - %ld) | Result = %lld\n",
            w->worker_id,
            (unsigned long)pthread_self(),
            item.start,
            item.end,
            partial
        );

        pthread_mutex_unlock(&g_print_mutex);
    }

    pthread_mutex_lock(&g_print_mutex);

    printf(
        "[Worker %d] Exiting. Chunks processed: %ld\n",
        w->worker_id,
        w->chunks_processed
    );

    pthread_mutex_unlock(&g_print_mutex);

    return NULL;
}


// Sequential execution
static long long run_sequential(void)
{
    long long total = 0;

    for (long i = 0; i < DATA_SIZE; i++)
    {
        total += g_data[i] % 97;
    }

    return total;
}


// Multithreaded execution
static long long run_multithreaded(int num_workers)
{
    g_result = 0;

    buffer_init(&g_buffer);

    pthread_t producer_tid;

    pthread_t *worker_tids =
        malloc(sizeof(pthread_t) * num_workers);

    WorkerArg *worker_args =
        malloc(sizeof(WorkerArg) * num_workers);

    if (worker_tids == NULL || worker_args == NULL)
    {
        printf("Thread memory allocation failed!\n");
        free(worker_tids);
        free(worker_args);
        buffer_destroy(&g_buffer);
        exit(1);
    }

    pthread_create(
        &producer_tid,
        NULL,
        producer_fn,
        NULL
    );

    for (int i = 0; i < num_workers; i++)
    {
        worker_args[i].worker_id = i;
        worker_args[i].chunks_processed = 0;

        pthread_create(
            &worker_tids[i],
            NULL,
            worker_fn,
            &worker_args[i]
        );
    }

    pthread_join(producer_tid, NULL);

    for (int i = 0; i < num_workers; i++)
    {
        pthread_join(worker_tids[i], NULL);
    }

    buffer_destroy(&g_buffer);

    free(worker_tids);
    free(worker_args);

    return g_result;
}


// Main function
int main(int argc, char *argv[])
{
    int num_workers = DEFAULT_WORKERS;

    if (argc > 1)
    {
        num_workers = atoi(argv[1]);

        if (num_workers < 1)
            num_workers = DEFAULT_WORKERS;
    }

    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);

    printf("\n");
    printf("====================================================\n");
    printf("       MULTITHREADED LINUX APPLICATION\n");
    printf("          POSIX THREADS & MUTEXES DEMO\n");
    printf("====================================================\n");

    printf("CPU Cores Available : %ld\n", nprocs);
    printf("Total Data Elements : %d\n", DATA_SIZE);
    printf("Elements per Chunk  : %d\n", CHUNK_SIZE);
    printf("Total Work Items    : %d\n",
           (DATA_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE);
    printf("Worker Threads      : %d\n", num_workers);
    printf("Buffer Capacity     : %d work items\n", BUFFER_SIZE);

    printf("====================================================\n");

    g_data = malloc(sizeof(long) * DATA_SIZE);

    if (g_data == NULL)
    {
        printf("Memory allocation failed!\n");
        return 1;
    }

    for (long i = 0; i < DATA_SIZE; i++)
    {
        g_data[i] = i;
    }

    printf("\n>>> RUNNING SEQUENTIAL BASELINE <<<\n");

    double t0 = now_seconds();

    long long seq_result = run_sequential();

    double t1 = now_seconds();

    double seq_time = t1 - t0;

    printf("Sequential Result : %lld\n", seq_result);
    printf("Sequential Time   : %.4f seconds\n", seq_time);

    printf("\n>>> RUNNING MULTITHREADED VERSION <<<\n\n");

    double t2 = now_seconds();

    long long par_result = run_multithreaded(num_workers);

    double t3 = now_seconds();

    double par_time = t3 - t2;

    printf("\n");
    printf("====================================================\n");
    printf("                  FINAL RESULTS\n");
    printf("====================================================\n");

    printf("Sequential Result    : %lld\n", seq_result);
    printf("Multithreaded Result : %lld\n", par_result);

    printf("Correctness Check    : %s\n",
           (seq_result == par_result) ? "PASSED" : "FAILED");

    printf("Sequential Time      : %.4f seconds\n", seq_time);
    printf("Multithreaded Time   : %.4f seconds\n", par_time);

    if (par_time > 0.0)
    {
        printf("Speed-up             : %.2fx\n",
               seq_time / par_time);
    }

    printf("====================================================\n");

    free(g_data);

    return (seq_result == par_result) ? 0 : 1;
}


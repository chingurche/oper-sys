#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h> // только snprintf

static int MAX_THREADS;
static int active_threads = 0;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static int partition(int *a, int lo, int hi) {
    int pivot = a[lo + (hi - lo) / 2];
    int i = lo, j = hi;
    while (i <= j) {
        while (a[i] < pivot) i++;
        while (a[j] > pivot) j--;
        if (i <= j) {
            int t = a[i]; a[i] = a[j]; a[j] = t;
            i++; j--;
        }
    }
    return i;  
}

typedef struct {
    int *a;
    int  lo, hi;
} Task;

static void *worker(void *arg);

static int try_spawn(Task *t) {
    pthread_mutex_lock(&mtx);
    if (active_threads >= MAX_THREADS) {
        pthread_mutex_unlock(&mtx);
        return 0;
    }
    active_threads++;
    pthread_mutex_unlock(&mtx);

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, worker, t) != 0) {
        pthread_attr_destroy(&attr);
        pthread_mutex_lock(&mtx);
        active_threads--;
        pthread_mutex_unlock(&mtx);
        return 0;
    }
    pthread_attr_destroy(&attr);
    return 1;
}

static void parallel_quicksort_body(int *a, int lo, int hi) {
    if (lo >= hi) return;

    int p = partition(a, lo, hi);

    Task *left = malloc(sizeof(Task));
    left->a = a; left->lo = lo; left->hi = p - 1;

    if (!try_spawn(left)) {
        parallel_quicksort_body(a, lo, p - 1);
        free(left);
    }

    parallel_quicksort_body(a, p, hi);
}

static void *worker(void *arg) {
    Task *t = (Task *)arg;
    parallel_quicksort_body(t->a, t->lo, t->hi);
    free(t);

    pthread_mutex_lock(&mtx);
    active_threads--;
    pthread_mutex_unlock(&mtx);
    return NULL;
}

void parallel_quicksort(int *a, int n, int num_threads) {
    MAX_THREADS = num_threads;
    active_threads = 0;

    parallel_quicksort_body(a, 0, n - 1);

    for (;;) {
        pthread_mutex_lock(&mtx);
        int done = (active_threads == 0);
        pthread_mutex_unlock(&mtx);
        if (done) break;
        usleep(10);
    }
}

int main(int argc, char **argv) {
    int nthreads = (argc > 1) ? atoi(argv[1]) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    int n = (argc > 2) ? atoi(argv[2]) : 1000000; 

    int *a = malloc(n * sizeof(int));
    srand(42);
    for (int i = 0; i < n; i++) a[i] = rand();

    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);  

    parallel_quicksort(a, n, nthreads);

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed = (int)((end.tv_sec  - start.tv_sec)  * 1000.0
                  + (end.tv_nsec - start.tv_nsec) / 1e6); // погрешность 10мс из-за usleep

    for (int i = 1; i < n; i++) {
        if (a[i-1] > a[i]) {
            const char msg[] = "Алгоритм сортировки не работает\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            free(a);
            return 1;
        }
    }
    char msg[128];
    int len = snprintf(msg, sizeof(msg),
                    "Число потоков : %d\n"
                    "Время исполнения (мс) : %d\n",
                    nthreads, elapsed);
    write(STDOUT_FILENO, msg, len);
    free(a);
    return 0;
}
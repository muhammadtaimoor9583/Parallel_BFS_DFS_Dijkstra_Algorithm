#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define MAX_VERTICES 10000
#define MAX_THREAD_COUNT 8
#define WORK_REQUEST_THRESHOLD 5

typedef struct {
    int vertices;
    int** adj_matrix;
} Graph;

typedef struct {
    int* items;
    int front;
    int rear;
    int size;
    int capacity;
    pthread_mutex_t mutex;
} Queue;

typedef struct {
    int* buffer;
    int capacity;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} MessageQueue;

typedef struct {
    Graph* graph;
    bool* visited;
    int thread_id;
    int thread_count;
    Queue** local_queues;
    MessageQueue** message_queues;
    bool* termination_flag;
    pthread_mutex_t* visited_mutex;
    int* work_count;
    pthread_mutex_t* work_count_mutex;
    int search_value;
    bool* found_flag;
    pthread_mutex_t* found_mutex;
} ThreadArgs;

Queue* createQueue(int capacity) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) return NULL;
    
    q->items = (int*)malloc(capacity * sizeof(int));
    if (!q->items) {
        free(q);
        return NULL;
    }
    
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    q->capacity = capacity;
    pthread_mutex_init(&q->mutex, NULL);
    return q;
}

bool isQueueEmpty(Queue* q) {
    return q->size == 0;
}

void enqueue(Queue* q, int value) {
    pthread_mutex_lock(&q->mutex);
    if (q->size == q->capacity) {
        pthread_mutex_unlock(&q->mutex);
        return;
    }
    q->rear = (q->rear + 1) % q->capacity;
    q->items[q->rear] = value;
    q->size++;
    pthread_mutex_unlock(&q->mutex);
}

int dequeue(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    if (isQueueEmpty(q)) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    int item = q->items[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    pthread_mutex_unlock(&q->mutex);
    return item;
}

int queueSize(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    int size = q->size;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

MessageQueue* createMessageQueue(int capacity) {
    MessageQueue* mq = (MessageQueue*)malloc(sizeof(MessageQueue));
    if (!mq) return NULL;
    
    mq->buffer = (int*)malloc(capacity * sizeof(int));
    if (!mq->buffer) {
        free(mq);
        return NULL;
    }
    
    mq->capacity = capacity;
    mq->size = 0;
    mq->front = 0;
    mq->rear = -1;
    pthread_mutex_init(&mq->mutex, NULL);
    pthread_cond_init(&mq->not_empty, NULL);
    pthread_cond_init(&mq->not_full, NULL);
    return mq;
}

void sendMessage(MessageQueue* mq, int vertex) {
    pthread_mutex_lock(&mq->mutex);
    
    while (mq->size == mq->capacity) {
        pthread_cond_wait(&mq->not_full, &mq->mutex);
    }
    
    mq->rear = (mq->rear + 1) % mq->capacity;
    mq->buffer[mq->rear] = vertex;
    mq->size++;
    
    pthread_cond_signal(&mq->not_empty);
    pthread_mutex_unlock(&mq->mutex);
}

int receiveMessage(MessageQueue* mq, bool non_blocking) {
    pthread_mutex_lock(&mq->mutex);
    
    if (non_blocking && mq->size == 0) {
        pthread_mutex_unlock(&mq->mutex);
        return -1;
    }
    
    while (mq->size == 0) {
        pthread_cond_wait(&mq->not_empty, &mq->mutex);
    }
    
    int vertex = mq->buffer[mq->front];
    mq->front = (mq->front + 1) % mq->capacity;
    mq->size--;
    
    pthread_cond_signal(&mq->not_full);
    pthread_mutex_unlock(&mq->mutex);
    return vertex;
}

void* bfsThread(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    Graph* graph = t_args->graph;
    bool* visited = t_args->visited;
    int thread_id = t_args->thread_id;
    int thread_count = t_args->thread_count;
    Queue* local_queue = t_args->local_queues[thread_id];
    MessageQueue** message_queues = t_args->message_queues;
    bool* termination_flag = t_args->termination_flag;
    pthread_mutex_t* visited_mutex = t_args->visited_mutex;
    int* work_count = t_args->work_count;
    pthread_mutex_t* work_count_mutex = t_args->work_count_mutex;
    int search_value = t_args->search_value;
    bool* found_flag = t_args->found_flag;
    pthread_mutex_t* found_mutex = t_args->found_mutex;
    
    while (!(*termination_flag) && !(*found_flag)) {
        int current_vertex = dequeue(local_queue);
        
        // If local queue is empty, try to get work from message queue
        if (current_vertex == -1) {
            current_vertex = receiveMessage(message_queues[thread_id], true);
            
            // If no work in our queue, try work stealing from other threads
            if (current_vertex == -1) {
                bool found_work = false;
                for (int i = 0; i < thread_count && !found_work; i++) {
                    if (i != thread_id) {
                        current_vertex = receiveMessage(message_queues[i], true);
                        if (current_vertex != -1) {
                            found_work = true;
                        }
                    }
                }
                
                // If still no work, check termination condition
                if (!found_work) {
                    pthread_mutex_lock(work_count_mutex);
                    if (*work_count == 0) {
                        *termination_flag = true;
                    }
                    pthread_mutex_unlock(work_count_mutex);
                    
                    usleep(10);  // Sleep a bit to avoid busy waiting
                    continue;
                }
            }
        }
        
        // Process the current vertex
        if (current_vertex != -1) {
            // Check if this is the value we're searching for
            if (current_vertex == search_value) {
                pthread_mutex_lock(found_mutex);
                *found_flag = true;
                pthread_mutex_unlock(found_mutex);
                
                // Signal termination
                *termination_flag = true;
                pthread_mutex_lock(work_count_mutex);
                *work_count = 0;
                pthread_mutex_unlock(work_count_mutex);
                return NULL;
            }
            
            pthread_mutex_lock(work_count_mutex);
            (*work_count)--;
            pthread_mutex_unlock(work_count_mutex);
            
            // Process neighbors
            for (int i = 0; i < graph->vertices; i++) {
                if (graph->adj_matrix[current_vertex][i]) {
                    pthread_mutex_lock(visited_mutex);
                    if (!visited[i]) {
                        visited[i] = true;
                        pthread_mutex_unlock(visited_mutex);
                        
                        // Add neighbor to work count
                        pthread_mutex_lock(work_count_mutex);
                        (*work_count)++;
                        pthread_mutex_unlock(work_count_mutex);
                        
                        // Decide whether to process locally or distribute work
                        if (queueSize(local_queue) < WORK_REQUEST_THRESHOLD) {
                            // Keep the work locally
                            enqueue(local_queue, i);
                        } else {
                            // Distribute work to the thread with least work
                            int min_work_thread = 0;
                            int min_work = queueSize(t_args->local_queues[0]);
                            
                            for (int j = 1; j < thread_count; j++) {
                                int work_size = queueSize(t_args->local_queues[j]);
                                if (work_size < min_work) {
                                    min_work = work_size;
                                    min_work_thread = j;
                                }
                            }
                            
                            // Send work to the thread with the least work
                            sendMessage(message_queues[min_work_thread], i);
                        }
                    } else {
                        pthread_mutex_unlock(visited_mutex);
                    }
                }
            }
        }
    }
    
    return NULL;
}

Graph* createGraph(int vertices) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    if (!graph) return NULL;
    
    graph->vertices = vertices;
    graph->adj_matrix = (int**)malloc(vertices * sizeof(int*));
    if (!graph->adj_matrix) {
        free(graph);
        return NULL;
    }
    
    for (int i = 0; i < vertices; i++) {
        graph->adj_matrix[i] = (int*)calloc(vertices, sizeof(int));
        if (!graph->adj_matrix[i]) {
            for (int j = 0; j < i; j++) {
                free(graph->adj_matrix[j]);
            }
            free(graph->adj_matrix);
            free(graph);
            return NULL;
        }
    }
    return graph;
}

void addEdge(Graph* graph, int src, int dest) {
    graph->adj_matrix[src][dest] = 1;  // Forward edge
    graph->adj_matrix[dest][src] = 1;  // Backward edge to make it undirected
}

Graph* readGraphFromFile(const char* filename, int* search_value) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file %s\n", filename);
        return NULL;
    }
    
    int vertices, edges;
    fscanf(file, "%d %d", &vertices, &edges);
    
    Graph* graph = createGraph(vertices);
    if (!graph) {
        fclose(file);
        return NULL;
    }
    
    for (int i = 0; i < edges; i++) {
        int src, dest;
        if (fscanf(file, "%d %d", &src, &dest) != 2) {
            printf("Error reading edge %d\n", i);
            for (int j = 0; j < graph->vertices; j++) {
                free(graph->adj_matrix[j]);
            }
            free(graph->adj_matrix);
            free(graph);
            fclose(file);
            return NULL;
        }
        addEdge(graph, src, dest);
    }
    
    // Read the search value
    if (fscanf(file, "%d", search_value) != 1) {
        printf("Error reading search value\n");
        for (int j = 0; j < graph->vertices; j++) {
            free(graph->adj_matrix[j]);
        }
        free(graph->adj_matrix);
        free(graph);
        fclose(file);
        return NULL;
    }
    
    fclose(file);
    return graph;
}

bool parallelBFS(Graph* graph, int start_vertex, int search_value, int thread_count) {
    if (thread_count < 1 || thread_count > MAX_THREAD_COUNT) {
        printf("Invalid thread count: %d, must be between 1 and %d\n", 
               thread_count, MAX_THREAD_COUNT);
        return false;
    }
    
    bool* visited = (bool*)calloc(graph->vertices, sizeof(bool));
    if (!visited) {
        printf("Memory allocation failed for visited array\n");
        return false;
    }
    
    pthread_t* threads = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
    if (!threads) {
        free(visited);
        printf("Memory allocation failed for threads\n");
        return false;
    }
    
    ThreadArgs* thread_args = (ThreadArgs*)malloc(thread_count * sizeof(ThreadArgs));
    if (!thread_args) {
        free(visited);
        free(threads);
        printf("Memory allocation failed for thread arguments\n");
        return false;
    }
    
    // Create local queues for each thread
    Queue** local_queues = (Queue**)malloc(thread_count * sizeof(Queue*));
    if (!local_queues) {
        free(visited);
        free(threads);
        free(thread_args);
        printf("Memory allocation failed for local queues\n");
        return false;
    }
    
    for (int i = 0; i < thread_count; i++) {
        local_queues[i] = createQueue(graph->vertices / thread_count + 1);
        if (!local_queues[i]) {
            for (int j = 0; j < i; j++) {
                free(local_queues[j]->items);
                pthread_mutex_destroy(&local_queues[j]->mutex);
                free(local_queues[j]);
            }
            free(local_queues);
            free(visited);
            free(threads);
            free(thread_args);
            printf("Memory allocation failed for queue %d\n", i);
            return false;
        }
    }
    
    // Create message queues for inter-thread communication
    MessageQueue** message_queues = (MessageQueue**)malloc(thread_count * sizeof(MessageQueue*));
    if (!message_queues) {
        for (int i = 0; i < thread_count; i++) {
            free(local_queues[i]->items);
            pthread_mutex_destroy(&local_queues[i]->mutex);
            free(local_queues[i]);
        }
        free(local_queues);
        free(visited);
        free(threads);
        free(thread_args);
        printf("Memory allocation failed for message queues\n");
        return false;
    }
    
    for (int i = 0; i < thread_count; i++) {
        message_queues[i] = createMessageQueue(graph->vertices / thread_count + 1);
        if (!message_queues[i]) {
            for (int j = 0; j < i; j++) {
                free(message_queues[j]->buffer);
                pthread_mutex_destroy(&message_queues[j]->mutex);
                pthread_cond_destroy(&message_queues[j]->not_empty);
                pthread_cond_destroy(&message_queues[j]->not_full);
                free(message_queues[j]);
            }
            for (int j = 0; j < thread_count; j++) {
                free(local_queues[j]->items);
                pthread_mutex_destroy(&local_queues[j]->mutex);
                free(local_queues[j]);
            }
            free(message_queues);
            free(local_queues);
            free(visited);
            free(threads);
            free(thread_args);
            printf("Memory allocation failed for message queue %d\n", i);
            return false;
        }
    }
    
    // Initialize termination flag, found flag and work count
    bool termination_flag = false;
    bool found_flag = false;
    int work_count = 1;  // Start with 1 for the initial vertex
    
    // Initialize mutex for visited array, work count, and found flag
    pthread_mutex_t visited_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t work_count_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t found_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // Initialize starting vertex
    visited[start_vertex] = true;
    enqueue(local_queues[0], start_vertex);  // Assign to the first thread
    
    // Create threads
    for (int i = 0; i < thread_count; i++) {
        thread_args[i].graph = graph;
        thread_args[i].visited = visited;
        thread_args[i].thread_id = i;
        thread_args[i].thread_count = thread_count;
        thread_args[i].local_queues = local_queues;
        thread_args[i].message_queues = message_queues;
        thread_args[i].termination_flag = &termination_flag;
        thread_args[i].visited_mutex = &visited_mutex;
        thread_args[i].work_count = &work_count;
        thread_args[i].work_count_mutex = &work_count_mutex;
        thread_args[i].search_value = search_value;
        thread_args[i].found_flag = &found_flag;
        thread_args[i].found_mutex = &found_mutex;
        
        int result = pthread_create(&threads[i], NULL, bfsThread, &thread_args[i]);
        if (result != 0) {
            printf("Error creating thread %d: %d\n", i, result);
            termination_flag = true;  // Signal other threads to terminate
            
            // Join any threads that were created
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            
            // Cleanup
            for (int j = 0; j < thread_count; j++) {
                free(local_queues[j]->items);
                pthread_mutex_destroy(&local_queues[j]->mutex);
                free(local_queues[j]);
                free(message_queues[j]->buffer);
                pthread_mutex_destroy(&message_queues[j]->mutex);
                pthread_cond_destroy(&message_queues[j]->not_empty);
                pthread_cond_destroy(&message_queues[j]->not_full);
                free(message_queues[j]);
            }
            free(local_queues);
            free(message_queues);
            pthread_mutex_destroy(&visited_mutex);
            pthread_mutex_destroy(&work_count_mutex);
            pthread_mutex_destroy(&found_mutex);
            free(visited);
            free(threads);
            free(thread_args);
            return false;
        }
    }
    
    // Join threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Store the result before cleanup
    bool result = found_flag;
    
    // Cleanup
    free(visited);
    for (int i = 0; i < thread_count; i++) {
        free(local_queues[i]->items);
        pthread_mutex_destroy(&local_queues[i]->mutex);
        free(local_queues[i]);
        free(message_queues[i]->buffer);
        pthread_mutex_destroy(&message_queues[i]->mutex);
        pthread_cond_destroy(&message_queues[i]->not_empty);
        pthread_cond_destroy(&message_queues[i]->not_full);
        free(message_queues[i]);
    }
    free(local_queues);
    free(message_queues);
    pthread_mutex_destroy(&visited_mutex);
    pthread_mutex_destroy(&work_count_mutex);
    pthread_mutex_destroy(&found_mutex);
    free(threads);
    free(thread_args);
    
    return result;
}

int main() {
    int search_value;
    Graph* graph = readGraphFromFile("../dfs/large_graph.txt", &search_value);
    
    if (graph == NULL) {
        return 1;
    }
    
    printf("Searching for number: %d\n", search_value);
    
    double execution_times[MAX_THREAD_COUNT];
    double speedups[MAX_THREAD_COUNT];
    double efficiencies[MAX_THREAD_COUNT];
    
    printf("\nRunning sequential BFS (1 thread)...\n");
    clock_t start_time, end_time;
    double cpu_time_used;
    
    start_time = clock();
    bool found = parallelBFS(graph, 0, search_value, 1);
    end_time = clock();
    
    cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    execution_times[0] = cpu_time_used;
    speedups[0] = 1.0;  // Speedup relative to itself is 1
    efficiencies[0] = 1.0;  // Efficiency is speedup/thread_count = 1/1 = 1
    
    if (found) {
        printf("Found.\n");
    } else {
        printf("Not found.\n");
    }
    
    printf("It takes time: %.6f seconds\n", cpu_time_used);
    
    for (int thread_count = 2; thread_count <= MAX_THREAD_COUNT; thread_count++) {
        printf("\nRunning parallel BFS with %d threads...\n", thread_count);
        
        start_time = clock();
        found = parallelBFS(graph, 0, search_value, thread_count);
        end_time = clock();
        
        cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        execution_times[thread_count-1] = cpu_time_used;
        
        // Calculate speedup and efficiency with capping
        speedups[thread_count-1] = execution_times[0] / cpu_time_used;
        
        // Cap speedup to not exceed thread count (theoretical maximum)
        if (speedups[thread_count-1] > thread_count) {
            printf("Note: Measured speedup (%.2f) exceeds theoretical maximum. Capping to %d.\n", 
                  speedups[thread_count-1], thread_count);
            speedups[thread_count-1] = thread_count;
        }
        
        efficiencies[thread_count-1] = speedups[thread_count-1] / thread_count;
        
        if (found) {
            printf("Found.\n");
        } else {
            printf("Not found.\n");
        }
        
        printf("It takes time: %.6f seconds\n", cpu_time_used);
        printf("Speedup: %.6f, Efficiency: %.6f\n", 
               speedups[thread_count-1], efficiencies[thread_count-1]);
    }
    
    // Generate data for plotting execution time
    FILE *fp = fopen("thread_performance.dat", "w");
    if (fp != NULL) {
        fprintf(fp, "# Thread_Count Execution_Time Speedup Efficiency\n");
        for (int i = 0; i < MAX_THREAD_COUNT; i++) {
            fprintf(fp, "%d %.6f %.6f %.6f\n", i+1, execution_times[i], 
                    speedups[i], efficiencies[i]);
        }
        fclose(fp);
        printf("\nPerformance data saved to thread_performance.dat\n");
    }
    
    // Generate gnuplot script for execution time
    FILE *gnuplot = fopen("plot_performance.gp", "w");
    if (gnuplot != NULL) {
        fprintf(gnuplot, "set terminal png size 800,600\n");
        fprintf(gnuplot, "set output 'thread_performance.png'\n");
        fprintf(gnuplot, "set title 'Parallel BFS Performance: Threads vs. Execution Time'\n");
        fprintf(gnuplot, "set xlabel 'Number of Threads'\n");
        fprintf(gnuplot, "set ylabel 'Execution Time (seconds)'\n");
        fprintf(gnuplot, "set grid\n");
        fprintf(gnuplot, "set style data linespoints\n");
        fprintf(gnuplot, "set key top right\n");
        fprintf(gnuplot, "plot 'thread_performance.dat' using 1:2 title 'Execution Time' with linespoints lw 2 pt 7\n");
        fclose(gnuplot);
    }
    
    // Generate gnuplot script for speedup
    gnuplot = fopen("plot_speedup.gp", "w");
    if (gnuplot != NULL) {
        fprintf(gnuplot, "set terminal png size 800,600\n");
        fprintf(gnuplot, "set output 'speedup.png'\n");
        fprintf(gnuplot, "set title 'Parallel BFS Performance: Threads vs. Speedup'\n");
        fprintf(gnuplot, "set xlabel 'Number of Threads'\n");
        fprintf(gnuplot, "set ylabel 'Speedup'\n");
        fprintf(gnuplot, "set grid\n");
        fprintf(gnuplot, "set style data linespoints\n");
        fprintf(gnuplot, "set key top left\n");
        fprintf(gnuplot, "plot 'thread_performance.dat' using 1:3 title 'Speedup' with linespoints lw 2 pt 7,\\\n");
        fprintf(gnuplot, "     x title 'Linear Speedup' with lines lt 2\n");
        fclose(gnuplot);
        printf("Speedup graph script generated. Run 'gnuplot plot_speedup.gp' to generate the graph.\n");
    }
    
    // Generate gnuplot script for efficiency
    gnuplot = fopen("plot_efficiency.gp", "w");
    if (gnuplot != NULL) {
        fprintf(gnuplot, "set terminal png size 800,600\n");
        fprintf(gnuplot, "set output 'efficiency.png'\n");
        fprintf(gnuplot, "set title 'Parallel BFS Performance: Threads vs. Efficiency'\n");
        fprintf(gnuplot, "set xlabel 'Number of Threads'\n");
        fprintf(gnuplot, "set ylabel 'Efficiency'\n");
        fprintf(gnuplot, "set grid\n");
        fprintf(gnuplot, "set style data linespoints\n");
        fprintf(gnuplot, "set key top right\n");
        fprintf(gnuplot, "set yrange [0:1.1]\n");
        fprintf(gnuplot, "plot 'thread_performance.dat' using 1:4 title 'Efficiency' with linespoints lw 2 pt 7,\\\n");
        fprintf(gnuplot, "     1 title 'Ideal Efficiency' with lines lt 2\n");
        fclose(gnuplot);
        printf("Efficiency graph script generated. Run 'gnuplot plot_efficiency.gp' to generate the graph.\n");
    }
    
    // Cleanup graph
    for (int i = 0; i < graph->vertices; i++) {
        free(graph->adj_matrix[i]);
    }
    free(graph->adj_matrix);
    free(graph);
    
    return 0;
}
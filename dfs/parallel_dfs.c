#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

// Update to ensure we can handle the required vertices from the large graph
#define MAX_VERTICES 10000
#define MAX_THREAD_COUNT 8  // Maximum number of threads to test
#define WORK_REQUEST_THRESHOLD 5  // Increased to improve load balancing

// Graph structure
typedef struct {
    int vertices;
    int** adj_matrix;
} Graph;

// Stack structure for DFS
typedef struct {
    int* items;
    int top;
    int size;
    pthread_mutex_t mutex;
} Stack;

// Message queue for work distribution
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

// Thread arguments structure
typedef struct {
    Graph* graph;
    bool* visited;
    int thread_id;
    int thread_count;  // Added to store thread count
    Stack** local_stacks;
    MessageQueue** message_queues;
    bool* termination_flag;
    pthread_mutex_t* visited_mutex;
    int* work_count;
    pthread_mutex_t* work_count_mutex;
    int search_value;
    bool* found_flag;
    pthread_mutex_t* found_mutex;
} ThreadArgs;

// Initialize stack
Stack* createStack(int size) {
    Stack* s = (Stack*)malloc(sizeof(Stack));
    if (!s) return NULL;
    
    s->items = (int*)malloc(size * sizeof(int));
    if (!s->items) {
        free(s);
        return NULL;
    }
    
    s->top = -1;
    s->size = size;
    pthread_mutex_init(&s->mutex, NULL);
    return s;
}

// Check if stack is empty
bool isStackEmpty(Stack* s) {
    return s->top == -1;
}

// Push element to stack
void push(Stack* s, int value) {
    pthread_mutex_lock(&s->mutex);
    if (s->top == s->size - 1) {
        pthread_mutex_unlock(&s->mutex);
        return;
    }
    s->top++;
    s->items[s->top] = value;
    pthread_mutex_unlock(&s->mutex);
}

// Pop element from stack
int pop(Stack* s) {
    pthread_mutex_lock(&s->mutex);
    if (isStackEmpty(s)) {
        pthread_mutex_unlock(&s->mutex);
        return -1;
    }
    int item = s->items[s->top];
    s->top--;
    pthread_mutex_unlock(&s->mutex);
    return item;
}

// Get stack size
int stackSize(Stack* s) {
    pthread_mutex_lock(&s->mutex);
    int size = s->top + 1;
    pthread_mutex_unlock(&s->mutex);
    return size;
}

// Initialize message queue
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

// Send message to queue
void sendMessage(MessageQueue* mq, int vertex) {
    pthread_mutex_lock(&mq->mutex);
    
    // Wait until queue is not full
    while (mq->size == mq->capacity) {
        pthread_cond_wait(&mq->not_full, &mq->mutex);
    }
    
    mq->rear = (mq->rear + 1) % mq->capacity;
    mq->buffer[mq->rear] = vertex;
    mq->size++;
    
    pthread_cond_signal(&mq->not_empty);
    pthread_mutex_unlock(&mq->mutex);
}

// Receive message from queue with timeout
int receiveMessage(MessageQueue* mq, bool non_blocking) {
    pthread_mutex_lock(&mq->mutex);
    
    if (non_blocking && mq->size == 0) {
        pthread_mutex_unlock(&mq->mutex);
        return -1;
    }
    
    // Wait until queue is not empty
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

// Thread function for parallel DFS
void* dfsThread(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    Graph* graph = t_args->graph;
    bool* visited = t_args->visited;
    int thread_id = t_args->thread_id;
    int thread_count = t_args->thread_count;  // Use the passed thread count
    Stack* local_stack = t_args->local_stacks[thread_id];
    MessageQueue** message_queues = t_args->message_queues;
    bool* termination_flag = t_args->termination_flag;
    pthread_mutex_t* visited_mutex = t_args->visited_mutex;
    int* work_count = t_args->work_count;
    pthread_mutex_t* work_count_mutex = t_args->work_count_mutex;
    int search_value = t_args->search_value;
    bool* found_flag = t_args->found_flag;
    pthread_mutex_t* found_mutex = t_args->found_mutex;
    
    while (!(*termination_flag) && !(*found_flag)) {
        int current_vertex = pop(local_stack);
        
        // If local stack is empty, try to get work from message queue
        if (current_vertex == -1) {
            // Check our message queue for work
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
                    
                    // Sleep a bit to avoid busy waiting
                    usleep(10);
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
                        if (stackSize(local_stack) < WORK_REQUEST_THRESHOLD) {
                            // Keep the work locally
                            push(local_stack, i);
                        } else {
                            // Distribute work to the thread with the least amount of work
                            int min_work_thread = 0;
                            int min_work = stackSize(t_args->local_stacks[0]);
                            
                            for (int j = 1; j < thread_count; j++) {
                                int work_size = stackSize(t_args->local_stacks[j]);
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

// Create graph
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
            // Free previously allocated memory
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

// Add edge to graph
void addEdge(Graph* graph, int src, int dest) {
    graph->adj_matrix[src][dest] = 1;  // Forward edge
    graph->adj_matrix[dest][src] = 1;  // Backward edge to make the graph undirected
}

// Read graph from file
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
            // Cleanup
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
        // Cleanup
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

// Parallel DFS implementation with variable thread count
bool parallelDFS(Graph* graph, int start_vertex, int search_value, int thread_count) {
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
    
    // Create local stacks for each thread
    Stack** local_stacks = (Stack**)malloc(thread_count * sizeof(Stack*));
    if (!local_stacks) {
        free(visited);
        free(threads);
        free(thread_args);
        printf("Memory allocation failed for local stacks\n");
        return false;
    }
    
    for (int i = 0; i < thread_count; i++) {
        local_stacks[i] = createStack(graph->vertices / thread_count + 1);
        if (!local_stacks[i]) {
            // Cleanup previously created stacks
            for (int j = 0; j < i; j++) {
                free(local_stacks[j]->items);
                pthread_mutex_destroy(&local_stacks[j]->mutex);
                free(local_stacks[j]);
            }
            free(local_stacks);
            free(visited);
            free(threads);
            free(thread_args);
            printf("Memory allocation failed for stack %d\n", i);
            return false;
        }
    }
    
    // Create message queues for inter-thread communication
    MessageQueue** message_queues = (MessageQueue**)malloc(thread_count * sizeof(MessageQueue*));
    if (!message_queues) {
        // Cleanup stacks
        for (int i = 0; i < thread_count; i++) {
            free(local_stacks[i]->items);
            pthread_mutex_destroy(&local_stacks[i]->mutex);
            free(local_stacks[i]);
        }
        free(local_stacks);
        free(visited);
        free(threads);
        free(thread_args);
        printf("Memory allocation failed for message queues\n");
        return false;
    }
    
    for (int i = 0; i < thread_count; i++) {
        message_queues[i] = createMessageQueue(graph->vertices / thread_count + 1);
        if (!message_queues[i]) {
            // Cleanup previously created queues
            for (int j = 0; j < i; j++) {
                free(message_queues[j]->buffer);
                pthread_mutex_destroy(&message_queues[j]->mutex);
                pthread_cond_destroy(&message_queues[j]->not_empty);
                pthread_cond_destroy(&message_queues[j]->not_full);
                free(message_queues[j]);
            }
            // Cleanup stacks
            for (int j = 0; j < thread_count; j++) {
                free(local_stacks[j]->items);
                pthread_mutex_destroy(&local_stacks[j]->mutex);
                free(local_stacks[j]);
            }
            free(message_queues);
            free(local_stacks);
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
    push(local_stacks[0], start_vertex);  // Assign to the first thread
    
    // Create threads
    for (int i = 0; i < thread_count; i++) {
        thread_args[i].graph = graph;
        thread_args[i].visited = visited;
        thread_args[i].thread_id = i;
        thread_args[i].thread_count = thread_count;  // Pass the thread count
        thread_args[i].local_stacks = local_stacks;
        thread_args[i].message_queues = message_queues;
        thread_args[i].termination_flag = &termination_flag;
        thread_args[i].visited_mutex = &visited_mutex;
        thread_args[i].work_count = &work_count;
        thread_args[i].work_count_mutex = &work_count_mutex;
        thread_args[i].search_value = search_value;
        thread_args[i].found_flag = &found_flag;
        thread_args[i].found_mutex = &found_mutex;
        
        int result = pthread_create(&threads[i], NULL, dfsThread, &thread_args[i]);
        if (result != 0) {
            printf("Error creating thread %d: %d\n", i, result);
            termination_flag = true;  // Signal other threads to terminate
            
            // Join any threads that were created
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            
            // Cleanup
            for (int j = 0; j < thread_count; j++) {
                free(local_stacks[j]->items);
                pthread_mutex_destroy(&local_stacks[j]->mutex);
                free(local_stacks[j]);
                free(message_queues[j]->buffer);
                pthread_mutex_destroy(&message_queues[j]->mutex);
                pthread_cond_destroy(&message_queues[j]->not_empty);
                pthread_cond_destroy(&message_queues[j]->not_full);
                free(message_queues[j]);
            }
            free(local_stacks);
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
        free(local_stacks[i]->items);
        pthread_mutex_destroy(&local_stacks[i]->mutex);
        free(local_stacks[i]);
        free(message_queues[i]->buffer);
        pthread_mutex_destroy(&message_queues[i]->mutex);
        pthread_cond_destroy(&message_queues[i]->not_empty);
        pthread_cond_destroy(&message_queues[i]->not_full);
        free(message_queues[i]);
    }
    free(local_stacks);
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
    Graph* graph = readGraphFromFile("large_graph.txt", &search_value);
    
    if (graph == NULL) {
        return 1;
    }
    
    printf("Searching for number: %d\n", search_value);
    
    double execution_times[MAX_THREAD_COUNT];
    double speedups[MAX_THREAD_COUNT];
    double efficiencies[MAX_THREAD_COUNT];
    
    printf("\nRunning sequential DFS (1 thread)...\n");
    clock_t start_time, end_time;
    double cpu_time_used;
    
    start_time = clock();
    bool found = parallelDFS(graph, 0, search_value, 1);
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
        printf("\nRunning parallel DFS with %d threads...\n", thread_count);
        
        start_time = clock();
        found = parallelDFS(graph, 0, search_value, thread_count);
        end_time = clock();
        
        cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
        execution_times[thread_count-1] = cpu_time_used;
        
        // Calculate speedup and efficiency
        speedups[thread_count-1] = execution_times[0] / cpu_time_used;
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
        fprintf(gnuplot, "set title 'Parallel DFS Performance: Threads vs. Execution Time'\n");
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
        fprintf(gnuplot, "set title 'Parallel DFS Performance: Threads vs. Speedup'\n");
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
        fprintf(gnuplot, "set title 'Parallel DFS Performance: Threads vs. Efficiency'\n");
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
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_VERTICES 1000
#define MAX_THREADS 4

// Graph structure
typedef struct {
    int vertices;
    int** adj_matrix;
} Graph;

// Queue structure for BFS
typedef struct {
    int* items;
    int front;
    int rear;
    int size;
    pthread_mutex_t mutex;
} Queue;

// Thread arguments structure
typedef struct {
    Graph* graph;
    Queue* queue;
    bool* visited;
    int thread_id;
    int* level;
} ThreadArgs;

// Initialize queue
Queue* createQueue(int size) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->items = (int*)malloc(size * sizeof(int));
    q->front = -1;
    q->rear = -1;
    q->size = size;
    pthread_mutex_init(&q->mutex, NULL);
    return q;
}

// Check if queue is empty
bool isEmpty(Queue* q) {
    return q->front == -1;
}

// Add element to queue
void enqueue(Queue* q, int value) {
    pthread_mutex_lock(&q->mutex);
    if (q->rear == q->size - 1) {
        pthread_mutex_unlock(&q->mutex);
        return;
    }
    if (q->front == -1)
        q->front = 0;
    q->rear++;
    q->items[q->rear] = value;
    pthread_mutex_unlock(&q->mutex);
}

// Remove element from queue
int dequeue(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    if (isEmpty(q)) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    int item = q->items[q->front];
    q->front++;
    if (q->front > q->rear) {
        q->front = q->rear = -1;
    }
    pthread_mutex_unlock(&q->mutex);
    return item;
}

// Thread function for parallel BFS
void* bfsThread(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    Graph* graph = t_args->graph;
    Queue* queue = t_args->queue;
    bool* visited = t_args->visited;
    int thread_id = t_args->thread_id;
    int* level = t_args->level;

    while (1) {
        int current_vertex = dequeue(queue);
        if (current_vertex == -1) {
            break;
        }

        // Process neighbors
        for (int i = 0; i < graph->vertices; i++) {
            if (graph->adj_matrix[current_vertex][i] && !visited[i]) {
                visited[i] = true;
                level[i] = level[current_vertex] + 1;
                enqueue(queue, i);
            }
        }
    }

    return NULL;
}

// Create graph
Graph* createGraph(int vertices) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    graph->vertices = vertices;
    graph->adj_matrix = (int**)malloc(vertices * sizeof(int*));
    for (int i = 0; i < vertices; i++) {
        graph->adj_matrix[i] = (int*)calloc(vertices, sizeof(int));
    }
    return graph;
}

// Add edge to graph
void addEdge(Graph* graph, int src, int dest) {
    graph->adj_matrix[src][dest] = 1;
    graph->adj_matrix[dest][src] = 1;
}

// Parallel BFS implementation
void parallelBFS(Graph* graph, int start_vertex) {
    Queue* queue = createQueue(MAX_VERTICES);
    bool* visited = (bool*)calloc(graph->vertices, sizeof(bool));
    int* level = (int*)calloc(graph->vertices, sizeof(int));
    pthread_t threads[MAX_THREADS];
    ThreadArgs thread_args[MAX_THREADS];

    // Initialize starting vertex
    visited[start_vertex] = true;
    level[start_vertex] = 0;
    enqueue(queue, start_vertex);

    // Create threads
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_args[i].graph = graph;
        thread_args[i].queue = queue;
        thread_args[i].visited = visited;
        thread_args[i].thread_id = i;
        thread_args[i].level = level;
        pthread_create(&threads[i], NULL, bfsThread, &thread_args[i]);
    }

    // Join threads
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Print BFS levels
    printf("Vertex\tLevel\n");
    for (int i = 0; i < graph->vertices; i++) {
        printf("%d\t%d\n", i, level[i]);
    }

    // Cleanup
    free(visited);
    free(level);
    free(queue->items);
    free(queue);
}

int main() {
    // Create a sample graph
    Graph* graph = createGraph(6);
    addEdge(graph, 0, 1);
    addEdge(graph, 0, 2);
    addEdge(graph, 1, 3);
    addEdge(graph, 2, 4);
    addEdge(graph, 3, 5);

    printf("Parallel BFS starting from vertex 0:\n");
    parallelBFS(graph, 0);

    // Cleanup graph
    for (int i = 0; i < graph->vertices; i++) {
        free(graph->adj_matrix[i]);
    }
    free(graph->adj_matrix);
    free(graph);

    return 0;
} 
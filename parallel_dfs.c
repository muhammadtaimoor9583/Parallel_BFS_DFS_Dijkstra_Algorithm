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

// Stack structure for DFS
typedef struct {
    int* items;
    int top;
    int size;
    pthread_mutex_t mutex;
} Stack;

// Thread arguments structure
typedef struct {
    Graph* graph;
    Stack* stack;
    bool* visited;
    int thread_id;
} ThreadArgs;

// Initialize stack
Stack* createStack(int size) {
    Stack* s = (Stack*)malloc(sizeof(Stack));
    s->items = (int*)malloc(size * sizeof(int));
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

// Thread function for parallel DFS
void* dfsThread(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    Graph* graph = t_args->graph;
    Stack* stack = t_args->stack;
    bool* visited = t_args->visited;

    while (1) {
        int current_vertex = pop(stack);
        if (current_vertex == -1) {
            break;
        }

        // Process neighbors
        for (int i = 0; i < graph->vertices; i++) {
            if (graph->adj_matrix[current_vertex][i] && !visited[i]) {
                visited[i] = true;
                push(stack, i);
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

// Parallel DFS implementation
void parallelDFS(Graph* graph, int start_vertex) {
    Stack* stack = createStack(MAX_VERTICES);
    bool* visited = (bool*)calloc(graph->vertices, sizeof(bool));
    pthread_t threads[MAX_THREADS];
    ThreadArgs thread_args[MAX_THREADS];

    // Initialize starting vertex
    visited[start_vertex] = true;
    push(stack, start_vertex);

    // Create threads
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_args[i].graph = graph;
        thread_args[i].stack = stack;
        thread_args[i].visited = visited;
        thread_args[i].thread_id = i;
        pthread_create(&threads[i], NULL, dfsThread, &thread_args[i]);
    }

    // Join threads
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Print DFS traversal order
    printf("DFS Traversal Order:\n");
    for (int i = 0; i < graph->vertices; i++) {
        if (visited[i]) {
            printf("%d ", i);
        }
    }
    printf("\n");

    // Cleanup
    free(visited);
    free(stack->items);
    free(stack);
}

int main() {
    // Create a sample graph
    Graph* graph = createGraph(6);
    addEdge(graph, 0, 1);
    addEdge(graph, 0, 2);
    addEdge(graph, 1, 3);
    addEdge(graph, 2, 4);
    addEdge(graph, 3, 5);

    printf("Parallel DFS starting from vertex 0:\n");
    parallelDFS(graph, 0);

    // Cleanup graph
    for (int i = 0; i < graph->vertices; i++) {
        free(graph->adj_matrix[i]);
    }
    free(graph->adj_matrix);
    free(graph);

    return 0;
} 
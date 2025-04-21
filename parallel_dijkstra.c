#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <stdbool.h>

#define MAX_VERTICES 1000
#define MAX_THREADS 4
#define INF INT_MAX

// Graph structure with weights
typedef struct {
    int vertices;
    int** adj_matrix;
    int** weight_matrix;
} Graph;

// Priority queue structure
typedef struct {
    int* vertices;
    int* distances;
    int size;
    pthread_mutex_t mutex;
} PriorityQueue;

// Thread arguments structure
typedef struct {
    Graph* graph;
    PriorityQueue* pq;
    int* dist;
    bool* visited;
    int thread_id;
    int start_vertex;
} ThreadArgs;

// Initialize priority queue
PriorityQueue* createPriorityQueue(int size) {
    PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    pq->vertices = (int*)malloc(size * sizeof(int));
    pq->distances = (int*)malloc(size * sizeof(int));
    pq->size = 0;
    pthread_mutex_init(&pq->mutex, NULL);
    return pq;
}

// Check if priority queue is empty
bool isPriorityQueueEmpty(PriorityQueue* pq) {
    return pq->size == 0;
}

// Insert into priority queue
void insert(PriorityQueue* pq, int vertex, int distance) {
    pthread_mutex_lock(&pq->mutex);
    int i = pq->size;
    pq->size++;
    
    while (i > 0 && distance < pq->distances[(i-1)/2]) {
        pq->distances[i] = pq->distances[(i-1)/2];
        pq->vertices[i] = pq->vertices[(i-1)/2];
        i = (i-1)/2;
    }
    
    pq->distances[i] = distance;
    pq->vertices[i] = vertex;
    pthread_mutex_unlock(&pq->mutex);
}

// Extract minimum from priority queue
int extractMin(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->mutex);
    if (isPriorityQueueEmpty(pq)) {
        pthread_mutex_unlock(&pq->mutex);
        return -1;
    }

    int min_vertex = pq->vertices[0];
    pq->size--;
    
    if (pq->size > 0) {
        pq->vertices[0] = pq->vertices[pq->size];
        pq->distances[0] = pq->distances[pq->size];
        
        int i = 0;
        while (1) {
            int left = 2*i + 1;
            int right = 2*i + 2;
            int smallest = i;
            
            if (left < pq->size && pq->distances[left] < pq->distances[smallest])
                smallest = left;
            if (right < pq->size && pq->distances[right] < pq->distances[smallest])
                smallest = right;
            
            if (smallest == i) break;
            
            // Swap
            int temp_vertex = pq->vertices[i];
            int temp_dist = pq->distances[i];
            pq->vertices[i] = pq->vertices[smallest];
            pq->distances[i] = pq->distances[smallest];
            pq->vertices[smallest] = temp_vertex;
            pq->distances[smallest] = temp_dist;
            
            i = smallest;
        }
    }
    
    pthread_mutex_unlock(&pq->mutex);
    return min_vertex;
}

// Create graph
Graph* createGraph(int vertices) {
    Graph* graph = (Graph*)malloc(sizeof(Graph));
    graph->vertices = vertices;
    graph->adj_matrix = (int**)malloc(vertices * sizeof(int*));
    graph->weight_matrix = (int**)malloc(vertices * sizeof(int*));
    
    for (int i = 0; i < vertices; i++) {
        graph->adj_matrix[i] = (int*)calloc(vertices, sizeof(int));
        graph->weight_matrix[i] = (int*)malloc(vertices * sizeof(int));
        for (int j = 0; j < vertices; j++) {
            graph->weight_matrix[i][j] = INF;
        }
    }
    return graph;
}

// Add edge to graph with weight
void addEdge(Graph* graph, int src, int dest, int weight) {
    graph->adj_matrix[src][dest] = 1;
    graph->weight_matrix[src][dest] = weight;
    graph->adj_matrix[dest][src] = 1;
    graph->weight_matrix[dest][src] = weight;
}

// Thread function for parallel Dijkstra
void* dijkstraThread(void* args) {
    ThreadArgs* t_args = (ThreadArgs*)args;
    Graph* graph = t_args->graph;
    PriorityQueue* pq = t_args->pq;
    int* dist = t_args->dist;
    bool* visited = t_args->visited;

    while (1) {
        int u = extractMin(pq);
        if (u == -1) {
            break;
        }

        if (visited[u]) continue;
        visited[u] = true;

        // Process neighbors
        for (int v = 0; v < graph->vertices; v++) {
            if (graph->adj_matrix[u][v] && !visited[v]) {
                int new_dist = dist[u] + graph->weight_matrix[u][v];
                if (new_dist < dist[v]) {
                    dist[v] = new_dist;
                    insert(pq, v, new_dist);
                }
            }
        }
    }

    return NULL;
}

// Parallel Dijkstra implementation
void parallelDijkstra(Graph* graph, int start_vertex) {
    PriorityQueue* pq = createPriorityQueue(MAX_VERTICES);
    int* dist = (int*)malloc(graph->vertices * sizeof(int));
    bool* visited = (bool*)calloc(graph->vertices, sizeof(bool));
    pthread_t threads[MAX_THREADS];
    ThreadArgs thread_args[MAX_THREADS];

    // Initialize distances
    for (int i = 0; i < graph->vertices; i++) {
        dist[i] = INF;
    }
    dist[start_vertex] = 0;

    // Initialize priority queue
    insert(pq, start_vertex, 0);

    // Create threads
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_args[i].graph = graph;
        thread_args[i].pq = pq;
        thread_args[i].dist = dist;
        thread_args[i].visited = visited;
        thread_args[i].thread_id = i;
        thread_args[i].start_vertex = start_vertex;
        pthread_create(&threads[i], NULL, dijkstraThread, &thread_args[i]);
    }

    // Join threads
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Print shortest distances
    printf("Vertex\tDistance from Source\n");
    for (int i = 0; i < graph->vertices; i++) {
        if (dist[i] == INF)
            printf("%d\tINF\n", i);
        else
            printf("%d\t%d\n", i, dist[i]);
    }

    // Cleanup
    free(dist);
    free(visited);
    free(pq->vertices);
    free(pq->distances);
    free(pq);
}

int main() {
    // Create a sample graph
    Graph* graph = createGraph(6);
    addEdge(graph, 0, 1, 4);
    addEdge(graph, 0, 2, 1);
    addEdge(graph, 1, 3, 1);
    addEdge(graph, 2, 1, 2);
    addEdge(graph, 2, 3, 5);
    addEdge(graph, 3, 4, 3);
    addEdge(graph, 4, 5, 2);

    printf("Parallel Dijkstra starting from vertex 0:\n");
    parallelDijkstra(graph, 0);

    // Cleanup graph
    for (int i = 0; i < graph->vertices; i++) {
        free(graph->adj_matrix[i]);
        free(graph->weight_matrix[i]);
    }
    free(graph->adj_matrix);
    free(graph->weight_matrix);
    free(graph);

    return 0;
} 
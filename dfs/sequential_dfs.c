#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define MAX_VERTICES 10000

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
} Stack;

// Initialize stack
Stack* createStack(int size) {
    Stack* s = (Stack*)malloc(sizeof(Stack));
    s->items = (int*)malloc(size * sizeof(int));
    s->top = -1;
    s->size = size;
    return s;
}

// Check if stack is empty
bool isStackEmpty(Stack* s) {
    return s->top == -1;
}

// Push element to stack
void push(Stack* s, int value) {
    if (s->top == s->size - 1) {
        return;
    }
    s->top++;
    s->items[s->top] = value;
}

// Pop element from stack
int pop(Stack* s) {
    if (isStackEmpty(s)) {
        return -1;
    }
    int item = s->items[s->top];
    s->top--;
    return item;
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
    graph->adj_matrix[src][dest] = 1;  // Forward edge
    graph->adj_matrix[dest][src] = 1;  // Backward edge to make the graph undirected
}

// Sequential DFS implementation
bool sequentialDFS(Graph* graph, int start_vertex, int search_value) {
    Stack* stack = createStack(MAX_VERTICES);
    bool* visited = (bool*)calloc(graph->vertices, sizeof(bool));
    bool found = false;

    // Initialize starting vertex
    visited[start_vertex] = true;
    push(stack, start_vertex);
    

    // DFS traversal
    while (!isStackEmpty(stack)) {
        int current_vertex = pop(stack);
        
    

        // Check if we found the search value
        if (current_vertex == search_value) {
            found = true;
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

    
    // Cleanup
    free(visited);
    free(stack->items);
    free(stack);
    
    return found;
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
    
    for (int i = 0; i < edges; i++) {
        int src, dest;
        fscanf(file, "%d %d", &src, &dest);
        addEdge(graph, src, dest);
    }
    
    // Read the search value
    fscanf(file, "%d", search_value);
    
    fclose(file);
    return graph;
}

int main() {
    int search_value;
    Graph* graph = readGraphFromFile("large_graph.txt", &search_value);
    
    if (graph == NULL) {
        return 1;
    }
    
    printf("Searching for number: %d\n", search_value);
    
    clock_t start_time, end_time;
    double cpu_time_used;
    
    start_time = clock();
    bool found = sequentialDFS(graph, 0, search_value);
    end_time = clock();
    
    cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    
    if (found) {
        printf("Found vertex %d! using sequential dfs\n", search_value);
    } else {
        printf("Not found.\n");
    }
    
    printf("It takes time: %.6f seconds\n", cpu_time_used);
    
    // Cleanup graph
    for (int i = 0; i < graph->vertices; i++) {
        free(graph->adj_matrix[i]);
    }
    free(graph->adj_matrix);
    free(graph);
    
    return 0;
}
# Parallel Graph Algorithms Implementation

This project implements parallel versions of three fundamental graph algorithms:
- Breadth-First Search (BFS)
- Depth-First Search (DFS)
- Dijkstra's Shortest Path Algorithm

The implementation uses multi-threading with POSIX threads (pthreads) to enhance traversal and shortest path computations in large-scale graphs. The goal is to improve efficiency and scalability while minimizing communication overhead through work stealing and dynamic load balancing.

## Project Structure

```
Parallel_BFS_DFS_Dijkstra_Algorithm/
│
├── bfs/                    # Parallel BFS implementation
│   ├── parallel_bfs.c      # Source code for parallel BFS
│   ├── parallel_bfs        # Compiled executable
│   ├── efficiency.png      # Efficiency graph
│   ├── speedup.png         # Speedup graph
│   ├── output.png          # Output visualization
│   ├── thread_performance.png  # Performance comparison graph
│   ├── thread_performance.dat  # Performance data
│   ├── plot_efficiency.gp  # Gnuplot script for efficiency graph
│   ├── plot_performance.gp # Gnuplot script for performance graph
│   └── plot_speedup.gp     # Gnuplot script for speedup graph
│
├── dfs/                    # Parallel DFS implementation
│   ├── parallel_dfs.c      # Source code for parallel DFS
│   ├── parallel_dfs        # Compiled executable
│   ├── large_graph.txt     # Large graph test data
│   ├── efficiency.png      # Efficiency graph
│   ├── speedup.png         # Speedup graph
│   ├── output.png          # Output visualization
│   ├── thread_performance.png  # Performance comparison graph
│   ├── thread_performance.dat  # Performance data
│   ├── plot_efficiency.gp  # Gnuplot script for efficiency graph
│   ├── plot_performance.gp # Gnuplot script for performance graph
│   └── plot_speedup.gp     # Gnuplot script for speedup graph
│
├── dijkstra/               # Parallel Dijkstra implementation
│   ├── parallel_dijkstra.c # Source code for parallel Dijkstra
│   ├── parallel_dijkstra   # Compiled executable
│   ├── sample_graph.txt    # Sample weighted graph test data
│   ├── graph.txt           # Performance analysis text file
│   ├── efficiency.png      # Efficiency graph
│   ├── speedup.png         # Speedup graph
│   ├── output.png          # Output visualization
│   ├── thread_performance.png  # Performance comparison graph
│   ├── thread_performance.dat  # Performance data
│   ├── plot_efficiency.gp  # Gnuplot script for efficiency graph
│   ├── plot_performance.gp # Gnuplot script for performance graph
│   └── plot_speedup.gp     # Gnuplot script for speedup graph
│
├── report.docx             # Detailed report on implementation and findings
└── README.md               # This file
```

## Requirements

- C compiler (gcc recommended)
- POSIX threads library (pthread)
- gnuplot (for generating performance graphs)

## Compilation Instructions

### Compiling Parallel BFS

```bash
cd bfs
gcc -o parallel_bfs parallel_bfs.c -lpthread -lm
```

### Compiling Parallel DFS

```bash
cd dfs
gcc -o parallel_dfs parallel_dfs.c -lpthread -lm
```

### Compiling Parallel Dijkstra

```bash
cd dijkstra
gcc -o parallel_dijkstra parallel_dijkstra.c -lpthread -lm
```

## Running the Programs

### Running Parallel BFS

```bash
cd bfs
./parallel_bfs
```

The program reads graph data from "../dfs/large_graph.txt" by default.

### Running Parallel DFS

```bash
cd dfs
./parallel_dfs
```

The program reads graph data from "large_graph.txt" by default.

### Running Parallel Dijkstra

```bash
cd dijkstra
./parallel_dijkstra
```

The program reads graph data from "sample_graph.txt" by default. If the file is not found, it generates a random weighted graph.

## Input File Format

### BFS/DFS Input Format

The input file should follow this format:
```
<number_of_vertices> <number_of_edges>
<source_vertex_1> <destination_vertex_1>
<source_vertex_2> <destination_vertex_2>
...
<search_value>
```

Example:
```
6 8
0 1
0 2
1 3
1 4
2 4
3 5
4 5
2 5
5
```

### Dijkstra Input Format

The input file should follow this format:
```
<number_of_vertices> <number_of_edges>
<source_vertex_1> <destination_vertex_1> <weight_1>
<source_vertex_2> <destination_vertex_2> <weight_2>
...
<source_vertex> <target_vertex>
```

Example (sample_graph.txt):
```
6 9
0 1 4
0 2 2
1 2 1
1 3 5
2 3 8
2 4 10
3 4 2
3 5 6
4 5 3
0 5
```

## Generating Performance Graphs

The performance graphs (thread_performance.png, speedup.png, efficiency.png) have already been generated. If you want to regenerate them, run:

```bash
cd <algorithm_directory>  # e.g., cd bfs
gnuplot plot_performance.gp
gnuplot plot_speedup.gp
gnuplot plot_efficiency.gp
```

This will generate PNG images showing execution time, speedup, and efficiency metrics.

## Algorithm Features

### Common Features
- Multi-threaded implementation using pthreads
- Work stealing for dynamic load balancing
- Message queues for inter-thread communication
- Mutex locks for thread synchronization
- Performance analysis with speedup and efficiency metrics

### Specific Features

#### Parallel BFS
- Level-by-level graph exploration using queues
- Concurrent exploration of vertices at the same level
- Automatic work distribution across threads with load balancing

#### Parallel DFS
- Stack-based implementation for depth exploration
- Concurrent exploration of different graph regions
- Dynamic thread assignment for graph regions

#### Parallel Dijkstra
- Priority queue (min-heap) implementation for vertex selection
- Concurrent edge relaxation with synchronization
- Path reconstruction capability
- Adaptive work distribution based on vertex distance

## Performance Analysis

Each implementation generates:
1. Execution time comparison across different thread counts (thread_performance.png)
2. Speedup analysis compared to single-threaded execution (speedup.png)
3. Efficiency metrics to evaluate scaling properties (efficiency.png)

For detailed analysis, refer to:
- The `graph.txt` file in the Dijkstra directory
- The performance graphs in each algorithm's directory
- The full report (report.docx)

## Insights and Findings

### Parallel BFS
- Effective for level-by-level parallelism
- Works well with breadth-oriented graph exploration
- Offers good load balancing for regular graph structures

### Parallel DFS
- Provides depth-oriented parallelism
- Good for search problems and path finding
- May experience workload imbalance in irregular graphs

### Parallel Dijkstra
- Combines priority-based exploration with parallelism
- Thread contention can affect performance with many threads
- Most complex synchronization requirements of the three algorithms

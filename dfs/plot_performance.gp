set terminal png size 800,600
set output 'thread_performance.png'
set title 'Parallel DFS Performance: Threads vs. Execution Time'
set xlabel 'Number of Threads'
set ylabel 'Execution Time (seconds)'
set grid
set style data linespoints
set key top right
plot 'thread_performance.dat' using 1:2 title 'Execution Time' with linespoints lw 2 pt 7

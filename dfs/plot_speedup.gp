set terminal png size 800,600
set output 'speedup.png'
set title 'Parallel DFS Performance: Threads vs. Speedup'
set xlabel 'Number of Threads'
set ylabel 'Speedup'
set grid
set style data linespoints
set key top left
plot 'thread_performance.dat' using 1:3 title 'Speedup' with linespoints lw 2 pt 7,\
     x title 'Linear Speedup' with lines lt 2

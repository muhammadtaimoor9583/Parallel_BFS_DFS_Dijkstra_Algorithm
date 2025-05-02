set terminal png size 800,600
set output 'efficiency.png'
set title 'Parallel Dijkstra Performance: Threads vs. Efficiency'
set xlabel 'Number of Threads'
set ylabel 'Efficiency'
set grid
set style data linespoints
set key top right
set yrange [0:1.1]
plot 'thread_performance.dat' using 1:4 title 'Efficiency' with linespoints lw 2 pt 7,\
     1 title 'Ideal Efficiency' with lines lt 2

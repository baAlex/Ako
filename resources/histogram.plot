

# > gnuplot -e "input='data.txt'" histogram.plot

set term pngcairo size 1000,500 font "Source Sans Pro,12" linewidth 2 rounded

# Style
set tics nomirror
set border 3 back linecolor rgb "#404040"
set grid back linecolor rgb "#CCCCCC" linewidth 1
set format y "%10.0f"

#
set xrange [0:512]

# Output 1
set xlabel "Value"
set ylabel "Frequency"

set output "out.png"
plot input using 1:2 title "Data (literals)" with linespoints,\
        "" using 1:3 title "Instructions (rle/literals lengths)" with linespoints

# Output 2
set xlabel "Value"
set ylabel "Frequency, logarithmic scale"

set output "out-log.png"
set logscale y 2
plot input using 1:2 title "Data (literals)" with linespoints,\
        "" using 1:3 title "Instructions (rle/literals lengths)" with linespoints

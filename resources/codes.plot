

# > gnuplot -e "input='data.csv'" codes.plot

set term pngcairo size 1000,500 font "Source Sans Pro,12" linewidth 2 rounded

# Style
set tics nomirror
set border 3 back linecolor rgb "#404040"
set grid back linecolor rgb "#CCCCCC" linewidth 1
set format y "%10.0f"

#
set datafile separator ","
set xrange [0:255]
set yrange [0:65535]

# Output 1
set xlabel "Code"
set ylabel "Value"

set output "out.png"
plot input using 1:0 title "" with lines

# Output 2
set xlabel "Code"
set ylabel "Value, logarithmic scale"

set output "out-log.png"
set logscale y 2
plot input using 1:0 title "" with lines

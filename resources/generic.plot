

# > gnuplot -e "input='data.csv'" generic.plot

set term pngcairo size 1000,500 font "Source Sans Pro,12" linewidth 2 rounded

# Style
set tics nomirror
set border 3 back linecolor rgb "#404040"
set grid back linecolor rgb "#CCCCCC" linewidth 1
set format y "%10.0f"

#
set datafile separator ","

# Output 1
set output "out.png"
plot input using 1 title "" with linespoints

# Output 2
set output "out-log.png"
set logscale y 2
plot input using 1 title "" with linespoints

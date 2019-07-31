set terminal png
set output "seventh-packet-byte-count.png"
set title "Packet Byte Count vs. Time \n\nTrace Source Path: /NodeList/*/$ns3::Ipv4L3Protocol/Tx"
set xlabel "Time (Seconds)"
set ylabel "Packet Byte Count"

set key outside center below
set datafile missing "-nan"
plot "seventh-packet-byte-count.dat" index 0 title "Packet Byte Count-0" with linespoints, "seventh-packet-byte-count.dat" index 1 title "Packet Byte Count-1" with linespoints

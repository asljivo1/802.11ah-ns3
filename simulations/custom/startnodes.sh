cd ../forwardsocketdata/

# ipv6 socket.io binding requires explicit ip address, sadly
for i in {0..60}; do
	nodejs index.js $((7000+$i)) $((8000+$i)) 2001:6a8:1d80:2031:225:90ff:fe73:bf32 1> /dev/null &
done

#internet access
sudo route del default gw 10.2.47.254 && sudo route add default gw 10.2.47.253
sudo route add -net 10.11.0.0 netmask 255.255.0.0 gw 10.2.47.254
sudo route add -net 10.2.0.0 netmask 255.255.240.0 gw 10.2.47.254

if [ ! -d "/usr/local/src/802.11ah-ns3" ]; then
	# install everything first
	bash /proj/wall2-ilabt-iminds-be/ns3ah/setup/installscript.sh
else
	echo "802.11ah Folder exists, no installation necessary"
fi

cd /usr/local/src/802.11ah-ns3
git pull

cd ns-3
CXXFLAGS="-std=c++0x" ./waf configure --build-profile=optimized --disable-examples --disable-tests
./waf

cd ../simulations

# start slave
mono SimulationBuilder.exe --slave http://ns3ah.ns3ah.wall2-ilabt-iminds-be.wall2.ilabt.iminds.be:12345/SimulationHost/  >> "/proj/wall2-ilabt-iminds-be/ns3ah/logs/$HOSTNAME.slave.log" &
#mono SimulationBuilder.exe --slave http://pi.dragonbyte.info:8086/SimulationHost/ >> "/proj/wall2-ilabt-iminds-be/ns3ah/logs/$HOSTNAME.slave.log" & 

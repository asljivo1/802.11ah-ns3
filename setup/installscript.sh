#Installation
#------------

# If you don't have internet access on the node of the virtual wall
sudo route del default gw 10.2.47.254 && sudo route add default gw 10.2.47.253
sudo route add -net 10.11.0.0 netmask 255.255.0.0 gw 10.2.47.254
sudo route add -net 10.2.0.0 netmask 255.255.240.0 gw 10.2.47.254


# Installation of ns3 dependencies
apt-get update

apt-get -y install gcc g++ python
apt-get -y install gcc g++ python python-dev
apt-get -y install qt4-dev-tools libqt4-dev
apt-get -y install mercurial
apt-get -y install bzr
apt-get -y install cmake libc6-dev libc6-dev-i386 g++-multilib
apt-get -y install gdb valgrind
apt-get -y install gsl-bin libgsl0-dev libgsl0ldbl
apt-get -y install flex bison libfl-dev
apt-get -y install tcpdump
apt-get -y install sqlite sqlite3 libsqlite3-dev
apt-get -y install libxml2 libxml2-dev
apt-get -y install libgtk2.0-0 libgtk2.0-dev
apt-get -y install vtun lxc

# to run the visualizer
apt-get -y install node npm

# to edit & compile the code
# download vscode

# install typescript transpiler which vscode can use
# sudo npm install tsc

# to run the SimulationBuilder.exe slave

apt-get -y install mono-complete

# for easier visualization of core usage & active script list
apt-get -y install htop

cd /usr/local/src

git clone https://github.com/drake7707/802.11ah-ns3.git

cd 802.11ah-ns3/ns-3

CXXFLAGS="-std=c++0x" ./waf configure --build-profile=optimized --disable-examples --disable-tests

./waf



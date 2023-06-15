#!/usr/bin/env sh

sudo apt install g++ python3 gdb cmake automake build-essential libsqlite3-dev libboost-all-dev libssl-dev git python3-setuptools castxml tshark jq

wget https://www.nsnam.org/releases/ns-allinone-3.38.tar.bz2
tar xfj ns-allinone-3.38.tar.bz2
rm ns-allinone-3.38.tar.bz2

./ns-allinone-3.38/ns-3.38/ns3 configure --enable-mpi
./ns-allinone-3.38/ns-3.38/ns3 build


mv eddd ns-allinone-3.38/ns-3.38/scratch/eddd

./ns-allinone-3.38/ns-3.38/ns3 build


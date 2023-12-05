#!/bin/bash

rm host_fifo
rm client_fifo

mkdir build
cd build
cmake -S ../ -B ./
make

rm -rf client*autogen
rm -rf host*autogen


mv client* ../
mv host* ../

cd ../
rm -r build
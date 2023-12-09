#!/bin/bash

rm host_*
rm client_*

mkdir build
cd build
cmake -S ../ -B ./
make

mv client* ../
mv host* ../

cd ../
rm -r build
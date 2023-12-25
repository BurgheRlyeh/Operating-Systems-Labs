#!/bin/bash

mkdir tmp
cd tmp

cmake -S ../ -B ./
make

mv tests ../
cd ../
rm -r tmp

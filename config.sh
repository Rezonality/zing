#!/bin/sh

mkdir build
cd build
if [ "$1" != "" ] ; then
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$1 ../
else
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../
fi
cd ../


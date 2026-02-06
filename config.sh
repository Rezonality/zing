#!/bin/sh

mkdir build
cd build
if [ "$1" != "" ] ; then
cmake -G "Unix Makefiles" -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=$1 -DCMAKE_EXPORT_COMPILE_COMMANDS=YES ../
else
cmake -G "Unix Makefiles" -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Release ../
fi
cd ../


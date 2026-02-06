#!/bin/sh

mkdir build
cd build
if [ "$1" != "" ] ; then
cmake -G "Xcode" -DBUILD_QT=ON -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=$1 ../
else
cmake -G "Xcode" -DBUILD_QT=ON -DBUILD_IMGUI=ON -DCMAKE_BUILD_TYPE=Release ../
fi
cd ../


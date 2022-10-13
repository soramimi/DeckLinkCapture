#!/bin/sh

rm _build1 -fr
rm _build2 -fr
mkdir _build1 2>/dev/null
mkdir _build2 2>/dev/null

# make plugin module

cd _build1
QT_SELECT=5 qmake ../CudaPlugin.pro
make -j4
cd ..

# make example application

cd _build2
QT_SELECT=5 qmake ../exampleapp.pro
make
cd ..

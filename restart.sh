#!/bin/bash

make unmount
make clean
rm data.nufs
make
./cowtool new data.nufs

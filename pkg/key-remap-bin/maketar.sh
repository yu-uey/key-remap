#!/bin/bash

name="./key-remap-bin-0.1.0"

mkdir $name
cp ./key-remap.service $name
cp ../../target/debug/key-remap $name

tar -czvf $name.tar.gz $name

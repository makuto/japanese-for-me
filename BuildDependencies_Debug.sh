#!/bin/sh

echo "Building curl..."
cd Dependencies/curl
./buildconf
curl_dir=$(echo $PWD/local_install)
./configure --prefix=$curl_dir # --enable-debug
make -j4
make install
cd ../../

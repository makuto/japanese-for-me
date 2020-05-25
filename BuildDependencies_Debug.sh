#!/bin/sh

echo "Building curl..."
cd Dependencies/curl
./buildconf
curl_dir=$(echo $PWD/local_install)
./configure --prefix=$curl_dir # --enable-debug
make -j4
make install
cd ../../

echo "Building mecab..."
cd Dependencies/mecab # Root of repo
mkdir build
cd build
output=$(pwd)
../mecab/configure --prefix=$output/local CC=clang CXX=clang++ CXXFLAGS="-g" --with-charset=utf8
make -j4
make install # Must do this before making the dictionary

# Dictionary must be built in directory
cd ../mecab-ipadic
./configure --prefix=$output/local CC=clang CXX=clang++ CXXFLAGS="-g" --with-charset=utf8 --with-mecab-config=$output/local/bin/mecab-config
make
make install

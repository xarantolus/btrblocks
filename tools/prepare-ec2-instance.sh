#!/usr/bin/sh

# This script prepares an EC2 instance with Amazon Linux 2023 for compiling Btrblocks
#
# Add $HOME/bin to your path in addition to this script

set -eu

# Dependencies
sudo yum -y groupinstall "Development Tools"
sudo yum -y install openssl-devel libcurl-devel bzip2-devel postgresql-devel tmux git htop tree perf boost-devel tbb tbb-devel zlib-devel wget gnupg

pip3 install pandas pyarrow pyspark pyyaml

# Add symlinks to newer gcc versions
mkdir $HOME/bin
pushd $HOME/bin
ln -sf /usr/bin/gcc10-g++ g++
ln -sf /usr/bin/gcc10-c++ c++
ln -sf /usr/bin/gcc10-gcc gcc
ln -sf /usr/bin/gcc10-cc cc
ln -sf /usr/bin/gcc10-cpp cpp
popd # $HOME/bin

version='3.23.1'
threads=2

rm -rf "cmake-${version}"
wget "https://github.com/Kitware/CMake/releases/download/v${version}/cmake-${version}.tar.gz"
tar xf "cmake-${version}.tar.gz"
cd "cmake-${version}"
./configure --parallel=${threads}
make -j ${threads}
sudo make install

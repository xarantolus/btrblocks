#!/usr/bin/sh

# This script prepares an EC2 instance with Amazon Linux 2 for compiling Btrblocks
#
# Add $HOME/bin to your path in addition to this script

set -eu

# Dependencies
sudo yum -y groupinstall "Development Tools"
sudo yum -y install openssl-devel libcurl-devel bzip2-devel postgresql-devel tmux git htop tree perf boost-devel cmake tbb tbb-devel zlib-devel

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
./llvm.sh 18 all
ln -s /usr/bin/clang-18 /usr/bin/clang
ln -s /usr/bin/clang++-18 /usr/bin/clang++
ln -s /usr/bin/clangd-18 /usr/bin/clangd
ln -s /usr/bin/clang-format-18 /usr/bin/clang-format
ln -s /usr/bin/clang-tidy-18 /usr/bin/clang-tidy
ln -s /usr/bin/ld.lld-18 /usr/bin/ld.lld
ln -s /usr/bin/lld-18 /usr/bin/lld
ln -s /usr/bin/lldb-18 /usr/bin/lldb
update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100
update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100
update-alternatives --install /usr/bin/ld ld /usr/bin/ld.lld 100

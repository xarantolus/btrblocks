#!/usr/bin/sh

# This script prepares an EC2 instance with Amazon Linux 2 for compiling Btrblocks
#
# Add $HOME/bin to your path in addition to this script

set -eu
# Check if apt is available
if command -v apt > /dev/null; then
    # Use apt for dependencies
    sudo apt update
    sudo apt -y install build-essential libssl-dev libcurl4-openssl-dev libbz2-dev libpq-dev tmux git htop tree linux-tools-common libboost-all-dev cmake libtbb-dev zlib1g-dev lsb-release wget gnupg
else
    # Use yum for dependencies
    sudo yum -y groupinstall "Development Tools"
    sudo yum -y install openssl-devel libcurl-devel bzip2-devel postgresql-devel tmux git htop tree perf boost-devel cmake tbb tbb-devel zlib-devel wget gnupg
fi

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 18 all
sudo ln -s /usr/bin/clang-18 /usr/bin/clang
sudo ln -s /usr/bin/clang++-18 /usr/bin/clang++
sudo ln -s /usr/bin/clangd-18 /usr/bin/clangd
sudo ln -s /usr/bin/clang-format-18 /usr/bin/clang-format
sudo ln -s /usr/bin/clang-tidy-18 /usr/bin/clang-tidy
sudo ln -s /usr/bin/ld.lld-18 /usr/bin/ld.lld
sudo ln -s /usr/bin/lld-18 /usr/bin/lld
sudo ln -s /usr/bin/lldb-18 /usr/bin/lldb
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100
sudo update-alternatives --install /usr/bin/ld ld /usr/bin/ld.lld 100

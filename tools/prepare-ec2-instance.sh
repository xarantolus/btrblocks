#!/usr/bin/sh

# This script prepares an EC2 instance with Amazon Linux 2 for compiling Btrblocks
#
# Add $HOME/bin to your path in addition to this script

set -eu

# Dependencies
sudo yum -y groupinstall "Development Tools"
sudo yum -y install openssl-devel libcurl-devel bzip2-devel postgresql-devel gcc-c++ tmux git htop tree perf boost-devel cmake tbb tbb-devel

#!/bin/bash

#Dependecies:
	sudo apt install cmake -y
	sudo apt install make -y
	sudo apt install pkg-config -y
	cd Dependencies
	. ./Build.sh
	cd ../
	clear

#Building:
	source ./Dependencies/emsdk/emsdk_env.sh
	mkdir build
	cd build
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH
	emcmake cmake -DENABLE_CURL=FALSE -DCMAKE_BUILD_TYPE=Release -DCFLAGS='-pthread' -DCXXFLAGS='-pthread' ..
	emmake make
	make install


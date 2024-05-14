#!/bin/bash

#Autopoint check(Debian only):
if ! dpkg-query -W -f='${Status}' autopoint > /dev/null 2>&1 | grep "ok installed"; then sudo apt install autopoint; fi

#Setting working directory:
mkdir PKGCONFIG
working_dir=$(pwd)
echo $working_dir
#Cloning:

    #Emsdk:
    if [ -d $working_dir/emsdk ]; then
        echo "Emsdk: Found... Continue."
        source ./emsdk/emsdk_env.sh > /dev/null 2>&1

    else
        echo "Emsdk: Not Found... Cloning."
        git clone https://github.com/emscripten-core/emsdk.git > /dev/null 2>&1
        cd emsdk
        echo "Setting up Emsdk..."
        ./emsdk install latest > /dev/null 2>&1
        ./emsdk activate latest > /dev/null 2>&1
        source ./emsdk_env.sh > /dev/null 2>&1
        echo "Emsdk: Installation Complete... Installing Ports."
        echo "Emsdk: 'Note: Building Ports Takes Time, DO NOT INTERUPT!'"
        sleep 6
        embuilder build ALL
        echo "Emsdk: Ports Installed... Done... Continue."
        cd $working_dir

    fi

    #Lzma:
    if [ -d $working_dir/liblzma ]; then
        echo "Lzma: Found... Continue."

    else
        echo "Lzma: Not Found... Cloning."
        git clone https://github.com/kobolabs/liblzma.git > /dev/null 2>&1
        echo "Lzma: Done... Continue."

    fi

    #Pango/Cairo/Freetype/Fribidi/Glib/Harfbuzz/Expat/Ffi/Png/Pixman/Zlib:
    if [ -d $working_dir/pango-cairo-wasm ]; then
        echo "PangoCairoPack: Found... Continue."

    else
        echo "PangoCairoPack: Not Found... Cloning." 
        git clone https://github.com/notsilverhoft/pango-cairo-wasm.git > /dev/null 2>&1
        cd pango-cairo-wasm
        git submodule init > /dev/null 2>&1
        git submodule update > /dev/null 2>&1
        cd $working_dir

    fi



#Building:

    #Lzma:
    echo "Build: Target Is Lzma..."
    sleep 6
    cd liblzma
    sh autogen.sh
    emconfigure ./configure CFLAGS='-pthread' CXXFLAGS='-pthread' --host x86_64-pc-linux-gnu --prefix=$working_dir/liblzma/build
    emmake make
    emmake make install
    cd $working_dir/PKGCONFIG
    mkdir lzma
    cd $working_dir
    cp -r $working_dir/liblzma/build $working_dir/PKGCONFIG/lzma/build
    lzma_dir=$working_dir/PKGCONFIG/lzma/build/lib/pkgconfig

    #Pango/Cairo/Freetype/Fribidi/Glib/Harfbuzz/Expat/Ffi/Png/Pixman/Zlib:
    echo "Build: Target Is PangoCairoPack..."
    sleep 6
    cd pango-cairo-wasm
    sleep 6
    sudo apt install ragel byacc flex autoconf automake lbzip2 gperf gettext autogen libtool meson ninja-build pkg-config cmake magic -y
    export magicdir=$working_dir/pango-cairo-wasm
    echo "Build: 'Note: Please be aware that this collection of libraries takes a while to build, so please be patient!'"
    sleep 6
    bash build.sh
    cd $working_dir/PKGCONFIG
    mkdir pango-cairo-wasm
    cd $working_dir
    cp -r  $working_dir/pango-cairo-wasm/build $working_dir/PKGCONFIG/pango-cairo-wasm/build
    pango_cairo_wasm_dir=$working_dir/PKGCONFIG/pango-cairo-wasm/build/lib/pkgconfig

#Setting PKG_CONFIG_PATH:
    PKG_CONFIG_PATH=$lzma_dir:$pango_cairo_wasm_dir
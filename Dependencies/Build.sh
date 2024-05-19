#!/bin/bash

#Autopoint check(Debian only):
    if ! dpkg-query -W -f='${Status}' autopoint > /dev/null 2>&1 | grep "ok installed"; then sudo apt install autopoint; fi


sudo apt update
sudo apt upgrade automake -y
sudo apt install python3-pip -y
sudo apt install m4
python3 -m pip install --upgrade pip
pip3 install meson==1.4.0

#Autoconf check
    if dpkg-query -W -f='${Status}' autoconf > /dev/null 2>&1 | grep "ok installed"; then sudo apt remove autoconf && wget http://ftp.us.debian.org/debian/pool/main/a/autoconf/autoconf_2.71-3_all.deb && sudo dpkg -i autoconf_2.71-3_all.deb
    
    else
    wget http://ftp.us.debian.org/debian/pool/main/a/autoconf/autoconf_2.71-3_all.deb && sudo dpkg -i autoconf_2.71-3_all.deb
    
    fi
    

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
            sleep 4
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
            echo "PangoCairoPack: Done... Continue."

        fi

    #FFmpeg
        if [ -d $working_dir/FFmpeg ]; then
            echo "FFmpeg: Found... Continue."
            
        else
            echo "FFmpeg: Not Found... Cloning."
            git clone https://github.com/FFmpeg/FFmpeg.git > /dev/null 2>&1
            echo "FFmpeg: Done... Continue."
            
        fi
        
clear
        
        
#Building:

    #Lzma:
        echo "Build: Target Is Lzma..."
        sleep 4
        cd liblzma
        sh autogen.sh
        emconfigure ./configure --host x86_64-pc-linux-gnu \
        --prefix=$working_dir/liblzma/build
        emmake make  CFLAGS='-pthread' CXXFLAGS='-pthread'
        emmake make install
        cd $working_dir/PKGCONFIG
        mkdir lzma
        cd $working_dir
        cp -r $working_dir/liblzma/build $working_dir/PKGCONFIG/lzma/build
        lzma_dir=$working_dir/PKGCONFIG/lzma/build/lib/pkgconfig
        clear

    #Pango/Cairo/Freetype/Fribidi/Glib/Harfbuzz/Expat/Ffi/Png/Pixman/Zlib:
        echo "Build: Target Is PangoCairoPack..."
        sleep 4
        cd pango-cairo-wasm
        sleep 4
        sudo apt install ragel byacc flex autoconf automake lbzip2 gperf gettext autogen libtool meson \
        ninja-build pkg-config cmake magic -y
        export magicdir=$working_dir/pango-cairo-wasm
        echo "Build: 'Note: Please be aware that this collection of libraries takes a while to build, so please be patient!'"
        sleep 4
        bash build.sh
        cd $working_dir/PKGCONFIG
        mkdir pango-cairo-wasm
        cd $working_dir
        cp -r  $working_dir/pango-cairo-wasm/build $working_dir/PKGCONFIG/pango-cairo-wasm/build
        pango_cairo_wasm_dir=$working_dir/PKGCONFIG/pango-cairo-wasm/build/lib/pkgconfig
        source ./emsdk/emsdk_env.sh
        clear

    #FFmpeg
        echo "Build: Target is FFmpeg..."
        cd FFmpeg
        emconfigure ./configure --disable-x86asm --disable-inline-asm --disable-doc --disable-stripping \
        --nm="$working_dir/emsdk/upstream/bin/llvm-nm -g" \
        --ar=emar --cc=emcc --cxx=em++ --objcc=emcc --dep-cc=emcc\
        --prefix=$working_dir/FFmpeg/build --extra-ldflags='-s TOTAL_MEMORY=33554432'
        emmake make CFLAGS='-pthread -sUSE_SDL=2 -s' CXXFLAGS='-pthread -sUSE_SDL=2'
        emmake make install
        cd $working_dir/PKGCONFIG
        mkdir FFmpeg
        cd $working_dir
        cp -r $working_dir/FFmpeg/build $working_dir/PKGCONFIG/FFmpeg/build
        FFmpeg_dir=$working_dir/PKGCONFIG/FFmpeg/build/lib/pkgconfig
        clear
    
#Setting PKG_CONFIG_PATH:
    PKG_CONFIG_PATH=$lzma_dir:$pango_cairo_wasm_dir:$FFmpeg_dir

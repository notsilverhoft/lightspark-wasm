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
    sudo rm autoconf_2.71-3_all.deb
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
            clear

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
            echo "Cloning Submodules... Please wait!"
            git submodule update --init
            clear
            cd $working_dir
            echo "PangoCairoPack: Done... Continue."

        fi

    #FFmpeg
        if [ -d $working_dir/FFmpeg ]; then
            echo "FFmpeg: Found... Continue."
            
        else
            echo "FFmpeg: Not Found... Cloning."
            git clone https://github.com/FFmpeg/FFmpeg.git > /dev/null 2>&1
            clear
            echo "FFmpeg: Done... Continue."

            
        fi

    #SDL2
        if [ -d $working_dir/SDL2-2.30.7 ]; then
            echo "SDL2: Found... Continue."

        else
            echo "SDL2: Not Found... Cloning."
            wget https://github.com/libsdl-org/SDL/releases/download/release-2.30.7/SDL2-2.30.7.zip > /dev/null 2>&1
            unzip SDL2-2.30.7.zip
            sudo rm SDL2-2.30.7.zip
            clear
            echo "SDL2: Done... Continue."

        fi

    #rtmpdump
        if [ -d $working_dir/rtmpdump ]; then
            echo "RTMP: Found... Continue."
        
        else
            echo "RTMP: Not Found... Cloning."
            git clone https://github.com/notsilverhoft/rtmpdump.git > /dev/null 2>&1
            clear
            echo "RTMP: Done... Continue."

        fi
        
    #OpenSSL
        if [ -d $working_dir/openssl ]; then
            echo "RTMP: Found... Continue."

        else
            echo "OpenSSL: Not Found... Cloning."
            git clone https://github.com/openssl/openssl.git > /dev/null 2>&1
            clear
            echo "SDL2: Done... Continue."

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
        export lzma_dir=$working_dir/PKGCONFIG/lzma/build/lib/pkgconfig
        clear
        echo "Removing build directory for Lzma..."
        sleep 4
        sudo rm -r liblzma
        clear

    #Pango/Cairo/Freetype/Fribidi/Glib/Harfbuzz/Expat/Ffi/Png/Pixman/Zlib:
        echo "Build: Target Is PangoCairoPack..."
        sleep 4
        cd pango-cairo-wasm
        sudo dockerd > /dev/null 2>&1
        sudo docker build --progress=plain --no-cache -t 'pangocairowasm' .
        sleep 6
        clear
        echo "Removing pangocairowasm dir..."
        cd $working_dir
        sudo rm -r pango-cairo-wasm
        echo "Setting variables..."
        image=pangocairowasm
        container_id=$(docker create "$image")
        source_path=/magic/build
        destination_path=$working_dir/PKGCONFIG/pango-cairo-wasm/build
        clear
        sleep 2
        echo "Downloading from Docker..."
        sleep 4
        docker cp "$container_id:$source_path" "$destination_path"
        clear
        echo "Cleaning up..."
        docker rm "$container_id"
        yes Y | docker rmi pangociarowasm
        yes Y | docker builder prune
        sleep 2
        clear
        echo "Setting up env variables..."
        cd $working_dir/PKGCONFIG
        mkdir pango-cairo-wasm
        cd $working_dir
        export pango_cairo_wasm_dir=$working_dir/PKGCONFIG/pango-cairo-wasm/build/lib/pkgconfig
        clear
        echo "Removing build directory for PangoCairoPack"
        sleep 4
        sudo rm -r pango-cairo-wasm
        clear

    #FFmpeg
        echo "Build: Target is FFmpeg..."
        cd FFmpeg
        emconfigure ./configure --disable-x86asm --disable-inline-asm --disable-doc --disable-stripping \
        --nm="$working_dir/emsdk/upstream/bin/llvm-nm -g" \
        --ar=emar --cc=emcc --cxx=em++ --objcc=emcc --dep-cc=emcc\
        --prefix=$working_dir/PKGCONFIG/FFmpeg/build --extra-ldflags='-s TOTAL_MEMORY=33554432'
        emmake make CFLAGS='-pthread -sUSE_SDL=2 -s' CXXFLAGS='-pthread -sUSE_SDL=2'
        emmake make install
        cd $working_dir/PKGCONFIG
        mkdir FFmpeg
        cd $working_dir
        export FFmpeg_dir=$working_dir/PKGCONFIG/FFmpeg/build/lib/pkgconfig
        clear
        echo "Removing build directory for FFmpeg..."
        sleep 4
        sudo rm -r FFmpeg
        clear

    #SDL2
        echo "Build: Target is SDL2..."
        cd SDL2-2.30.7
        emconfigure ./configure --host=wasm32-unknown-emscripten --enable-pthreads --disable-assembly --disable-cpuinfo CFLAGS="-sUSE_SDL=0 -O3 -pthread" LDFLAGS="-pthread" --prefix=$working_dir/PKGCONFIG/SDL2/build
        emmake make -j4 
        make install
        cd $working_dir
        export SDL2_dir=$working_dir/PKGCONFIG/SDL2/build/lib/pkgconfig
        clear
        echo "Removing build directory for SDL2..."
        sleep 4
        sudo rm -r SDL2-2.30.7
        clear

    #rtmpdump
        echo "Build: Target is RTMP..."
        cd rtmpdump
        make clean
        emmake make CRYPTO=
        make install DESTDIR=$working_dir/PKGCONFIG/rtmpdump/build
        cd $working_dir
        export RTMP_dir=$working_dir/PKGCONFIG/rtmpdump/build/lib/pkgconfig
        clear
        echo "Removing build directory for SDL2..."
        sleep 4
        sudo rm -r rtmpdump
        clear

    #OpenSSL
        echo "Build: Target is OpenSSL..."
        cd openssl
        emconfigure ./Configure -no-asm -no-apps no-ssl2 no-ssl3 no-comp no-hw no-engine no-deprecated shared no-dso --openssldir=built linux-generic32 -pthread --cross-compile-prefix=/ --prefix=$working_dir/PKGCONFIG/OpenSSL/build
        emmake make 
        sudo make install
        cd $working_dir
        export OpenSSL_dir=$working_dir/PKGCONFIG/OpenSSL/build/lib/pkgconfig
        clear
        echo "Removing build directory for OpenSSL..."
        sleep 4
        sudo rm -r openssl
        clear

    clear
    
#Setting PKG_CONFIG_PATH:
    export PKG_CONFIG_PATH=$lzma_dir:$pango_cairo_wasm_dir:$FFmpeg_dir:$SDL2_dir:$RTMP_dir:$OpenSSL_dir
#Done Message:
    echo "Dependencies: Done... Continue..."

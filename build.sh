#!/bin/bash

#Dependecies:
	sudo apt install cmake -y
	sudo apt install make -y
	sudo apt install pkg-config -y
	cd Dependencies
	. ./Build.sh
	cd ../
	clear

#Header Fixing:
	cd Fixed-Header-Files
	. ./fix.sh
	cd ../
#Building:
	source ./Dependencies/emsdk/emsdk_env.sh
	mkdir build
	cd build
	export PKG_CONFIG_PATH=$PKG_CONFIG_PATH
	emcmake cmake -DENABLE_CURL=FALSE -DEMSCRIPTEN=ON -DCMAKE_BUILD_TYPE=Release -DHAVE_ATOMIC=ON ..
	echo -I$working_dir/../build -I$working_dir/PKGCONFIG/emsdk/upstream/include -I$working_dir/PKGCONFIG/emsdk/upstream/include/__atomic -I$working_dir/PKGCONFIG/emsdk/upstream/emscripten/cache/sysroot/include -I$working_dir/PKGCONFIG/liblzma/build/include -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/pango-1.0 -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/fribidi -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/harfbuzz -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0 -I$working_dir/PKGCONFIG/pango-cairo-wasm/lib/glib-2.0/include -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/cairo -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/freetype2 -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/libpng16 -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/pixman-1 -I$working_dir/PKGCONFIG/SDL2/build/include -I$working_dir/PKGCONFIG/SDL2/build/include/SDL2 -I$working_dir/PKGCONFIG/FFmpeg/build/include -I$working_dir/../src -I$working_dir/../src/scripting -I$working_dir/../src/3rdparty/jxrlib/jxrgluelib -I$working_dir/../src/3rdparty/avmplus/pcre -I$working_dir/../src/3rdparty/imgui -I$working_dir/../src/3rdparty/imgui/backends > $working_dir/../build/src/CMakeFiles/lightspark.dir/includes_CXX.rsp
	emmake make



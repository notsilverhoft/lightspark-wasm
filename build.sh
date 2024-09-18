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
	echo libspark.a -L$working_dir/PKGCONFIG/pango-cairo-wasm/build/lib -lpangocairo-1.0 -lm -lpangoft2-1.0 -lpango-1.0 -lgio-2.0 -lgobject-2.0 -lffi -lgmodule-2.0 -lglib-2.0 -lfribidi -lharfbuzz -lcairo -ldl -lfontconfig -lfreetype -lz -lpng16 -lpixman-1 -L$working_dir/PKGCONFIG/lzma/build/lib -llzma -L$working_dir/PKGCONFIG/FFmpeg/build/lib -lavformat -lavcodec -lswresample -lavutil -lX11 -L$working_dir/PKGCONFIG/rtmpdump/build/lib -lrtmp -L$working_dir/PKGCONFIG/OpenSSL/build/lib -lssl -lcrypto -lSDL2 -lm -L$working_dir/PKGCONFIG/pango-cairo-wasm/build/lib -lpangoft2-1.0 -lpango-1.0 -lgio-2.0 -lgobject-2.0 -lffi -lgmodule-2.0 -lglib-2.0 -lfribidi -lharfbuzz -lcairo -ldl -lfontconfig -lfreetype -lz -lpng16 -lpixman-1 -llzma -lavformat -lavcodec -lswresample -lavutil -lX11 -lrtmp -lssl -lcrypto -lSDL2 > $working_dir/../build/src/CMakeFiles/lightspark.dir/linklibs.rsp
	echo $working_dir/emsdk/upstream/emscripten/em++ -s USE_SDL=2 -s USE_FREETYPE=1 -s USE_ZLIB=1 -s USE_LIBJPEG=1 -s USE_LIBPNG=1 -Wall -Wnon-virtual-dtor -Woverloaded-virtual -Wno-undefined-var-template -pipe -fvisibility=hidden -fvisibility-inlines-hidden -std=c++14 -Wdisabled-optimization -Wextra -Wno-unused-parameter -Wno-invalid-offsetof -O2 -DNDEBUG  -Wl, --allow-undefined -s USE_SDL=2 -s USE_FREETYPE=1 -s USE_ZLIB=1 -s USE_LIBJPEG=1 -s USE_LIBPNG=1 -Wl, --no-entry -s @CMakeFiles/lightspark.dir/objects1.rsp  -o ../x86/Release/bin/lightspark.html @CMakeFiles/lightspark.dir/linklibs.rsp > $working_dir/../build/src/CMakeFiles/lightspark.dir/link.txt
	echo -I$working_dir/../build -I$working_dir/PKGCONFIG/emsdk/upstream/include -I$working_dir/PKGCONFIG/emsdk/upstream/include/__atomic -I$working_dir/PKGCONFIG/emsdk/upstream/emscripten/cache/sysroot/include -I$working_dir/PKGCONFIG/liblzma/build/include -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/pango-1.0 -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/fribidi -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/harfbuzz -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0 -I$working_dir/PKGCONFIG/pango-cairo-wasm/lib/glib-2.0/include -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/cairo -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/freetype2 -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/libpng16 -I$working_dir/PKGCONFIG/pango-cairo-wasm/build/include/pixman-1 -I$working_dir/PKGCONFIG/SDL2/build/include -I$working_dir/PKGCONFIG/SDL2/build/include/SDL2 -I$working_dir/PKGCONFIG/FFmpeg/build/include -I$working_dir/../src -I$working_dir/../src/scripting -I$working_dir/../src/3rdparty/jxrlib/jxrgluelib -I$working_dir/../src/3rdparty/avmplus/pcre -I$working_dir/../src/3rdparty/imgui -I$working_dir/../src/3rdparty/imgui/backends > $working_dir/../build/src/CMakeFiles/lightspark.dir/includes_CXX.rsp
	emmake make



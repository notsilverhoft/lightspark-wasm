#Env:
fix_dir=$(pwd)

#Fixing:
    sudo cp $fix_dir/galloca.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/galloca.h
    sudo cp $fix_dir/gtypes.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/gtypes.h
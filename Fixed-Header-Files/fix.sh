#Env:
fix_dir=$(pwd)

#Fixing:
    sudo cp $fix_dir/galloca.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/galloca.h
    sudo cp $fix_dir/gtypes.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/gtypes.h
    sudo cp $fix_dir/glibconfig.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/lib/glib-2.0/include/glibconfig.h
    sudo cp $fix_dir/glib.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib.h
    sudo cp $fix_dir/garray.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/garray.h
    sudo cp $fix_dir/gasyncqueue.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/gasyncqueue.h
    sudo cp $fix_dir/gthread.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/gthread.h
    sudo cp $fix_dir/gatomic.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/gatomic.h
    sudo cp $fix_dir/glib-typeof.h $working_dir/PKGCONFIG/pango-cairo-wasm/build/include/glib-2.0/glib/glib-typeof.h
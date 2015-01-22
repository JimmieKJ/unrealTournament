#!/bin/bash

####
#NOTE: This file is only expected to run on Linux. Running via Cygwin, etc won't work...

mkdir linuxforhtml5_build
mkdir html5_build
mkdir HTML5

cd linuxforhtml5_build
./../source/runConfigureICU Linux --with-library-bits=64 --with-data-packaging=files
make VERBOSE=1

cd ../html5_build
# emcc & em++ will fail these checks and cause a missing default constructor linker issue so we force the tests to pass
export ac_cv_override_cxx_allocation_ok="yes"
export ac_cv_override_placement_new_ok="yes"
COMMONCOMPILERFLAGS="-DUCONFIG_NO_TRANSLITERATION=1 -DU_USING_ICU_NAMESPACE=0 -DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1 -DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -DU_STATIC_IMPLEMENTATION -DU_OVERRIDE_CXX_ALLOCATION=1"
emconfigure ./../source/configure CFLAGS="$COMMONCOMPILERFLAGS" CXXFLAGS="$COMMONCOMPILERFLAGS -std=c++11" CPPFLAGS="$COMMONCOMPILERFLAGS" --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --disable-tests --build=i386-pc-linux-gnu --with-data-packaging=files --with-cross-build=$PWD/../linuxforhtml5_build
emmake make VERBOSE=1

function build_lib {
    local libbasename="${1##*/}"
    local finallibname="${libbasename%.a}.bc"
    echo Building $1 to $finallibname
    mkdir tmp
    cd tmp
    llvm-ar x ../$f
    for f in *.ao; do
        mv "$f" "${f%.ao}.bc";
    done
    local BC_FILES=
    for f in *.bc; do
        BC_FILES="$BC_FILES $f"
    done
    emcc $BC_FILES -o "../$finallibname"
    cd ..
    rm -rf tmp/*
    rmdir tmp
}

cd ../HTML5
for f in ../html5_build/lib/*.a; do
    build_lib $f
done
for f in ../html5_build/stubdata/*.a; do
    build_lib $f
done

cd ..
rm -rf html5_build/*
rm -rf linuxforhtml5_build/*
rmdir html5_build
rmdir linuxforhtml5_build

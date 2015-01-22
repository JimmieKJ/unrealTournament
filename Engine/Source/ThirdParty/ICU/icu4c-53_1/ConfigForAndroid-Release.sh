SYSROOT=$NDKROOT/platforms/android-19/arch-arm
GCCTOOLCHAIN=$NDKROOT/toolchains/arm-linux-androideabi-4.8/prebuilt/windows-x86_64
GNUSTL=$NDKROOT/sources/cxx-stl/gnu-libstdc++/4.8
COMMONTOOLCHAINFLAGS="-target armv7a-none-linux-androideabi --sysroot=$SYSROOT -gcc-toolchain $GCCTOOLCHAIN"
COMMONCOMPILERFLAGS="-fdiagnostics-format=msvc -fPIC -fno-exceptions -frtti -fno-short-enums -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -fsigned-char"
export CFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -x c"
export CXXFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -x c++ -std=c++11"
export CPPFLAGS="$COMMONTOOLCHAINFLAGS -DUCONFIG_NO_TRANSLITERATION=1 -DPIC -DU_HAVE_NL_LANGINFO_CODESET=0 -DU_TIMEZONE=0 -I$GNUSTL/include -I$GNUSTL/libs/armeabi-v7a/include"
export LDFLAGS="$COMMONTOOLCHAINFLAGS -nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -lgnustl_shared -lc -lgcc -L$GNUSTL/libs/armeabi-v7a -march=armv7-a -Wl,--fix-cortex-a8"
../../Source/configure --prefix=$PWD --build=x86_64-unknown-cygwin --with-cross-build=$PWD/../../Win64/VS2013 --host=armv7a-none-linux-androideabi --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --with-data-packaging=files
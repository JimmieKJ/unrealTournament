#!/bin/bash

<<DOC

Build script for Mac OS X.
==========================

See https://libwebsockets.org for more information about this project.

This subdirectory contains the following:

* libwebsockets
	Pre-built static libraries and include headers for linking into engine code.
* libwebsockets_source
	The source for libWebsockets version 1.7-stable.

In order to keep this directory as pristine as possible, create the temporary build direcory outside the source dir instead of as a subdirectory as suggested in libwebsockets_source/README.build.md, but refer to that document if you need to modify any build options.

This library needs the OpenSSL library already provided for some platforms in ../OpenSSL. When adding new platforms, you may have to add OpenSSL binaries for that platform first.

DOC

if [[ $(uname) = "Darwin" ]]
then
	LIB_SUBDIR=lib/Mac
	INCLUDE_SUBDIR=include/Mac
else
	echo "Unsupported OS:" $(uname)
	exit 1
fi

function realpath()
{
	(cd $1; pwd -P)
}

mkdir -p Build/openssl_include

INSTALL_PREFIX=$(realpath libwebsockets)
SOURCE_PREFIX=$(realpath libwebsockets_src)
OPENSSL_PREFIX=$(realpath ../OpenSSL/1.0.2g/)
OPENSSL_INCLUDE="$OPENSSL_PREFIX/$INCLUDE_SUBDIR/"
OPENSSL_LIBS="$OPENSSL_PREFIX/$LIB_SUBDIR/libssl.a;$OPENSSL_PREFIX/$LIB_SUBDIR/libcrypto.a"
export MACOSX_DEPLOYMENT_TARGET=10.9

mkdir -p "$INSTALL_PREFIX/$LIB_SUBDIR"

cd Build

cmake \
	-DCMAKE_INSTALL_PREFIX:PATH="$INSTALL_PREFIX" \
	-DLWS_INSTALL_LIB_DIR:PATH="$LIB_SUBDIR" \
	-DLWS_OPENSSL_INCLUDE_DIRS:PATH="$OPENSSL_INCLUDE" \
	-DLWS_OPENSSL_LIBRARIES:PATH="$OPENSSL_LIBS" \
	-DLWS_SSL_CLIENT_USE_OS_CA_CERTS:BOOL=ON \
	-DLWS_WITHOUT_TESTAPPS:BOOL=ON \
	-DLWS_WITH_HTTP2:BOOL=ON \
	-DLWS_WITH_SHARED:BOOL=OFF \
	"$SOURCE_PREFIX"

make
make install
cd ..

mkdir -p "$INSTALL_PREFIX/$INCLUDE_SUBDIR"
mv -f "$INSTALL_PREFIX/include/lws_config.h" "$INSTALL_PREFIX/$INCLUDE_SUBDIR/"
# Don't know how to prevent this one
rm -rf "$INSTALL_PREFIX/lib/cmake"
rm -rf "$INSTALL_PREFIX/lib/pkgconfig"

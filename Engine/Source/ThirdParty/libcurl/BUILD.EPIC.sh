#!/bin/bash

set -x

# UE4 build script for the following ThirdParty libraries:

ZLIB_VERSION=v1.2.8
SSL_VERSION=OpenSSL_1_0_1s
CURL_VERSION=curl-7_48_0
WS_VERSION=v1.7.4

# jump down to build_environment() if this is your first time reading this script

# ================================================================================

configure_platform()
{
	# you can force SYSTEM and MACHINE here...
	# e.g. SYSTEM=$ANDROID_ARCH
	SYSTEM=$(uname)
	MACHINE=$(uname -m)

	# in other words, where to output the build
	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Linux)
			if [ $MACHINE == "x86_64" ]; then
				PLATFORM=Linux/x86_64-unknown-linux-gnu
			else
				PLATFORM=Linux/arm-unknown-linux-gnueabihf
			fi
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Darwin)
			PLATFORM=Mac
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			if $UE4_BUILD_WIN32; then
				WINTARGET=32
			else
				WINTARGET=64
			fi
			if $USE_VS_2013; then
				PLATFORM=Win$WINTARGET/VS2013
			else
				PLATFORM=Win$WINTARGET/VS2015
			fi
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		arch-arm)
			PLATFORM=Android/ARMv7
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		arch-x86)
			PLATFORM=Android/x86
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		arch-x86_64)
			PLATFORM=Android/x64
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			PLATFORM=unknown
			;;
	esac
}

# ================================================================================

get_zlib()
{
	mkdir -p zlib
	cd zlib
		git clone https://github.com/madler/zlib.git $ZLIB_VERSION
		cd $ZLIB_VERSION
		git checkout $ZLIB_VERSION -b $ZLIB_VERSION
	cd ../..
}
path_zlib()
{
	cd zlib/$ZLIB_VERSION
	# ----------------------------------------
	CUR_PATH=$(pwd)
	CMAKE_PATH="$CUR_PATH/"
	BUILD_PATH="$CUR_PATH/Intermediate/$PLATFORM"
	ZLIB_ROOT_PATH="$CUR_PATH/INSTALL.$ZLIB_VERSION/$PLATFORM"
	case $SYSTEM in
		Darwin)
			ZLIB_ZLIB=libz.a
			ZLIB_ZLIB_SO=libz.dylib
			;;
		*_NT-*)
			ZLIB_ZLIB=zlibstatic.lib
#			ZLIB_ZLIB_SO=not used
			;;
		*)
			# default to unix centric
			ZLIB_ZLIB=libz.a
			ZLIB_ZLIB_SO=libz.so
			;;
	esac
	# ----------------------------------------
	cd -
}
build_zlib()
{
	path_zlib

	cd zlib/$ZLIB_VERSION
	# ----------------------------------------
	# reset
	rm -f "$CMAKE_PATH"/CMakeCache.txt
	rm -rf "$ZLIB_ROOT_PATH"
	rm -rf "$BUILD_PATH"
	mkdir -p "$BUILD_PATH"
	cd "$BUILD_PATH"
	git reset --hard HEAD

	# ----------------------------------------

	# also "install" - will be used with libssl & libcurl
	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~
			if $UE4_BUILD_WIN32; then
				# WIN32 doesn't use CMake -- see [ win32/Makefile.msc ] for details
				cd "$CUR_PATH"
( cat <<_EOF_
				call $VCVARSALL
				nmake -f win32\\Makefile.msc clean
				nmake -f win32\\Makefile.msc
_EOF_
) > dos_zlib.bat
				./dos_zlib.bat

				# install - with the same filenames CMake does
				mkdir -p "$ZLIB_ROOT_PATH"/{include,lib}
				cp zlib.h zconf.h "$ZLIB_ROOT_PATH/include/."
				cp zlib.lib "$ZLIB_ROOT_PATH/lib/zlibstatic.lib"
				cp zdll.lib "$ZLIB_ROOT_PATH/lib/zlib"
				cp zlib1.dll "$ZLIB_ROOT_PATH/lib/zlib.dll"

			# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~
			else
				"$CMAKE" -G "$CMAKE_GEN" \
					-DCMAKE_INSTALL_PREFIX:PATH="$ZLIB_ROOT_PATH" \
					"$CMAKE_PATH"
				"$CMAKE" --build . --config RelWithDebInfo
				"$CMAKE" --build . --config RelWithDebInfo --target install
			fi
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			"$CMAKE" -G "$CMAKE_GEN" \
				-DCMAKE_INSTALL_PREFIX:PATH="$ZLIB_ROOT_PATH" \
				-DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_MIN_TARGET \
				"$CMAKE_PATH"
			make
			make install
			;;
	esac
	# ----------------------------------------
	cd "$CUR_PATH/../.."
}
# OpenSSL on linux expects zlib compiled with -fPIC
path_zlib_fPIC()
{
	if [ $SYSTEM == 'Linux' ]; then
		cd zlib/$ZLIB_VERSION
		# ----------------------------------------
		CUR_PATH=$(pwd)
		CMAKE_PATH="$CUR_PATH/"
		BUILD_PATH="$CUR_PATH/Intermediate/fPIC/$PLATFORM"
		ZLIB_ROOT_PATH="$CUR_PATH/INSTALL.$ZLIB_VERSION/$PLATFORM"
		ZLIB_ZLIB=libz_fPIC.a
		ZLIB_ZLIB_SO=libz_fPIC.so
		# ----------------------------------------
		cd -
	fi
}
build_zlib_fPIC()
{
	if [ $SYSTEM == 'Linux' ]; then
		path_zlib_fPIC

		cd zlib/$ZLIB_VERSION
		# ----------------------------------------
		rm -f "$CMAKE_PATH"/CMakeCache.txt
#		rm -rf "$ZLIB_ROOT_PATH"
		rm -rf "$BUILD_PATH"
		mkdir -p "$BUILD_PATH"
		cd "$BUILD_PATH"
		# ----------------------------------------
		# change target from libz.* to libz_fPIC.* and enable PIC build
		git checkout -- "$CMAKE_PATH/CMakeLists.txt"
		perl -pi -e 's/(OUTPUT_NAME) z\)/$1 z_fPIC\)/' "$CMAKE_PATH/CMakeLists.txt"
		# ........................................
		"$CMAKE" -G "$CMAKE_GEN" \
			-DCMAKE_INSTALL_PREFIX:PATH="$ZLIB_ROOT_PATH" \
			-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=TRUE \
			"$CMAKE_PATH"
		make
		make install
		# ----------------------------------------
		cd "$CUR_PATH/../.."
	fi
}
deadcode_zlib_graveyard()
{
	# OSX sed also requires backup file postfix/extension
	sed -i 's/\(OUTPUT_NAME\) z)/\1 z_fPIC\)\n\tset \(CMAKE_POSITION_INDEPENDENT_CODE TRUE\)/' "$CMAKE_PATH/CMakeLists.txt"

	make DESTDIR="$BUILD_PATH/INSTALL" install
#	$VISUALSTUDIO zlib.sln /build RelWithDebInfo
}

# ================================================================================

get_openssl()
{
	mkdir -p OpenSSL
	cd OpenSSL
		git clone https://github.com/openssl/openssl.git $SSL_VERSION
		cd $SSL_VERSION
		git checkout $SSL_VERSION -b $SSL_VERSION
	cd ../..
}
path_openssl()
{
	cd OpenSSL/$SSL_VERSION
	# ----------------------------------------
	CUR_PATH=$(pwd)
	CMAKE_PATH="$CUR_PATH/"
	BUILD_PATH="$CUR_PATH/Intermediate/$PLATFORM"
	SSL_ROOT_PATH="$CUR_PATH/INSTALL.$SSL_VERSION/$PLATFORM"
	# ----------------------------------------
	cd -
}
build_openssl()
{
	# WARNING: OpenSSL build will BOMB if $ZLIB_ROOT_PATH (as well as OpenSSL itself)
	#          contains any whitespace -- will revisit this at a later date...


	path_zlib
	path_zlib_fPIC
	path_openssl

	cd OpenSSL/$SSL_VERSION
	# ----------------------------------------
	# NOTE: ./config could have been used to try to "determine the OS" and run ./Configure
	# but, android-x86_64 needs to be added to ./Configure
	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Linux)
			SSL_ARCH=linux-x86_64
#			SSL_ARCH=linux-x86_64-clang
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Darwin)
			SSL_ARCH=darwin64-x86_64-cc
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			if $UE4_BUILD_WIN32; then
				SSL_ARCH=VC-WIN32
#				SSL_ARCH=debug-VC-WIN32
				UPLINK=
				DO_BAT=ms
			else
				SSL_ARCH=VC-WIN64A
#				SSL_ARCH=debug-VC-WIN64A
				UPLINK='_64'
				DO_BAT=win64a
			fi
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		arch-arm)
			SSL_ARCH=android-armv7
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		arch-x86)
			SSL_ARCH=android-x86
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		arch-x86_64)
			SSL_ARCH=android-x86_64
			# ----------------------------------------
			# Add android-x86_64 config
			git checkout -- Configure
			perl -pi -e '/"android"/a \"android-x86_64\",\"gcc:-mandroid -I\\\$(ANDROID_DEV)\/include -B\\\$(ANDROID_DEV)\/lib -O2 -fomit-frame-pointer -fno-exceptions -fdata-sections -ffunction-sections -fno-short-enums -march=atom -fsigned-char -fPIC -Wall -MD -MF /dev/null -x c::-D_REENTRANT::-ldl:SIXTY_FOUR_BIT_LONG RC4_CHUNK DES_INT DES_UNROLL:\${no_asm}:dlfcn:linux-shared:-fPIC:-nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -march=atom:.so.\\\$(SHLIB_MAJOR).\\\$(SHLIB_MINOR)\",' Configure
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#		*IOS*)
#			SSL_ARCH=iphoneos-cross
#			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			;;
	esac


	# ----------------------------------------
	# reset
	rm -rf "$SSL_ROOT_PATH"
	rm -rf "$BUILD_PATH"
	rm -rf inc32 out32 out32dll tmp32 tmp32dll
	mkdir -p "$BUILD_PATH"

	# ----------------------------------------
	echo "Configuring $SSL_VERSION for $SSL_ARCH"
	case $SYSTEM in
		# NOTE: [no-ssl2] option has been deprecated
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			# https://github.com/openssl/openssl/issues/174#issuecomment-78445306
			# - the following would have also worked (which must be done after ./Configure ...):
			# - dos2unix "*/Makefile */*/Makefile */*.h */*/*.h"
			# to future-proof this, doing it with just git (for all files):
			git config --local core.autocrlf false
			git config --local core.eol lf
			git rm --cached -r .
			git reset --hard HEAD
			# this fix is needed when using git perl
			perl -pi -e 's!(dir})(\.\.)!\1/\2!' ms/uplink-x86${UPLINK}.pl

			# hack UI type -- otherwise, it collides with UE4's UI type
			perl -pi -e 's!(typedef struct ui_st)!#define UI OSSL_UI\n\1!' crypto/ossl_typ.h
			
			# WARNING: [threads] & [shared] options are not available on windows
			#          - errors on file generators
			#          - static & shared: are built separately via nt.mak & ntdll.mak
			# WARNING: no double quotes on zlib path - conflicts with C MACROS
			# WARNING: --with-zlib-lib is zlib itself -- whereas other (i.e. unix) builds is path
			# WARNING: no-asm means no need to install NASM -- OpenSSL does not support MASM
			#          https://github.com/openssl/openssl/blob/master/NOTES.WIN#L19-L22
			DOS_ZLIB_ROOT_PATH=$(cygpath -w $ZLIB_ROOT_PATH)
			DOS_SSL_ROOT_PATH=$(cygpath -w $SSL_ROOT_PATH)
			./Configure zlib no-asm \
				--with-zlib-lib=$DOS_ZLIB_ROOT_PATH\\lib\\$ZLIB_ZLIB \
				--with-zlib-include=$DOS_ZLIB_ROOT_PATH\\include \
				--prefix="$DOS_SSL_ROOT_PATH" \
				--openssldir="$DOS_SSL_ROOT_PATH" \
				$SSL_ARCH | tee "$BUILD_PATH/openssl-configure.log"

			if [[ $UE4_BUILD_WIN32 || $USE_VS_2013 ]]; then
				# defaultlib 'MSVCRT' conflicts with use of other libs; use /NODEFAULTLIB:library
				# http://stackoverflow.com/questions/18612072/link-warning-lnk4098-defaultlib-msvcrt-conflicts-with-use-of-other-libs-us
				git checkout -- ms/do_${DO_BAT}.bat
				perl -pi -e 's!(nt.mak)!\1\nperl -pi -e "s|/MT|/MD|" ms\\nt.mak!' ms/do_${DO_BAT}.bat
			fi

( cat <<_EOF_
			:: https://wiki.openssl.org/index.php/Compilation_and_Installation#W64

			call $VCVARSALL
			call ms\\do_${DO_BAT}.bat

			:: ------------------------------------------------------------
			:: STATIC BUILD
			nmake -f ms\\nt.mak clean
			nmake -f ms\\nt.mak

			:: sanity check the ***SSL*** build
			copy "$DOS_ZLIB_ROOT_PATH\\bin\\zlib.dll" out32\\.
			nmake -f ms\\nt.mak test

			:: "install" - will use this with libcurl
			nmake -f ms\\nt.mak install
			copy "$DOS_SSL_ROOT_PATH\\lib\\libeay32.lib" "$DOS_SSL_ROOT_PATH\\lib\\libeay${WINTARGET}_static.lib"
			copy "$DOS_SSL_ROOT_PATH\\lib\\ssleay32.lib" "$DOS_SSL_ROOT_PATH\\lib\\ssleay${WINTARGET}_static.lib"

			:: ------------------------------------------------------------
			:: DYNAMIC BUILD
			nmake -f ms\\ntdll.mak clean
			nmake -f ms\\ntdll.mak

			:: sanity check the ***SSL*** build
			copy "$DOS_ZLIB_ROOT_PATH\\bin\\zlib.dll" out32dll\\.
			nmake -f ms\\ntdll.mak test

			:: "install" - will use this with libcurl
			nmake -f ms\\ntdll.mak install
			copy "$DOS_SSL_ROOT_PATH\\bin\\libeay32.dll" "$DOS_SSL_ROOT_PATH\\lib\\libeay32.dll"
			copy "$DOS_SSL_ROOT_PATH\\bin\\ssleay32.dll" "$DOS_SSL_ROOT_PATH\\lib\\ssleay32.dll"
_EOF_
) > dos_openssl_${VS}_${WINTARGET}.bat
			echo ./dos_openssl_${VS}_${WINTARGET}.bat
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			# hack UI type -- otherwise, it collides with UE4's UI type
			git rm --cached -r .
			git reset --hard HEAD
			perl -pi -e 's!(typedef struct ui_st)!#define UI OSSL_UI\n\1!' crypto/ossl_typ.h

			# ........................................
			./Configure shared threads zlib \
				--with-zlib-lib=$ZLIB_ROOT_PATH/lib \
				--with-zlib-include=$ZLIB_ROOT_PATH/include \
				--prefix="$SSL_ROOT_PATH" \
				--openssldir="$SSL_ROOT_PATH" \
				$SSL_ARCH | tee "$BUILD_PATH/openssl-configure.log"
			if [ $SYSTEM == "Linux" ]; then
				# OpenSSL on linux expects zlib built with -fPIC
				# which we made it as "z_fPIC"
				git checkout -- Makefile
				perl -pi -e 's/ -lz/ -lz_fPIC/' Makefile
			fi
			if [ $SYSTEM == "Darwin" ]; then
				git checkout -- Makefile
				perl -pi -e "s/^CFLAG= -/CFLAG= -mmacosx-version-min=$OSX_MIN_TARGET -/" Makefile
			fi
			# separating builds as it was done before...
			make depend | tee "$BUILD_PATH/openssl-depend.log"
			make clean
			make build_crypto | tee "$BUILD_PATH/openssl-crypto.log"
			make build_ssl | tee "$BUILD_PATH/openssl-lib.log"

			# run SSL tests
			cp $ZLIB_ROOT_PATH/lib/*.so* .
			make test | tee "$BUILD_PATH/openssl-test.log"

			# "install" - will use this with libcurl
			make install | tee "$BUILD_PATH/openssl-install.log"
			;;
	esac
	# ----------------------------------------
	cd "$CUR_PATH/../.."
}
deadcode_openssl_graveyard()
{
	# double check these are indeed 64 bit
	dumpbin "$DOS_SSL_ROOT_PATH\\lib\\libeay32.lib" /headers | findstr machine
	dumpbin "$DOS_SSL_ROOT_PATH\\lib\\ssleay32.lib" /headers | findstr machine
}

# ================================================================================

get_libcurl()
{
	mkdir -p libcurl
	cd libcurl
		git clone https://github.com/curl/curl.git $CURL_VERSION
		cd $CURL_VERSION
		git checkout $CURL_VERSION -b $CURL_VERSION
	cd ../..
}
path_libcurl()
{
	cd libcurl/$CURL_VERSION
	# ----------------------------------------
	CUR_PATH=$(pwd)
	CMAKE_PATH="$CUR_PATH/"
	BUILD_PATH="$CUR_PATH/Intermediate/$PLATFORM"
	CURL_ROOT_PATH="$CUR_PATH/INSTALL.$CURL_VERSION/$PLATFORM"
	# ----------------------------------------
	cd -
}
build_libcurl()
{
	path_zlib
	path_zlib_fPIC
	path_openssl
	path_libcurl

	cd libcurl/$CURL_VERSION
	# ----------------------------------------
	# reset
	rm -f "$CMAKE_PATH"/CMakeCache.txt
	rm -rf "$CURL_ROOT_PATH"
	rm -rf "$BUILD_PATH"
	mkdir -p "$BUILD_PATH"
#	cd "$BUILD_PATH"
	# ----------------------------------------
	echo 'Generating curl makefiles'
	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			rm -rf builds
			# build details from ./winbuild/BUILD.WINDOWS.txt
			./buildconf.bat
			DEPS=$BUILD_PATH/WITH_DEVEL_DEPS
			mkdir -p $DEPS/{bin,include,lib}
			cp -r {$SSL_ROOT_PATH,$ZLIB_ROOT_PATH}/bin/* $DEPS/bin/.
			cp -r {$SSL_ROOT_PATH,$ZLIB_ROOT_PATH}/include/* $DEPS/include/.
			cp -r {$SSL_ROOT_PATH,$ZLIB_ROOT_PATH}/lib/* $DEPS/lib/.
			DOS_DEPS=$(cygpath -w $DEPS)
			cd winbuild

			git checkout -- MakefileBuild.vc
			# our static zlib is named zlibstatic
			perl -pi -e 's/zlib_a/zlibstatic/' MakefileBuild.vc
			# our static libeay32 & ssleay32 are named libeayXX_static & ssleayXX_static
			perl -pi -e "s/libeay32.lib ssleay32.lib gdi32/libeay${WINTARGET}_static.lib ssleay${WINTARGET}_static.lib gdi32/" MakefileBuild.vc

			if $UE4_BUILD_WIN32; then
				MACHINE=x86
			else
				MACHINE=x64
			fi
( cat <<_EOF_
			call $VCVARSALL
			nmake -f Makefile.vc mode=static VC=$VC WITH_DEVEL=$DOS_DEPS WITH_SSL=static WITH_ZLIB=static GEN_PDB=yes MACHINE=$MACHINE
			nmake -f Makefile.vc mode=dll    VC=$VC WITH_DEVEL=$DOS_DEPS WITH_SSL=static WITH_ZLIB=static GEN_PDB=yes MACHINE=$MACHINE
_EOF_
) > dos_libcurl.bat
			./dos_libcurl.bat
			cd -
			# make install
			mkdir -p $CURL_ROOT_PATH
			mv builds/libcurl-*-sspi $CURL_ROOT_PATH/.
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			if [ $SYSTEM == 'Linux' ]; then
				# OpenSSL on linux expects zlib built with -fPIC
				# which we made it as "z_fPIC"
				git checkout -- configure.ac
				perl -pi -e 's/-lz/-lz_fPIC/' configure.ac
			fi
			# build details from ./GIT-INFO
			./buildconf

			if [ $SYSTEM == 'DARWIN' ]; then
				EXTRA_FLAGS="CFLAGS='-mmacosx-version-min=$OSX_MIN_TARGET' LDFLAGS='-mmacosx-version-min=$OSX_MIN_TARGET'"
			fi
			# $PKG_CONFIG_PATH: ensure libs from $ZLIB_ROOT_PATH and $SSL_ROOT_PATH are actually used
			# [ configure ] needs to be able to find zlib's libs during conftest [ config tests ]
			PKG_CONFIG_PATH="$ZLIB_ROOT_PATH"/share/pkgconfig:"$SSL_ROOT_PATH"/lib/pkgconfig:$PKG_CONFIG_PATH \
			LD_LIBRARY_PATH="$ZLIB_ROOT_PATH"/lib \
			./configure \
				--with-zlib=$ZLIB_ROOT_PATH \
				--with-ssl=$SSL_ROOT_PATH \
				--prefix="$CURL_ROOT_PATH" \
				$EXTRA_FLAGS --enable-static --enable-shared \
				--enable-threaded-resolver --enable-hidden-symbols \
				--disable-ftp --disable-file --disable-ldap --disable-ldaps \
				--disable-rtsp --disable-telnet --disable-tftp \
				--disable-dict --disable-pop3 --disable-imap --disable-smtp \
				--disable-gopher --disable-manual --disable-idn | tee "$BUILD_PATH/curl-configure.log"
			env LDFLAGS=-L$SSL_ROOT_PATH/lib make | tee "$BUILD_PATH/curl-make.log"
			make install
			;;
	esac
	# ----------------------------------------
	cd "$CUR_PATH/../.."
}
deadcode_libcurl_graveyard()
{
	# CURL says "cmake build system is poorly maintained"...
	cd "$BUILD_PATH"
	"$CMAKE" -G "$CMAKE_GEN" \
		-DCMAKE_INSTALL_PREFIX:PATH="$CURL_ROOT_PATH" \
		-DOPENSSL_ROOT_DIR:PATH="$SSL_ROOT_PATH" \
		-DOPENSSL_INCLUDE_DIR:PATH="$SSL_ROOT_PATH/include" \
		-DOPENSSL_LIBRARIES:PATH="$SSL_ROOT_PATH/lib" \
		"$CMAKE_PATH"

# OSX bash is old -- doesn't support case-statement fall-through
#		Linux)
#			git checkout -- configure.ac
#			perl -pi -e 's/-lz/-lz_fPIC/' configure.ac
#			;&
#		*)
#			...
#			;;
}

# ================================================================================

get_libwebsockets()
{
	mkdir -p libWebSockets
	cd libWebSockets
		git clone https://github.com/warmcat/libwebsockets.git $WS_VERSION
		cd $WS_VERSION
		git checkout $WS_VERSION -b $WS_VERSION
	cd ../..
}
path_libwebsockets()
{
	cd libWebSockets/$WS_VERSION
	# ----------------------------------------
	CUR_PATH=$(pwd)
	CMAKE_PATH="$CUR_PATH/"
	BUILD_PATH="$CUR_PATH/Intermediate/$PLATFORM"
	WS_ROOT_PATH="$CUR_PATH/INSTALL.$WS_VERSION/$PLATFORM"
	# ----------------------------------------
	cd -
}
build_libwebsockets()
{
	path_zlib
	path_zlib_fPIC
	path_openssl
	path_libwebsockets

	cd libWebSockets/$WS_VERSION
	# ----------------------------------------
	# reset
	rm -f "$CMAKE_PATH"/CMakeCache.txt
	rm -rf "$WS_ROOT_PATH"
	rm -rf "$BUILD_PATH"
	mkdir -p "$BUILD_PATH"
	cd "$BUILD_PATH"

	# ----------------------------------------
	echo 'Generating libwebsockets makefiles'
	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			if $USE_VS_2013; then
				# TODO: deprecate this when visual studio 2013 is no longer supported
				# https://connect.microsoft.com/VisualStudio/feedback/details/809403/error-c3861-snprintf-identifier-not-found-in-visual-studio-2013
				# - when using VS 2013: test code does not have snprintf (nor LWS_HAVE__SNPRINTF) defined
				FILES=(test-fraggle.c test-ping.c test-server.h)
				for f in ${FILES[@]}; do
					git checkout -- $CUR_PATH/test-server/${f}
					perl -pi -e 's/(libwebsockets.h")/\1\n#define snprintf _snprintf/' $CUR_PATH/test-server/${f}
				done
			fi

			# NOTE: windows only: tell cmake to not use bundled zlib
			"$CMAKE" -G "$CMAKE_GEN" \
				-DCMAKE_INSTALL_PREFIX:PATH="$WS_ROOT_PATH" \
				-DOPENSSL_ROOT_DIR:PATH="$SSL_ROOT_PATH" \
				-DLWS_USE_BUNDLED_ZLIB:BOOL=OFF \
				-DLWS_ZLIB_INCLUDE_DIRS:PATH="$ZLIB_ROOT_PATH"/include \
				-DLWS_ZLIB_LIBRARIES:PATH="$ZLIB_ROOT_PATH"/lib/$ZLIB_ZLIB \
				"$CMAKE_PATH"
			"$CMAKE" --build . --config RelWithDebInfo
			"$CMAKE" --build . --config RelWithDebInfo --target install
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			# NOTE: https://github.com/warmcat/libwebsockets/blob/master/README.build.md
			#       unix only: need: -DCMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE
			#
			# note: shared libs are for test programs
			#
			# $PKG_CONFIG_PATH: ensure libs from $ZLIB_ROOT_PATH and $SSL_ROOT_PATH are actually used
			#
			# OSX needs OPENSSL_ROOT_DIR...
			PKG_CONFIG_PATH="$SSL_ROOT_PATH"/lib/pkgconfig:$PKG_CONFIG_PATH \
			"$CMAKE" -G "$CMAKE_GEN" \
				-DCMAKE_INSTALL_PREFIX:PATH="$WS_ROOT_PATH" \
				-DCMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE:PATH="$SSL_ROOT_PATH" \
				-DOPENSSL_ROOT_DIR:PATH="$SSL_ROOT_PATH" \
				-DCMAKE_OSX_DEPLOYMENT_TARGET=$OSX_MIN_TARGET \
				-DLWS_ZLIB_INCLUDE_DIRS:PATH="$ZLIB_ROOT_PATH"/include \
				-DLWS_ZLIB_LIBRARIES:PATH="$ZLIB_ROOT_PATH"/lib/$ZLIB_ZLIB_SO \
				"$CMAKE_PATH"
			make
			make install
			;;
	esac
	# ----------------------------------------
	cd "$CUR_PATH/../.."
}
deadcode_libwebsockets_graveyard()
{
			# following disabled test programs...
			"$CMAKE" -G "$CMAKE_GEN" \
				-DCMAKE_INSTALL_PREFIX:PATH="$WS_ROOT_PATH" \
				-DCMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE:PATH="$SSL_ROOT_PATH" \
				-DLWS_OPENSSL_INCLUDE_DIRS:PATH="$SSL_ROOT_PATH"/include \
				-DLWS_OPENSSL_LIBRARIES:PATH="$SSL_ROOT_PATH"/lib/ssl.so \
				-DLWS_WITHOUT_TEST_CLIENT:BOOL=ON \
				-DLWS_WITHOUT_TESTAPPS:BOOL=ON \
				-DLWS_WITHOUT_TEST_SERVER:BOOL=ON \
				-DLWS_WITHOUT_TEST_SERVER_EXTPOLL:BOOL=ON \
				-DLWS_ZLIB_INCLUDE_DIRS:PATH="$ZLIB_ROOT_PATH"/include \
				-DLWS_ZLIB_LIBRARIES:PATH="$ZLIB_ROOT_PATH"/lib/$ZLIB_ZLIB_SO \
				"$CMAKE_PATH"
			env LDFLAGS="-L\"$SSL_ROOT_PATH\"/lib -lssl -lcrypto" make VERBOSE=1
}

# ================================================================================

reset_hard()
{
	# ........................................
	cd zlib/$ZLIB_VERSION
		git reset --hard HEAD
		git clean -fd
	cd ../..
	# ........................................
	cd OpenSSL/$SSL_VERSION
		git reset --hard HEAD
		git clean -fd
	cd ../..
	# ........................................
	cd libcurl/$CURL_VERSION
		git reset --hard HEAD
		git clean -fd
	cd ../..
	# ........................................
	cd libWebSockets/$WS_VERSION
		git reset --hard HEAD
		git clean -fd
	cd ../..
	# ........................................
}

build_environment()
{
	# default to unix centric
	CMAKE_GEN='Unix Makefiles'
	CMAKE=cmake

	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Darwin)
			# remember to:
			# brew install autoconf automake libtool cmake git

			# WARNING: UE4 supports 10.9 as the minimum:
			#
			# https://developers.apple.com/downloads/
			# get: xcode 6.4
			#
			# follow these instructions
			# http://blog.felix-schwarz.org/post/141482111524/how-to-use-the-os-x-109-sdk-with-xcode-73
			# - however, can change [ MinimumSDKVersion to 10.9 ] instead of deleting those lines...
			#
			# for future reference
			# http://stackoverflow.com/questions/10335747/how-to-download-xcode-4-5-6-7-and-get-the-dmg-file
			# https://github.com/devernay/xcodelegacy/blob/master/XcodeLegacy.sh

			OSX_MIN_TARGET=10.9
			export MACOSX_DEPLOYMENT_TARGET=$OSX_MIN_TARGET
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			# this script was tested on GIT BASH
			# MS VisualStudio and CMAKE are also required

			if $USE_VS_2013; then
				VC=12
				VS=2013
				VSCT=$VS120COMNTOOLS
				X64=x86_amd64
#				VISUALSTUDIO="$VS120COMNTOOLS..\\IDE\\WDExpress.exe"
			else
				VC=14
				VS=2015
				VSCT=$VS140COMNTOOLS
				X64=amd64
#				VISUALSTUDIO="$VS140COMNTOOLS..\\IDE\\devenv.exe"
			fi
			if $UE4_BUILD_WIN32; then
				MACHINE=x86
			else
				MACHINE=$X64
			fi
			CMAKE_GEN="Visual Studio $VC $VS Win$WINTARGET"
			VCVARSALL="\"$VSCT..\\..\\VC\\vcvarsall.bat\" $MACHINE"

			CMAKE="C:\\Program Files (x86)\\CMake\\bin\\cmake.exe"
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			;;
	esac
}

# ================================================================================
# MAIN
# ================================================================================

# --------------------
# reset
USE_VS_2013=false
UE4_BUILD_WIN32=false

# --------------------
# fetch
get_zlib
get_openssl
get_libcurl
get_libwebsockets
	#reset_hard

# --------------------
# build
configure_platform
build_environment

build_zlib
build_zlib_fPIC
build_openssl
build_libcurl
build_libwebsockets

if [[ $SYSTEM == *'_NT-'* ]]; then
	# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	# as of Unreal Engine 4.11 -- Visual Studio 2013 is still supported

#	echo building with Visual Studio 2013
	USE_VS_2013=true
	configure_platform
	build_environment

	build_zlib
	build_openssl
	build_libcurl
	build_libwebsockets
	USE_VS_2013=false

	# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	echo building Win32 - VS2015
	UE4_BUILD_WIN32=true
	configure_platform
	build_environment

	build_zlib
	build_openssl
	build_libcurl
	build_libwebsockets

	# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	echo building Win32 - VS2013
	USE_VS_2013=true
	configure_platform
	build_environment

	build_zlib
	build_openssl
	build_libcurl
	build_libwebsockets

	# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	USE_VS_2013=false
	UE4_BUILD_WIN32=false
fi


# --------------------------------------------------------------------------------
# handy commands to know:
# cmake -LAH .
# ./configure --help
# git show-ref --head $TAG


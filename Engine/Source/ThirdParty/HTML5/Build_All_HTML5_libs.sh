#!/bin/sh

# build all ThirdParty libs for HTML5
# from the simplest to build to the most complex

cd ../zlib/zlib-1.2.5/Src/HTML5
	./build_html5.sh
cd -
cd ../libPNG/libPNG-1.5.2/projects/HTML5
	./build_html5.sh
cd -
cd ../FreeType2/FreeType2-2.6/Builds/html5
	./build_html5.sh
cd -
cd ../Ogg/libogg-1.2.2/build/HTML5
	./build_html5.sh
cd -
cd ../Vorbis/libvorbis-1.3.2/build/HTML5
	./build_html5.sh
cd -
cd ../ICU/icu4c-53_1
	./BuildForHTML5.sh
cd -
cd ../HarfBuzz/harfbuzz-1.2.4/BuildForUE/HTML5
	./BuildForHTML5.sh
cd -

# NOTE: PhysX needs to be built from the Dev-Physics stream
#       there, you'll find tools/libs, source codes, and docs
# the PhysX HTML5 build script can be found there...


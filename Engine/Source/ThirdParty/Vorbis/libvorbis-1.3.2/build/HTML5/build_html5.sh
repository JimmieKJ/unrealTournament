#!/bin/sh

cd ../../../../HTML5/
	. ./Build_All_HTML5_libs.rc
cd -


cd ../../lib

proj1=libvorbis
proj2=libvorbisfile
makefile=../build/HTML5/Makefile.HTML5

make clean   OPTIMIZATION=-O3 LIB1=${proj1}_O3.bc LIB2=${proj2}_O3.bc -f ${makefile}
make install OPTIMIZATION=-O3 LIB1=${proj1}_O3.bc LIB2=${proj2}_O3.bc -f ${makefile}

make clean   OPTIMIZATION=-O2 LIB1=${proj1}_O2.bc LIB2=${proj2}_O2.bc -f ${makefile}
make install OPTIMIZATION=-O2 LIB1=${proj1}_O2.bc LIB2=${proj2}_O2.bc -f ${makefile}

make clean   OPTIMIZATION=-Oz LIB1=${proj1}_Oz.bc LIB2=${proj2}_Oz.bc -f ${makefile}
make install OPTIMIZATION=-Oz LIB1=${proj1}_Oz.bc LIB2=${proj2}_Oz.bc -f ${makefile}

make clean   OPTIMIZATION=-O0 LIB1=${proj1}.bc LIB2=${proj2}.bc -f ${makefile}
make install OPTIMIZATION=-O0 LIB1=${proj1}.bc LIB2=${proj2}.bc -f ${makefile}

ls -l ../lib/HTML5


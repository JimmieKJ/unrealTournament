cscript /nologo FindToolchainPaths.vbs > Makefile.ToRun
copy Makefile.ToRun Makefile.ToRun.PIC

type Makefile.Epic >> Makefile.ToRun
type Makefile.Epic.PIC >> Makefile.ToRun.PIC

cd ..\..\src
rem nmake -f ..\Builds\Linux\Makefile.ToRun
nmake -f..\Builds\Linux\Makefile.ToRun clean
nmake -f ..\Builds\Linux\Makefile.ToRun install

nmake -f..\Builds\Linux\Makefile.ToRun.PIC clean
nmake -f ..\Builds\Linux\Makefile.ToRun.PIC install

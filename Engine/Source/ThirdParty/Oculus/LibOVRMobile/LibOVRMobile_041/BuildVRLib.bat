@pushd VRLib

@call ndk-build
@call ant -quiet debug

@echo copying files...

@mkdir ..\lib\armv7
@mkdir ..\lib\java

@copy obj\local\armeabi-v7a\liboculus.a ..\lib\armv7\liboculus.a
@copy jni\3rdParty\libjpeg.a ..\lib\armv7\libjpeg.a
@copy bin\classes.jar ..\lib\java\vrlib.jar

@popd

@call InstallVRLib.bat

@pause

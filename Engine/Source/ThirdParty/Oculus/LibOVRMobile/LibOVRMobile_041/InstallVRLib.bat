@pushd VRLib

@echo copying files...

@mkdir ..\..\..\..\..\..\Build\Android\Java\res\raw

@copy obj\local\armeabi-v7a\liboculus.a ..\lib\armv7\liboculus.a
@copy jni\3rdParty\libjpeg.a ..\lib\armv7\libjpeg.a
@copy ..\lib\java\vrlib.jar ..\..\..\..\..\..\Build\Android\Java\libs\vrlib.jar
@xcopy res\raw\. ..\..\..\..\..\..\Build\Android\Java\res\raw /s /y

@popd

@pause

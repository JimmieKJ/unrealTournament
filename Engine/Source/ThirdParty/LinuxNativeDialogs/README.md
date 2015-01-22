# you can just: bash README.md
# from its location to execute commands below

cd UELinuxNativeDialogs
cd build
cmake ..
make
mv lib{qt,gtk}*.so ../lib/Linux/x86_64-unknown-linux-gnu/
cp ../lib/Linux/x86_64-unknown-linux-gnu/*.so ../../../../../../Engine/Binaries/Linux/
cd ../../../../../../Engine/Binaries/Linux/

if [ -f libqt4dialog.so ]; then
  ln -s libqt4dialog.so libLND.so
fi

if [ -f libqt5dialog.so ]; then
  ln -s libqt5dialog.so libLND.so
fi

if [ -f libgtk2dialog.so ]; then
  ln -s libgtk2dialog.so libLND.so
fi

if [ -f libgtk3dialog.so ]; then
  ln -s libgtk3dialog.so libLND.so
fi

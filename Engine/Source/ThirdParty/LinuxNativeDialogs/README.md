UELinuxNativeDialogs
==================

Linux file dialog implementations dedicated for Unreal Engine. Purpose of this project is to provide a simple C API allowing to open linux file and font dialogs utilizing either of four mainstream GUI toolkits:
* Qt4
* Qt5
* Gtk+2
* Gtk+3

## Building

```
# Ubuntu
sudo apt-get install libgtk2.0-dev libgtk-3-dev libqt4-dev qtbase5-dev
mkdir build
cd build
cmake ..
make
```

The shared libraries can be dlopened and `UNativeDialogs.h` should be used to utilize them. Alternatively a normal link could be performed and required implementation chosen with `LD_LIBRARY_PATH`.

## License

The MIT License (MIT)

Copyright (c) 2014 Code Charm Ltd

If you require any special license please contact RushPL on #ue4linux @ irc.freenode.net or damian@codecharm.co.uk

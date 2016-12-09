#pragma once

namespace ansel
{
#if defined(ANSEL_SDK_DELAYLOAD)
    enum DelayLoadStatus
    {
        kDelayLoadOk = 0,
        kDelayLoadNoDllFound = -1,
        kDelayLoadIncompatibleVersion = -2,
    };
    // In order to use delay loading instead of regular linking to a dynamic library
    // define ANSEL_SDK_DELAYLOAD and use 'loadAnsel' function with an optional path to
    // the library (otherwise it's going to use the usual LoadLibrary semantic)
    // loadAnsel returns 0 in case it succeeds
    //           returns -1 in case it can't load the library (can't find it or architecture is not compatible)
    //           returns -2 in case it can't resolve all of the functions used at runtime (incompatible version or name clash)
    DelayLoadStatus loadLibraryW(const wchar_t* path = nullptr);
    DelayLoadStatus loadLibraryA(const char* path = nullptr);
#endif
}
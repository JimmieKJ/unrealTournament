/************************************************************************************

Filename    :   OVR_String_FormatUtil.cpp
Content     :   String format functions.
Created     :   February 27, 2013
Notes       : 

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_String.h"
#include "OVR_Log.h"

namespace OVR {

void StringBuffer::AppendFormat(const char* format, ...)
{       
    va_list argList;

    va_start(argList, format);
    UPInt size = OVR_vscprintf(format, argList);
    va_end(argList);

    char* buffer = (char*) OVR_ALLOC(sizeof(char) * (size+1));

    va_start(argList, format);
    UPInt result = OVR_vsprintf(buffer, size+1, format, argList);
    OVR_UNUSED1(result);
    va_end(argList);
    OVR_ASSERT_LOG(result == size, ("Error in OVR_vsprintf"));

    AppendString(buffer);

    OVR_FREE(buffer);
}

} // OVR

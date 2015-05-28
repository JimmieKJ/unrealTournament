/************************************************************************************

Filename    :   Console.cpp
Content     :   Allows adb to send commands to an application.
Created     :   11/21/2014
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

namespace OVR {

typedef void (*consoleFn_t)( void * appPtr, const char * cmd );

void InitConsole();
void ShutdownConsole();

// add a pointer to a function that can be executed from the console
void RegisterConsoleFunction( const char * name, consoleFn_t function );
void UnRegisterConsoleFunctions();

extern void DebugPrint( void * appPtr, const char * cmd );

}	// namespace Ovr

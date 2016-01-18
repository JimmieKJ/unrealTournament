/************************************************************************************

Filename    :   Log.h
Content     :   Macros and helpers for Android logging.
Created     :   4/15/2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( VRLib_Log_h )
#define VRLib_Log_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Std.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <time.h>
#include <stdint.h>

#if defined( OVR_OS_WIN32 )		// allow this file to be included in PC projects

// stub common functions for non-Android platforms
// TODO: Review latest Desktop LibOVRKernel for new cross-platform variants of these
// utilities.

// Log with an explicit tag
void LogWithTag( const int prio, const char * tag, const char * fmt, ... );

// Strips the directory and extension from fileTag to give a concise log tag
void LogWithFileTag( const int prio, const char * fileTag, const char * fmt, ... );

#define LOG( ... ) 	LogWithFileTag( 0, __FILE__, __VA_ARGS__ );
#define WARN( ... ) LogWithFileTag( 0, __FILE__, __VA_ARGS__ );
#define FAIL( ... ) LogWithFileTag( 0, __FILE__, __VA_ARGS__ );
#define LOG_WITH_TAG( __tag__, ... ) LogWithTag( 0, __FILE__, __VA_ARGS__ );
#define ASSERT_WITH_TAG( __expr__, __tag__ )

#elif defined( OVR_OS_ANDROID )

#include <android/log.h>
#include <jni.h>

// Log with an explicit tag
void LogWithTag( const int prio, const char * tag, const char * fmt, ... );

// Strips the directory and extension from fileTag to give a concise log tag
void LogWithFileTag( const int prio, const char * fileTag, const char * fmt, ... );

// Our standard logging (and far too much of our debugging) involves writing
// to the system log for viewing with logcat.  Previously we defined separate
// LOG() macros in each file to give them file-specific tags for filtering;
// now we use this helper function to massage the __FILE__ macro into just a
// file base -- jni/App.cpp becomes the tag "App".
#define LOG( ... ) LogWithFileTag( ANDROID_LOG_INFO, __FILE__, __VA_ARGS__ )
#define WARN( ... ) LogWithFileTag( ANDROID_LOG_WARN, __FILE__, __VA_ARGS__ )
#define FAIL( ... ) {LogWithFileTag( ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__ );abort();}

#define LOG_WITH_TAG( __tag__, ...) ( (void)LogWithTag( ANDROID_LOG_INFO, __tag__, __VA_ARGS__) )
#define WARN_WITH_TAG( __tag__, ...) ( (void)LogWithTag( ANDROID_LOG_WARN, __tag__, __VA_ARGS__) )
#define FAIL_WITH_TAG( __tag__, ... ) { (void)LogWithTag( ANDROID_LOG_ERROR, __tag__, __VA_ARGS__); abort(); }


// LOG (usually defined on a per-file basis to write to a specific tag) is for logging that can be checked in 
// enabled and generally only prints once or infrequently.
// SPAM is for logging you want to see every frame but should never be checked in
#if defined( OVR_BUILD_DEBUG )
// you should always comment this out before checkin
//#define ALLOW_LOG_SPAM
#endif

#if defined( ALLOW_LOG_SPAM )
#define SPAM(...) LogWithTag( ANDROID_LOG_VERBOSE, "Spam", __VA_ARGS__ )
#else
#define SPAM(...) { }
#endif

// TODO: we need a define for internal builds that will compile in assertion messages but not debug breaks
// and we need a define for external builds that will do nothing when an assert is hit.
#if !defined( OVR_BUILD_DEBUG )
#define ASSERT_WITH_TAG( __expr__, __tag__ ) { if ( !( __expr__ ) ) { WARN_WITH_TAG( __tag__, "ASSERTION FAILED: %s", #__expr__ ); } }
#else
#define ASSERT_WITH_TAG( __expr__, __tag__ ) { if ( !( __expr__ ) ) { WARN_WITH_TAG( __tag__, "ASSERTION FAILED: %s", #__expr__ ); OVR_DEBUG_BREAK; } }
#endif

#else
#error "unknown platform"
#endif	

// Declaring a variable with this class will report the time elapsed when it
// goes out of scope.
class LogCpuTime
{
public:

	LogCpuTime( const char * fileName, const char * fmt, ... ) :
		FileName( fileName )
	{
		va_list ap;
		va_start( ap, fmt );
#if defined( OVR_MSVC_SAFESTRING )
		vsnprintf_s( Label, sizeof( Label ), _TRUNCATE, fmt, ap );
#else
		vsnprintf( Label, sizeof( Label ), fmt, ap );
#endif
		va_end( ap );
		StartTimeNanoSec = GetNanoSeconds();
	}
	~LogCpuTime()
	{
		const double endTimeNanoSec = GetNanoSeconds();
		LOG( "%s took %6.4f seconds", Label, ( endTimeNanoSec - StartTimeNanoSec ) * 1e-9 );
	}

private:
	const char *	FileName;
	char			Label[1024];
	double			StartTimeNanoSec;

	static double GetNanoSeconds()
	{
#if defined( OVR_OS_ANDROID )
		struct timespec now;
		clock_gettime( CLOCK_MONOTONIC, &now );
		return (double)now.tv_sec * 1e9 + now.tv_nsec;
#else
		// TODO: Review OVR::Timer::GetSeconds() implementation
		OVR_ASSERT( 0 );
		return 0.0;
#endif
	}
};

#define LOGCPUTIME( ... ) const LogCpuTime logCpuTimeObject( __FILE__, __VA_ARGS__ )

// Call LogGpuTime::Begin() and LogGpuTime::End() to log the GPU rendering time between begin and end.
// Note that begin-end blocks cannot overlap.
// This seems to cause some stability problems, so don't do it automatically.
template< int NumTimers, int NumFrames = 10 >
class LogGpuTime
{
public:
					LogGpuTime();
					~LogGpuTime();

	bool			IsEnabled();
	void			Begin( int index );
	void			End( int index );
	void			PrintTime( int index, const char * label ) const;
	double			GetTime( int index ) const;
	double			GetTotalTime() const;

private:
	bool			UseTimerQuery;
	bool			UseQueryCounter;
	uint32_t		TimerQuery[NumTimers];
	int64_t			BeginTimestamp[NumTimers];
	int32_t			DisjointOccurred[NumTimers];
	int32_t			TimeResultIndex[NumTimers];
	double			TimeResultMilliseconds[NumTimers][NumFrames];
	int				LastIndex;
};

// Allow GPU Timer Queries - NOTE: GPU Timer queries
// can cause instability on current Adreno driver.
void SetAllowGpuTimerQueries( int enable );

// For CPU performance testing ONLY!
void SetThreadAffinityMask( int tid, int mask );
int GetThreadAffinityMask( int tid );

#endif // VRLib_Log_h

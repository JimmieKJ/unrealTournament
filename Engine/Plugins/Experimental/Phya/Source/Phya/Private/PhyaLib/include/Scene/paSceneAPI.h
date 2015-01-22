//
//  paSceneAPI.h
//
//  Top level scene functions, with audio io and thread wrappers.
//


#ifndef paSceneAPI_h
#define paSceneAPI_h

#include "Signal/paBlock.hpp"

class paRes;


PHYA_API int              paSetnFramesPerSecond(int n);		// Global sample rate.
PHYA_API int              paSetMaxTimeCost(int c);			// Computation time cost can be assigned to resonators and other objects when active.

#if USING_AIO

PHYA_API int              paSetnStreamBufFrames(int n);		// Wrapper for mono output, mainly for testing convenience.
PHYA_API int              paSetnDeviceBufFrames(int n);
PHYA_API int              paOpenStream();					// Open stream for paGenerate. 

#endif // USING_AIO

PHYA_API int              paSetLimiter(paFloat attackTime, paFloat holdTime, paFloat decayTime);		// Pass limiter time parameters. Zeros remove limiter.
PHYA_API int              paSetOutputCallback( void (*cb)(paFloat* output) );				// Callback available when using paGenerate, to route a mono signal (useful when using a thread).
PHYA_API int              paSetMultipleOutputCallback( void (*cb)(paRes* res, paFloat* output) );	// Callback routing res output to 3D audio, without limiting.
PHYA_API int              paInit();							// Must use if using paGenerate or paStartThread.
PHYA_API paFloat*		  paGenerate();						// Calculate and send one block, with limiter and callback send.

PHYA_API int              paTickInit();						// Lower level control, not normally used.
PHYA_API paBlock*	      paTick();							// Calculate one block of output from the scene.

PHYA_API int              paStartThread();					// Run paGenerate in another thread to avoid audio blocking.
PHYA_API int              paStopThread();
PHYA_API void             paLock();							// Use when changing collision state or passing data if GenerateThread used.
PHYA_API void             paUnlock();


#endif


// Terminology:
//
// Sample			The value of a single audio channel at a given time.
// Frame			A collection of samples from different channels at the same time.
// Tick				The time interval at which samples and frames are updated.
// Block			The basic unit of 1-channel frames used to buffer variables
//					between signal processes. The structure is kept simple to keep
//					efficiency high.
// Device			The audio hardware which is sending (or receiving) external audio.
// Device Buffer	The final buffer into which the user passes samples before appearing
//					on the soundcard. Note that this might not be the actual final
//					physical buffer. Eg in directsound the 'Device Buffer' here would be
//					a secondary buffer.
// Stream buffer	A software buffer which feeds the device buffer.
//					Making this bigger than the Block size can reduce overhead costs.


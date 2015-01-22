//
// paAudio.hpp
//
// paAudio contains global varibles and definitions which are used
// by the the signal processing and audio I/O systems.
//


#if !defined(__paAudio_hpp)
#define __paAudio_hpp


//#define NORES
//#define CAN
#define DISK
#define AUDIOTHREAD
//#define SLEEP
#define NOHELP
#define LIMITER
//#define DISPLAY_CONTACTS


// Experimental options:
//
//#define MANIFOLDCOLLISION		// single persistent audio contact driven by manifold.
//
// Surfaces:
//
//#define IMPACTSAMPLE
//#define WAVSURF	
//#define PLASTIC
//#define WATER
//#define LEAVES
//#define FREEPARTICLESURF
//#define GRAVEL1
//#define GRAVEL2
//#define FOIL
//#define REEDS




#include "System/paConfig.h"
#include "System/paFloat.h"

namespace paScene
{
	extern PHYA_API unsigned long	nFramesPerSecond;
};


#endif

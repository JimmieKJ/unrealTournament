//
// paOutputStream.cpp
//
#include "PhyaPluginPrivatePCH.h"

#include "System/paConfig.h"



#include "Signal/paBlock.hpp"
#include "Scene/paFlags.hpp"

#if USING_AIO
#include "AIO.hpp"

namespace paScene {
	CAIOoutStream* outputStream;
}

static CAIOoutStreamConfig _outputStreamConfig;
#endif // USING_AIO

void (*_userGenerateCallback)();


PHYA_API
int
paSetnFramesPerSecond(int n)
{
	paScene::nFramesPerSecond = n;
	return 0;
}

#if USING_AIO

PHYA_API
int
paSetnStreamBufFrames(int n)
{
	if (n>100 && n<10000){
		_outputStreamConfig.m_nStreamBufferFrames = n;
		return 0;
	}
	else 
		return -1;
}

PHYA_API
int
paSetnDeviceBufFrames(int n)
{
		_outputStreamConfig.m_nDeviceBufferFrames = n;
		return 0;
}

PHYA_API
int
paOpenStream()
{
	if (paScene::outputStreamIsOpen) return -1;

	_outputStreamConfig.m_wave.m_nFramesPerSecond = paScene::nFramesPerSecond;

	paScene::outputStream = new CAIOoutStream();

	int err = paScene::outputStream->open(&_outputStreamConfig);
	if (err == -1) return err;

	paScene::outputStreamIsOpen = true;
	return err;
}



// These are for generating audio in the main thread with no blocking.

PHYA_API
int
paSetAutoGenerateMinimumDeviceBufferWriteSamples(int n){
	_outputStreamConfig.m_nMinDeviceBufferWriteSamples = n;
	return 0;
}

PHYA_API
int
paSetAdaptiveAutoGenerateMininumDeviceBufferSamplesFilled(int n){
	_outputStreamConfig.m_nMinDeviceBufferSamplesFilled = n;
	return 0;
}

#endif // USING_AIO

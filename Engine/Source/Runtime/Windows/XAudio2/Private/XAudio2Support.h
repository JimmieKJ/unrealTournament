// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XAudio2Support.h: XAudio2 specific structures.
=============================================================================*/

#pragma once

#ifndef XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	#define XAUDIO_SUPPORTS_XMA2WAVEFORMATEX	1
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
#ifndef XAUDIO_SUPPORTS_DEVICE_DETAILS
	#define XAUDIO_SUPPORTS_DEVICE_DETAILS		1
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS
#ifndef XAUDIO2_SUPPORTS_MUSIC
	#define XAUDIO2_SUPPORTS_MUSIC				1
#endif	//XAUDIO2_SUPPORTS_MUSIC
#ifndef X3DAUDIO_VECTOR_IS_A_D3DVECTOR
	#define X3DAUDIO_VECTOR_IS_A_D3DVECTOR		1
#endif	//X3DAUDIO_VECTOR_IS_A_D3DVECTOR


/*------------------------------------------------------------------------------------
	XAudio2 system headers
------------------------------------------------------------------------------------*/
#include "Engine.h"
#include "SoundDefinitions.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AllowWindowsPlatformTypes.h"
	#include <xaudio2.h>
	#include <X3Daudio.h>
#include "HideWindowsPlatformTypes.h"

/*------------------------------------------------------------------------------------
	Dependencies, helpers & forward declarations.
------------------------------------------------------------------------------------*/

#define AUDIO_HWTHREAD			XAUDIO2_DEFAULT_PROCESSOR

#define SPEAKER_5POINT0          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT )
#define SPEAKER_6POINT1          ( SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT | SPEAKER_BACK_CENTER )

#define UE4_XAUDIO3D_INPUTCHANNELS 1

struct FPCMBufferInfo
{
	/** Format of the source PCM data */
	WAVEFORMATEX				PCMFormat;
	/** Address of PCM data in physical memory */
	uint8*						PCMData;
	/** Size of PCM data in physical memory */
	UINT32						PCMDataSize;
};

#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
struct FXMA2BufferInfo
{
	/** Format of the source XMA2 data */
	XMA2WAVEFORMATEX			XMA2Format;
	/** Address of XMA2 data in physical memory */
	uint8*						XMA2Data;
	/** Size of XMA2 data in physical memory */
	UINT32						XMA2DataSize;
};
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX

struct FXWMABufferInfo
{
	/** Format of the source XWMA data */
	WAVEFORMATEXTENSIBLE		XWMAFormat;
	/** Additional info required for xwma */
	XAUDIO2_BUFFER_WMA			XWMABufferData;
	/** Address of XWMA data in physical memory */
	uint8*						XWMAData;
	/** Size of XWMA data in physical memory */
	UINT32						XWMADataSize;
	/** Address of XWMA seek data in physical memory */
	UINT32*						XWMASeekData;
	/** Size of XWMA seek data */
	UINT32						XWMASeekDataSize;
};

typedef FAsyncRealtimeAudioTaskProxy<FXAudio2SoundBuffer> FAsyncRealtimeAudioTask;

/**
* Struct to store pending task information.
*/
struct FPendingAsyncTaskInfo
{
	FAsyncRealtimeAudioTask* RealtimeAsyncTask;
	FAsyncRealtimeAudioTask* RealtimeAsyncHeaderParseTask;
	FSoundBuffer* Buffer;

	FPendingAsyncTaskInfo()
		: RealtimeAsyncTask(nullptr)
		, RealtimeAsyncHeaderParseTask(nullptr)
		, Buffer(nullptr)
	{}
};

/**
 * XAudio2 implementation of FSoundBuffer, containing the wave data and format information.
 */
class FXAudio2SoundBuffer : public FSoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FXAudio2SoundBuffer(FAudioDevice* AudioDevice, ESoundFormat SoundFormat);
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
	 */
	~FXAudio2SoundBuffer();

	//~ Begin FSoundBuffer interface
	int32 GetSize() override;
	int32 GetCurrentChunkIndex() const override;
	int32 GetCurrentChunkOffset() const override;
	bool IsRealTimeSourceReady() override;
	void EnsureRealtimeTaskCompletion() override;
	//~ End FSoundBuffer interface 

	/** 
	 * Set up this buffer to contain and play XMA2 data
	 */
	void InitXMA2( FXAudio2Device* XAudio2Device, USoundWave* Wave, struct FXMAInfo* XMAInfo );

	/** 
	 * Set up this buffer to contain and play XWMA data
	 */
	void InitXWMA( USoundWave* Wave, struct FXMAInfo* XMAInfo );

	/** 
	 * Setup a WAVEFORMATEX structure
	 */
	void InitWaveFormatEx( uint16 Format, USoundWave* Wave, bool bCheckPCMData );

	/**
	* Read the compressed info (i.e. parse header and open up a file handle) from the given sound wave.
	*/
	bool ReadCompressedInfo(USoundWave* SoundWave) override;

	/**
	 * Decompresses a chunk of compressed audio to the destination memory
	 *
	 * @param Destination		Memory to decompress to
	 * @param bLooping			Whether to loop the sound seamlessly, or pad with zeroes
	 * @return					Whether the sound looped or not
	 */
	bool ReadCompressedData( uint8* Destination, bool bLooping );

	/**
	 * Sets the point in time within the buffer to the specified time
	 * If the time specified is beyond the end of the sound, it will be set to the end
	 *
	 * @param SeekTime		Time in seconds from the beginning of sound to seek to
	 */
	void Seek( const float SeekTime );

	/**
	 * Static function used to create an OpenAL buffer and dynamically upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateQueuedBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and dynamically upload procedural data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateProceduralBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create an OpenAL buffer and upload raw PCM data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreatePreviewBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave, FXAudio2SoundBuffer* Buffer );

	/**
	 * Static function used to create an OpenAL buffer and upload decompressed ogg vorbis data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateNativeBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create an XAudio buffer and dynamically upload streaming data to.
	 *
	 * @param InWave		USoundWave to use as template and wave source
	 * @param AudioDevice	audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* CreateStreamingBuffer( FXAudio2Device* XAudio2Device, USoundWave* Wave );

	/**
	 * Static function used to create a buffer.
	 *
	 * @param InWave USoundWave to use as template and wave source
	 * @param AudioDevice audio device to attach created buffer to
	 * @return FXAudio2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FXAudio2SoundBuffer* Init( FAudioDevice* AudioDevice, USoundWave* InWave, bool bForceRealtime );

	/** Format of the sound referenced by this buffer */
	int32							SoundFormat;

	union
	{
		FPCMBufferInfo			PCM;		
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		FXMA2BufferInfo			XMA2;			// Xenon only
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
		FXWMABufferInfo			XWMA;			// Xenon only
	};

	/** Wrapper to handle the decompression of audio codecs */
	class ICompressedAudioInfo* DecompressionState;

	/** Asynch task for parsing real-time decompressed compressed info headers */
	FAsyncRealtimeAudioTask* RealtimeAsyncHeaderParseTask;

	/** Bool indicating the that the real-time source is ready for real-time decoding. */
	FThreadSafeBool bRealTimeSourceReady;

	/** Set to true when the PCM data should be freed when the buffer is destroyed */
	bool bDynamicResource;
};

/**
 * Source callback class for handling loops
 */
class FXAudio2SoundSourceCallback : public IXAudio2VoiceCallback
{
public:
	FXAudio2SoundSourceCallback() {}
	virtual ~FXAudio2SoundSourceCallback() {}
	virtual void STDCALL OnStreamEnd() {}
	virtual void STDCALL OnVoiceProcessingPassEnd() {}
	virtual void STDCALL OnVoiceProcessingPassStart(UINT32) {}
	virtual void STDCALL OnBufferStart(void* BufferContext) {}
	virtual void STDCALL OnVoiceError(void* BufferContext, HRESULT Error) {}

	/**
	* Function is called by xaudio2 for a playing voice every time a buffer finishes playing
	* We use it to launch async decoding tasks and read the results of previous, finished tasks.
	*/
	virtual void STDCALL OnBufferEnd(void* BufferContext);

	/**
	* Function called whenever an xaudio2 voice loops on itself. Used to trigger notifications on loop.
	*/
	virtual void STDCALL OnLoopEnd(void* BufferContext);

	friend class FXAudio2SoundSource;
};

/**
 * XAudio2 implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FXAudio2SoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 */
	FXAudio2SoundSource(FAudioDevice* InAudioDevice);

	/**
	 * Destructor, cleaning up voice
	 */
	virtual ~FXAudio2SoundSource();

	/**
	 * Frees existing resources. Called from destructor and therefore not virtual.
	 */
	void FreeResources();

	/**
	* Initializes any effects used with this source voice
	*/
	void InitializeSourceEffects(uint32 InVoiceId) override;

	/**
	* Attempts to initialize the sound source. If the source needs to decode asynchronously,
	* this may not result in the source actually initializing, but instead it may launch
	* an async task to parse the encoded file header and open a file handle. For ogg-vorbis
	* synchronous file parsing can cause significant hitches.
	*/
	virtual bool PrepareForInitialization(FWaveInstance* InWaveInstance) override;

	/**
	* Queries if the async task has finished parsing the real-time decoded file header and if the
	* the sound source is ready for initialization.
	*/
	virtual bool IsPreparedToInit() override;

	/**
	 * Initializes a source with a given wave instance and prepares it for playback.
	 *
	 * @param	WaveInstance	wave instance being primed for playback
	 * @return	true if initialization was successful, false otherwise
	 */
	virtual bool Init(FWaveInstance* WaveInstance) override;

	/**
	 * Updates the source specific parameter like e.g. volume and pitch based on the associated
	 * wave instance.	
	 */
	virtual void Update();

	/**
	 * Plays the current wave instance.	
	 */
	virtual void Play();

	/**
	 * Stops the current wave instance and detaches it from the source.	
	 */
	virtual void Stop();

	/**
	 * Pauses playback of current wave instance.
	 */
	virtual void Pause();

	/**
	 * Handles feeding new data to a real time decompressed sound
	 */
	void HandleRealTimeSource(bool bBlockForData);

	/**
	 * Handles pushing fetched real time source data to the hardware
	 */
	void HandleRealTimeSourceData(bool bLooped);

	/**
	 * Queries the status of the currently associated wave instance.
	 *
	 * @return	true if the wave instance/ source has finished playback and false if it is 
	 *			currently playing or paused.
	 */
	virtual bool IsFinished();

	/**
	 * Create a new source voice
	 */
	bool CreateSource();

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMBuffers();

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitPCMRTBuffers();

	/** 
	 * Submit the relevant audio buffers to the system, accounting for looping modes
	 */
	void SubmitXMA2Buffers();

	/** 
	 * Submit the relevant audio buffers to the system
	 */
	void SubmitXWMABuffers();

	/**
	 * Calculates the volume for each channel
	 */
	void GetChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume);

	/**
	 * Returns a string describing the source
	 */
	virtual FString Describe(bool bUseLongName) override;

	/**
	 * Returns a string describing the source. For internal use to avoid recursively calling GetChannelVolumes if invoked from GetChannelVolumes.
	 */
	FString Describe_Internal(bool bUseLongName, bool bIncludeChannelVolumes);

	/** 
	 * Maps a sound with a given number of channels to to expected speakers
	 */
	void RouteDryToSpeakers(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);

	/** 
	 * Maps the sound to the relevant reverb effect
	 */
	void RouteToReverb(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);

	/** 
	 * Maps the sound to the relevant radio effect.
	 *
	 * @param	ChannelVolumes	The volumes associated to each channel. 
	 *							Note: Not all channels are mapped directly to a speaker.
	 */
	void RouteToRadio(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);

protected:

	enum class EDataReadMode : uint8
	{
		Synchronous,
		Asynchronous,
		AsynchronousSkipFirstFrame
	};

	/** Decompress through XAudio2Buffer, or call USoundWave procedure to generate more PCM data. Returns true/false: did audio loop? */
	bool ReadMorePCMData(const int32 BufferIndex, EDataReadMode DataReadMode);

	/** Retrieves the realtime buffer data from the given buffer index */
	uint8* GetRealtimeBufferData(const int32 InBufferIndex, const int32 InBufferSize);

	/** Returns if the source is using the default 3d spatialization. */
	bool IsUsingHrtfSpatializer();

	/** Returns Whether or not to create this source with the 3d spatialization effect. */
	bool CreateWithSpatializationEffect();

	/**
	 * Utility function for determining the proper index of an effect. Certain effects (such as: reverb and radio distortion) 
	 * are optional. Thus, they may be NULL, yet XAudio2 cannot have a NULL output voice in the send list for this source voice.
	 *
	 * @return	The index of the destination XAudio2 submix voice for the given effect; -1 if effect not in destination array. 
	 *
	 * @param	Effect	The effect type's (Reverb, Radio Distoriton, etc) index to find. 
	 */
	int32 GetDestinationVoiceIndexForEffect( SourceDestinations Effect );

	/**
	* Converts a vector orientation from UE4 coordinates to XAudio2 coordinates
	*/
	inline FVector ConvertToXAudio2Orientation(const FVector& InputVector);

	/**
	* Calculates the channel volumes for various input channel configurations.
	*/
	void GetMonoChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume);
	void GetStereoChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume);
	void GetQuadChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume);
	void GetHexChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume);

	/**
	* Routes channel sends for various input channel configurations.
	*/
	void RouteMonoToDry(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);
	void RouteStereoToDry(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);
	void RouteQuadToDry(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);
	void RouteHexToDry(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);
	void RouteMonoToReverb(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);
	void RouteStereoToReverb(float ChannelVolumes[CHANNEL_MATRIX_COUNT]);
	
	/** Owning audio device object */
	FXAudio2Device*	AudioDevice;

	/** Pointer to effects manager, which handles updating singleton effects */
	FXAudio2EffectsManager* Effects;

	/** Cached subclass version of Buffer (which the base class has) */
	FXAudio2SoundBuffer* XAudio2Buffer;

	/** XAudio2 source voice associated with this source. */
	IXAudio2SourceVoice* Source;

	/** The max channels in the voice's effect chain. This is used to classify a pool for IXAudio2SourceVoice. */
	int32 MaxEffectChainChannels;

	/** Asynchronous task for real time audio decoding, created from main thread */
	FAsyncRealtimeAudioTask* RealtimeAsyncTask;

	/** Destination voices */
	XAUDIO2_SEND_DESCRIPTOR Destinations[DEST_COUNT];

	/** Used to allow notification when a sound loops and to feed audio to realtime decoded sources. */
	XAUDIO2_BUFFER XAudio2Buffers[3];

	/** Raw real-time buffer data for use with realtime XAudio2Buffers sources */
	TArray<uint8> RealtimeBufferData[3];

	/** Additional buffer info for XWMA sounds */
	XAUDIO2_BUFFER_WMA XAudio2BufferXWMA[1];

	/** Index of this sound source in the audio device sound source array. */
	uint32 VoiceId;

	/** Which sound buffer should be written to next - used for triple buffering. */
	int32 CurrentBuffer;

	/** Set to true when the loop end callback is hit */
	FThreadSafeBool bLoopCallback;

	/** Whether or not the sound has finished playing */
	FThreadSafeBool bIsFinished;

	/** Whether or not the cached first buffer has played. Used to skip first two reads of a RT decoded file */
	FThreadSafeBool bPlayedCachedBuffer;

	/** Whether or not we need to submit our first buffers to the voice */
	FThreadSafeBool bFirstRTBuffersSubmitted;

	/** Set when we wish to let the buffers play themselves out */
	FThreadSafeBool bBuffersToFlush;

	/** Set to true when we've allocated resources that need to be freed */
	uint32 bResourcesNeedFreeing : 1;

	/** Whether or not this sound is spatializing using an HRTF spatialization algorithm. */
	uint32 bUsingHRTFSpatialization : 1;

	/** Whether or not we've already logged a warning on this sound about it switching algorithms after init. */
	uint32 bEditorWarnedChangedSpatialization : 1;

	friend class FXAudio2Device;
	friend class FXAudio2SoundSourceCallback;
};

/**
 * Helper class for 5.1 spatialization.
 */
class FSpatializationHelper
{
	/** Instance of X3D used to calculate volume multipliers.	*/
	X3DAUDIO_HANDLE		          X3DInstance;
	
	X3DAUDIO_DSP_SETTINGS         DSPSettings;
	X3DAUDIO_LISTENER             Listener;
	X3DAUDIO_EMITTER              Emitter;
	X3DAUDIO_CONE                 Cone;
	
	X3DAUDIO_DISTANCE_CURVE_POINT VolumeCurvePoint[2];
	X3DAUDIO_DISTANCE_CURVE       VolumeCurve;
	
	X3DAUDIO_DISTANCE_CURVE_POINT ReverbVolumeCurvePoint[2];
	X3DAUDIO_DISTANCE_CURVE       ReverbVolumeCurve;

	float                         EmitterAzimuths[UE4_XAUDIO3D_INPUTCHANNELS];

	// TODO: Hardcoding this to 8 because X3DAudioCalculate is ignoring the destination speaker count we put in and
	//       using the number of speakers on the output device.  For 7.1 this means that it writes to 8 speakers,
	//       overrunning the buffer and trashing other static variables
	// 	float					      MatrixCoefficients[UE4_XAUDIO3D_INPUTCHANNELS * SPEAKER_COUNT];
	float					      MatrixCoefficients[8]; 
	
public:
	/**
	 * Constructor, initializing all member variables.
	 */
	FSpatializationHelper( void );

	void Init();

	/**
	 * Logs out the entire state of the SpatializationHelper
	 */
	void DumpSpatializationState() const;

	/**
	 * Calculates the spatialized volumes for each channel.
	 *
	 * @param	OrientFront				The listener's facing direction.
	 * @param	ListenerPosition		The position of the listener.
	 * @param	EmitterPosition			The position of the emitter.
	 * @param	OmniRadius				At what distance we start treating the sound source as spatialized
	 * @param	OutVolumes				An array of floats with one volume for each output channel.
	 */
	void CalculateDolbySurroundRate( const FVector& OrientFront, const FVector& ListenerPosition, const FVector& EmitterPosition, float OmniRadius, float* OutVolumes );
};

/** A pool entry for related IXAudio2SourceVoices */
struct FSourceVoicePoolEntry
{
	/** The format for all voices in this entry */
	WAVEFORMATEX Format;
	
	/** The max number of channels used in the effect chain for this voice. This is needed because
		XAudio2 defaults the max output channels for any effect chain to be the number of input channels. So
		a mono-to-stereo effect (e.g. for HRTF processing) would not work. 
	*/
	int32 MaxEffectChainChannels;
	
	/** The array of free voices in this pool entry. */
	TArray<struct IXAudio2SourceVoice*> FreeVoices;
};

/** Function to compare two WAVEFORMATEX structs */
FORCEINLINE bool operator==(const WAVEFORMATEX& FormatA, const WAVEFORMATEX& FormatB)
{
	/** Unfortunately, need to compare every member of the WAVEFORMATEX struct */
	return FormatA.cbSize 			== FormatB.cbSize &&
			FormatA.nAvgBytesPerSec == FormatB.nAvgBytesPerSec &&
			FormatA.nBlockAlign 	== FormatB.nBlockAlign &&
			FormatA.nChannels 		== FormatB.nChannels &&
			FormatA.nSamplesPerSec 	== FormatB.nSamplesPerSec &&
			FormatA.wBitsPerSample 	== FormatB.wBitsPerSample &&
			FormatA.wFormatTag 		== FormatB.wFormatTag;
}


/** This structure holds any singleton XAudio2 resources which need to be used, not just "properties" of the device. */
struct FXAudioDeviceProperties
{
	// These variables are non-static to support multiple audio device instances
	struct IXAudio2*					XAudio2;
	struct IXAudio2MasteringVoice*		MasteringVoice;
	HMODULE								XAudio2Dll;

	// These variables are static because they are common across all audio device instances
	static int32						NumSpeakers;
	static const float*					OutputMixMatrix;
#if XAUDIO_SUPPORTS_DEVICE_DETAILS
	static XAUDIO2_DEVICE_DETAILS		DeviceDetails;
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS

	// For calculating speaker maps for 3d audio
	FSpatializationHelper				SpatializationHelper;

	/** Source callback to handle looping sound callbacks */
	FXAudio2SoundSourceCallback	SourceCallback;
	
	/** The array of voice pools. Each pool is according to the sound format (and max effect chain channels) */
	TArray<FSourceVoicePoolEntry*> VoicePool;

	/** Number of non-free active voices */
	int32 NumActiveVoices;

	FXAudioDeviceProperties()
		: XAudio2(nullptr)
		, MasteringVoice(nullptr)
		, XAudio2Dll(nullptr)
		, NumActiveVoices(0)
	{
	}
	
	~FXAudioDeviceProperties()
	{
		// Make sure we've free'd all of our active voices at this point!
		check(NumActiveVoices == 0);

		// Destroy all the xaudio2 voices allocated in our pools
		for (int32 i = 0; i < VoicePool.Num(); ++i)
		{
			for (int32 j = 0; j < VoicePool[i]->FreeVoices.Num(); ++j)
			{
				IXAudio2SourceVoice** Voice = &VoicePool[i]->FreeVoices[j];
				check(*Voice != nullptr);
				(*Voice)->DestroyVoice();
				*Voice = nullptr;
			}
		}

		// Now delete all the pool entries
		for (int32 i = 0; i < VoicePool.Num(); ++i)
		{
			delete VoicePool[i];
			VoicePool[i] = nullptr;
		}

		// close hardware interfaces
		if (MasteringVoice)
		{
			MasteringVoice->DestroyVoice();
			MasteringVoice = nullptr;
		}

		if (XAudio2)
		{
			// Force the hardware to release all references
			Validate(TEXT("~FXAudioDeviceProperties: XAudio2->Release()"),
					 XAudio2->Release());
			XAudio2 = nullptr;
		}

#if PLATFORM_WINDOWS && PLATFORM_64BITS
		if (XAudio2Dll)
		{
			if (!FreeLibrary(XAudio2Dll))
			{
				UE_LOG(LogAudio, Warning, TEXT("Failed to free XAudio2 Dll"));
			}
		}
#endif
	}

	bool Validate(const TCHAR* Function, uint32 ErrorCode) const
	{
		if (ErrorCode != S_OK)
		{
			switch (ErrorCode)
			{
				case XAUDIO2_E_INVALID_CALL:
				UE_LOG(LogAudio, Error, TEXT("%s error: Invalid Call"), Function);
				break;

				case XAUDIO2_E_XMA_DECODER_ERROR:
				UE_LOG(LogAudio, Error, TEXT("%s error: XMA Decoder Error"), Function);
				break;

				case XAUDIO2_E_XAPO_CREATION_FAILED:
				UE_LOG(LogAudio, Error, TEXT("%s error: XAPO Creation Failed"), Function);
				break;

				case XAUDIO2_E_DEVICE_INVALIDATED:
				UE_LOG(LogAudio, Error, TEXT("%s error: Device Invalidated"), Function);
				break;

				default:
				UE_LOG(LogAudio, Error, TEXT("%s error: Unhandled error code %d"), Function, ErrorCode);
				break;
			};

			return false;
		}

		return true;
	}


	void GetAudioDeviceList(TArray<FString>& OutAudioDeviceList) const
	{
		OutAudioDeviceList.Reset();

#if XAUDIO_SUPPORTS_DEVICE_DETAILS
		uint32 NumDevices = 0;
		if (XAudio2)
		{
			if (Validate( TEXT("GetAudioDeviceList: XAudio2->GetDeviceCount"),
				XAudio2->GetDeviceCount(&NumDevices)))
			{
				for (uint32 i = 0; i < NumDevices; ++i)
				{
					XAUDIO2_DEVICE_DETAILS Details;
					if (Validate(TEXT("GetAudioDeviceList: XAudio2->GetDeviceDetails"),
						XAudio2->GetDeviceDetails(i, &Details)))
					{
						OutAudioDeviceList.Add(FString(Details.DisplayName));
					}
				}
			}
		}
#endif
	}

	/** Returns either a new IXAudio2SourceVoice or a recycled IXAudio2SourceVoice according to the sound format and max channel count in the voice's effect chain*/
	void GetFreeSourceVoice(IXAudio2SourceVoice** Voice, const FPCMBufferInfo& BufferInfo, const XAUDIO2_EFFECT_CHAIN* EffectChain = nullptr, const XAUDIO2_VOICE_SENDS* SendList = nullptr, int32 MaxEffectChainChannels = 0)
	{
		bool bSuccess = false;

		check(XAudio2 != nullptr);
		bSuccess = Validate(TEXT("GetFreeSourceVoice, XAudio2->CreateSourceVoice"),
							XAudio2->CreateSourceVoice(Voice, &BufferInfo.PCMFormat, XAUDIO2_VOICE_USEFILTER, MAX_PITCH, &SourceCallback, SendList, EffectChain));
		if (bSuccess)
		{
			// Track the number of source voices out in the world
			++NumActiveVoices;
		}
		else
		{
			// If something failed, make sure we null the voice ptr output
			*Voice = nullptr;
		}
	}

	/** Releases the voice into a pool of free voices according to the voice format and the max effect chain channels */
	void ReleaseSourceVoice(IXAudio2SourceVoice* Voice, const FPCMBufferInfo& BufferInfo, const int32 MaxEffectChainChannels)
	{
		Voice->DestroyVoice();

		--NumActiveVoices;
	}

};

#if XAUDIO_SUPPORTS_DEVICE_DETAILS
	#define UE4_XAUDIO2_NUMCHANNELS		FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.nChannels
	#define UE4_XAUDIO2_CHANNELMASK		FXAudioDeviceProperties::DeviceDetails.OutputFormat.dwChannelMask
	#define UE4_XAUDIO2_SAMPLERATE		FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.nSamplesPerSec
#else	//XAUDIO_SUPPORTS_DEVICE_DETAILS
	#define UE4_XAUDIO2_NUMCHANNELS		8		// Up to 7.1 supported
	#define UE4_XAUDIO2_CHANNELMASK		3		// Default to left and right speakers...
	#define UE4_XAUDIO2_SAMPLERATE		44100	// Default to CD sample rate
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS
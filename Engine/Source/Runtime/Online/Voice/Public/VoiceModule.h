// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

/** Logging related to general voice chat flow (muting/registration/etc) */
VOICE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoice, Display, All);
/** Logging related to encoding of local voice packets */
VOICE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoiceEncode, Display, All);
/** Logging related to decoding of remote voice packets */
VOICE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoiceDecode, Display, All);

/** Internal voice capture logging */
DECLARE_LOG_CATEGORY_EXTERN(LogVoiceCapture, Warning, All);

/**
 * Module for Voice capture/compression/decompression implementations
 */
class FVoiceModule : 
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FVoiceModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FVoiceModule>("Voice");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Voice");
	}

	/**
	 * Instantiates a new voice capture object
	 *
	 * @return new voice capture object, possibly NULL
	 */
	virtual TSharedPtr<class IVoiceCapture> CreateVoiceCapture();

	/**
	 * Instantiates a new voice encoder object
	 *
	 * @return new voice encoder object, possibly NULL
	 */
	virtual TSharedPtr<class IVoiceEncoder> CreateVoiceEncoder();

	/**
	 * Instantiates a new voice decoder object
	 *
	 * @return new voice decoder object, possibly NULL
	 */
	virtual TSharedPtr<class IVoiceDecoder> CreateVoiceDecoder();

	/**
	 * @return true if voice is enabled
	 */
	inline bool IsVoiceEnabled() const
	{
		return bEnabled;
	}

private:

	// IModuleInterface

	/**
	 * Called when voice module is loaded
	 * Initialize platform specific parts of voice handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when voice module is unloaded
	 * Shutdown platform specific parts of voice handling
	 */
	virtual void ShutdownModule() override;

	/** Is voice interface enabled */
	bool bEnabled;
};


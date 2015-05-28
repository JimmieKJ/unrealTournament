// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformChunkInstall.h: Generic platform chunk based install classes.
==============================================================================================*/

#pragma once

#include "ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkInstaller, Log, All);

namespace EChunkLocation
{
	enum Type
	{
		// note: higher quality locations should have higher enum values, we sort by these in AssetRegistry.cpp
		DoesNotExist,	// chunk does not exist
		NotAvailable,	// chunk has not been installed yet
		LocalSlow,		// chunk is on local slow media (optical)
		LocalFast,		// chunk is on local fast media (HDD)

		BestLocation=LocalFast
	};
}


namespace EChunkInstallSpeed
{
	enum Type
	{
		Paused,					// chunk installation is paused
		Slow,					// installation is lower priority than Game IO
		Fast					// installation is higher priority than Game IO
	};
}

namespace EChunkPriority
{
	enum Type
	{
		Immediate,	// Chunk install is of highest priority, this can cancel lower priority installs.
		High	 ,	// Chunk is probably required soon so grab is as soon as possible.
		Low		 ,	// Install this chunk only when other chunks are not needed.
	};
}

namespace EChunkProgressReportingType
{
	enum Type
	{
		ETA,					// time remaining in seconds
		PercentageComplete		// percentage complete in 99.99 format
	};
}

/**
 * Platform Chunk Install Module Interface
 */
class IPlatformChunkInstallModule : public IModuleInterface
{
public:

	virtual IPlatformChunkInstall* GetPlatformChunkInstall() = 0;
};


DECLARE_DELEGATE_OneParam(FPlatformChunkInstallCompleteDelegate, uint32);

/**
* Interface for platform specific chunk based install
**/
class CORE_API IPlatformChunkInstall
{
public:

	/** Virtual destructor */
	virtual ~IPlatformChunkInstall() {}

	/**
	 * Get the current location of a chunk.
	 * @param ChunkID		The id of the chunk to check.
	 * @return				Enum specifying whether the chunk is available to use, waiting to install, or does not exist.
	 **/
	virtual EChunkLocation::Type GetChunkLocation( uint32 ChunkID ) = 0;

	/** 
	 * Check if a given reporting type is supported.
	 * @param ReportType	Enum specifying how progress is reported.
	 * @return				true if reporting type is supported on the current platform.
	 **/
	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) = 0;		

	/**
	 * Get the current install progress of a chunk.  Let the user specify report type for platforms that support more than one.
	 * @param ChunkID		The id of the chunk to check.
	 * @param ReportType	The type of progress report you want.
	 * @return				A value whose meaning is dependent on the ReportType param.
	 **/
	virtual float GetChunkProgress( uint32 ChunkID, EChunkProgressReportingType::Type ReportType ) = 0;

	/**
	 * Inquire about the priority of chunk installation vs. game IO.
	 * @return				Paused, low or high priority.
	 **/
	virtual EChunkInstallSpeed::Type GetInstallSpeed() = 0;
	/**
	 * Specify the priority of chunk installation vs. game IO.
	 * @param InstallSpeed	Pause, low or high priority.
	 * @return				false if the operation is not allowed, otherwise true.
	 **/
	virtual bool SetInstallSpeed( EChunkInstallSpeed::Type InstallSpeed ) = 0;
	
	/**
	 * Hint to the installer that we would like to prioritize a specific chunk
	 * @param ChunkID		The id of the chunk to prioritize.
	 * @param Priority		The priority for the chunk.
	 * @return				false if the operation is not allowed or the chunk doesn't exist, otherwise true.
	 **/
	virtual bool PrioritizeChunk( uint32 ChunkID, EChunkPriority::Type Priority ) = 0;

	/**
	 * For platforms that support emulation of the Chunk install.  Starts transfer of the next chunk.
	 * Does nothing in a shipping build.
	 * @return				true if the opreation succeeds.
	 **/
	virtual bool DebugStartNextChunk() = 0;

	/** 
	 * Request a delegate callback on chunk install completion. Request may not be respected.
	 * @param ChunkID		The id of the chunk of interest.
	 * @param Delegate		The delegate to call when the chunk is installed.
	 * @return				False if the delegate was not registered. True on success.
	 */
	virtual FDelegateHandle SetChunkInstallDelgate( uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate ) = 0;

	/**
	* Remove a delegate callback on chunk install completion.
	* @param ChunkID		The id of the chunk of interest.
	* @param Delegate		The delegate to remove.
	*/
	virtual void RemoveChunkInstallDelgate(uint32 ChunkID, FDelegateHandle Delegate) = 0;
};


/**
* Generic implementation of chunk based install
**/
class CORE_API FGenericPlatformChunkInstall : public IPlatformChunkInstall
{
public:
	/**
	 * Get the current location of a chunk.
	 * @param ChunkID		The id of the chunk to check.
	 * @return				Enum specifying whether the chunk is available to use, waiting to install, or does not exist.
	 **/
	virtual EChunkLocation::Type GetChunkLocation( uint32 ChunkID ) override
	{
		return EChunkLocation::LocalFast;
	}

	/** 
	 * Check if a given reporting type is supported.
	 * @param ReportType	Enum specifying how progress is reported.
	 * @return				true if reporting type is supported on the current platform.
	 **/
	virtual bool GetProgressReportingTypeSupported(EChunkProgressReportingType::Type ReportType) override
	{
		if (ReportType == EChunkProgressReportingType::PercentageComplete)
		{
			return true;
		}

		return false;
	}

	/**
	 * Get the current install progress of a chunk.  Let the user specify report type for platforms that support more than one.
	 * @param ChunkID		The id of the chunk to check.
	 * @param ReportType	The type of progress report you want.
	 * @return				A value whose meaning is dependent on the ReportType param.
	 **/
	virtual float GetChunkProgress( uint32 ChunkID, EChunkProgressReportingType::Type ReportType ) override
	{
		if (ReportType == EChunkProgressReportingType::PercentageComplete)
		{
			return 100.0f;
		}		
		return 0.0f;
	}

	/**
	 * Inquire about the priority of chunk installation vs. game IO.
	 * @return				Paused, low or high priority.
	 **/
	virtual EChunkInstallSpeed::Type GetInstallSpeed() override
	{
		return EChunkInstallSpeed::Paused;
	}

	/**
	 * Specify the priority of chunk installation vs. game IO.
	 * @param InstallSpeed	Pause, low or high priority.
	 * @return				false if the operation is not allowed, otherwise true.
	 **/
	virtual bool SetInstallSpeed( EChunkInstallSpeed::Type InstallSpeed ) override
	{
		return false;
	}
	
	/**
	 * Hint to the installer that we would like to prioritize a specific chunk (moves it to the head of the list.)
	 * @param ChunkID		The id of the chunk to prioritize.
	 * @return				false if the operation is not allowed or the chunk doesn't exist, otherwise true.
	 **/
	virtual bool PrioritizeChunk( uint32 ChunkID, EChunkPriority::Type Priority ) override
	{
		return false;
	}

	/**
	 * For platforms that support emulation of the Chunk install.  Starts transfer of the next chunk.
	 * Does nothing in a shipping build.
	 * @return				true if the opreation succeeds.
	 **/
	virtual bool DebugStartNextChunk() override
	{
		return true;
	}

	/**
	* Request a delegate callback on chunk install completion. Request may not be respected.
	* @param ChunkID		The id of the chunk of interest.
	* @param Delegate		The delegate when the chunk is installed.
	* @return				False if the delegate was not registered or the chunk is already installed. True on success.
	*/
	virtual FDelegateHandle SetChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate) override
	{
		return FDelegateHandle();
	}

	/**
	* Remove a delegate callback on chunk install completion.
	* @param ChunkID		The id of the chunk of interest.
	* @param Delegate		The delegate to remove.
	* @return				False if the delegate was not registered with a call to SetChunkInstallDelgate. True on success.
	*/
	virtual void RemoveChunkInstallDelgate(uint32 ChunkID, FDelegateHandle Delegate) override
	{
		return;
	}
};

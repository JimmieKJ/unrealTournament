// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformFile.cpp: Generic implementations of platform file I/O functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "ModuleManager.h"
#include "../HAL/IPlatformFileLogWrapper.h"
#include "../HAL/IPlatformFileProfilerWrapper.h"
#include "../HAL/IPlatformFileCachedWrapper.h"
#include "../HAL/IPlatformFileModule.h"
#include "../HAL/IPlatformFileOpenLogWrapper.h"


FPlatformFileManager::FPlatformFileManager()
	: TopmostPlatformFile(NULL)
{}

IPlatformFile& FPlatformFileManager::GetPlatformFile()
{
	if (TopmostPlatformFile == NULL)
	{
		TopmostPlatformFile = &IPlatformFile::GetPlatformPhysical();
	}
	return *TopmostPlatformFile;
}

void FPlatformFileManager::SetPlatformFile(IPlatformFile& NewTopmostPlatformFile)
{
	TopmostPlatformFile = &NewTopmostPlatformFile;
	TopmostPlatformFile->InitializeAfterSetActive();
}

IPlatformFile* FPlatformFileManager::FindPlatformFile(const TCHAR* Name)
{
	check(TopmostPlatformFile != NULL);
	for (IPlatformFile* ChainElement = TopmostPlatformFile; ChainElement; ChainElement = ChainElement->GetLowerLevel())
	{
		if (FCString::Stricmp(ChainElement->GetName(), Name) == 0)
		{
			return ChainElement;
		}
	}
	return NULL;
}

IPlatformFile* FPlatformFileManager::GetPlatformFile(const TCHAR* Name)
{
	IPlatformFile* PlatformFile = NULL;

	// Check Core platform files (Profile, Log) by name.
	if (FCString::Strcmp(FLoggedPlatformFile::GetTypeName(), Name) == 0)
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new FLoggedPlatformFile());
		PlatformFile = AutoDestroySingleton.GetOwnedPointer();
	}
#if !UE_BUILD_SHIPPING
	else if (FCString::Strcmp(TProfiledPlatformFile<FProfiledFileStatsFileDetailed>::GetTypeName(), Name) == 0)
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new TProfiledPlatformFile<FProfiledFileStatsFileDetailed>());
		PlatformFile = AutoDestroySingleton.GetOwnedPointer();
	}
	else if (FCString::Strcmp(TProfiledPlatformFile<FProfiledFileStatsFileSimple>::GetTypeName(), Name) == 0)
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new TProfiledPlatformFile<FProfiledFileStatsFileSimple>());
		PlatformFile = AutoDestroySingleton.GetOwnedPointer();
	}
	else if (FCString::Strcmp(FPlatformFileReadStats::GetTypeName(), Name) == 0)
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new FPlatformFileReadStats());
		PlatformFile = AutoDestroySingleton.GetOwnedPointer();
	}
	else if (FCString::Strcmp(FPlatformFileOpenLog::GetTypeName(), Name) == 0)
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new FPlatformFileOpenLog());
		PlatformFile = AutoDestroySingleton.GetOwnedPointer();
	}
#endif
	else if (FCString::Strcmp(FCachedReadPlatformFile::GetTypeName(), Name) == 0)
	{
		static TScopedPointer<IPlatformFile> AutoDestroySingleton(new FCachedReadPlatformFile());
		PlatformFile = AutoDestroySingleton.GetOwnedPointer();
	}
	else
	{
		// Try to load a module containing the platform file.
		class IPlatformFileModule* PlatformFileModule = FModuleManager::LoadModulePtr<IPlatformFileModule>(Name);
		if (PlatformFileModule != NULL)
		{
			// TODO: Attempt to create platform file
			PlatformFile = PlatformFileModule->GetPlatformFile();
		}
	}

	return PlatformFile;
}

FPlatformFileManager& FPlatformFileManager::Get()
{
	static FPlatformFileManager Singleton;
	return Singleton;
}

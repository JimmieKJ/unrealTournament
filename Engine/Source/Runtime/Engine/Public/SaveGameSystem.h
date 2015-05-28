// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_HTML5_BROWSER
	#include "HTML5JavaScriptFx.h"
#endif

/**
 * Interface for platform feature modules
 */

/** Defines the interface to platform's save game system (or a generic file based one) */
class ISaveGameSystem
{
public:

	/** Returns true if the platform has a native UI (like many consoles) */
	virtual bool PlatformHasNativeUI() = 0;

	/** Return true if the named savegame exists (probably not useful with NativeUI */
	virtual bool DoesSaveGameExist(const TCHAR* Name, const int32 UserIndex) = 0;

	/** Saves the game, blocking until complete. Platform may use FGameDelegates to get more information from the game */
	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data) = 0;

	/** Loads the game, blocking until complete */
	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data) = 0;

	/** Delete an existing save game, blocking until complete */
	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex) = 0;
};


//  @todo akhare - move this to core before checking - Properly implement the SaveSystem  for HTML5
/** A generic save game system that just uses IFileManager to save/load with normal files */
class FGenericSaveGameSystem : public ISaveGameSystem
{
public:
	virtual bool PlatformHasNativeUI() override
	{
		return false;
	}

	virtual bool DoesSaveGameExist(const TCHAR* Name, const int32 UserIndex) override
	{
#if PLATFORM_HTML5_BROWSER
		return UE_DoesSaveGameExist(TCHAR_TO_ANSI(Name),UserIndex);
#elif PLATFORM_HTML5_WIN32
		FILE *fp;
		fp=fopen("c:\\test.sav", "r");
		if (fp)
			return true;
		return false;
#else
		return IFileManager::Get().FileSize(*GetSaveGamePath(Name)) >= 0;
#endif
	}

	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data) override
	{
#if PLATFORM_HTML5_BROWSER
		return UE_SaveGame(TCHAR_TO_ANSI(Name),UserIndex,(char*)Data.GetData(),Data.Num());
#elif PLATFORM_HTML5_WIN32
		FILE *fp;
		fp=fopen("c:\\test.sav", "wb");
		fwrite((char*)Data.GetData(), sizeof(char), Data.Num(), fp);
		fclose(fp);
		return true;
#else
		return FFileHelper::SaveArrayToFile(Data, *GetSaveGamePath(Name));
#endif
	}

	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data) override
	{
#if PLATFORM_HTML5_BROWSER
		char*	OutData;
		int		Size;
		bool Result = UE_LoadGame(TCHAR_TO_ANSI(Name),UserIndex,&OutData,&Size);
		if (!Result)
			return false; 
		Data.Append((uint8*)OutData,Size);
		::free (OutData);
		return true;
#elif PLATFORM_HTML5_WIN32
		FILE *fp;
		fp=fopen("c:\\test.sav","rb");
		if (!fp)
			return false;
			// obtain file size:
		fseek (fp, 0 , SEEK_END);
		int size = ftell (fp);
		fseek (fp, 0 , SEEK_SET);
		Data.AddUninitialized(size);
		int result = fread (Data.GetData(),1,size,fp);
		fclose(fp);
		return true;
#else
		return FFileHelper::LoadFileToArray(Data, *GetSaveGamePath(Name));
#endif
	}

	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex) override
	{
#if PLATFORM_HTML5_BROWSER
		return false;
#else
		return IFileManager::Get().Delete(*GetSaveGamePath(Name), true, false, !bAttemptToUseUI);
#endif
	}

protected:

	/** Get the path to save game file for the given name, a platform _may_ be able to simply override this and no other functions above */
	virtual FString GetSaveGamePath(const TCHAR* Name)
	{
		return FString::Printf(TEXT("%s/SaveGames/%s.sav"), *FPaths::GameSavedDir(), Name);
	}
};

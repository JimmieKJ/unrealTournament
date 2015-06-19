// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*===================================================================================
	Scalability.h: Manager class for handling scalability settings
=====================================================================================*/

#pragma once

namespace Scalability
{ 
	/**
	 * Structure for holding the state of the engine scalability groups
	 * Actual engine state you can get though GetQualityLevels().
	**/
	struct ENGINE_API FQualityLevels
	{
		int32 ResolutionQuality;
		int32 ViewDistanceQuality;
		int32 AntiAliasingQuality;
		int32 ShadowQuality;
		int32 PostProcessQuality;
		int32 TextureQuality;
		int32 EffectsQuality;

		FQualityLevels()
		{
			SetDefaults();
		}
		
		bool operator==(const FQualityLevels& Other ) const
		{
			return FMemory::Memcmp( this, &Other, sizeof(FQualityLevels) ) == 0;
		}

		bool operator!=(const FQualityLevels& Other ) const
		{
			return !(*this == Other);
		}

		// @param Value 0:low, 1:medium, 2:high, 3:epic
		void SetFromSingleQualityLevel(int32 Value);

		void SetBenchmarkFallback();

		void SetDefaults();
	};


	/** This is the only suggested way to set the current state - don't set CVars directly **/
	ENGINE_API void SetQualityLevels(const FQualityLevels& QualityLevels);

	/** This is the only suggested way to get the current state - don't get CVars directly */
	ENGINE_API FQualityLevels GetQualityLevels();

	/**  */
	ENGINE_API void InitScalabilitySystem();

	/** @param IniName e.g. GEditorPerProjectIni or GGameUserSettingsIni */
	ENGINE_API void LoadState(const FString& IniName);
	
	/** @param IniName e.g. GEditorPerProjectIni or GGameUserSettingsIni */
	ENGINE_API void SaveState(const FString& IniName);

	/**
	 * Sends an analytic event with all quality level data
	 *
	 * @param bAutoApplied	Whether or not the quality levels were auto-applied (true) or applied by the user (false).
	 */
	ENGINE_API void RecordQualityLevelsAnalytics(bool bAutoApplied);

	/** Run synthbenchmark and configure scalability based on results **/
	ENGINE_API FQualityLevels BenchmarkQualityLevels(uint32 WorkScale=10);

	/** Process a console command line **/
	ENGINE_API void ProcessCommand(const TCHAR* Cmd, FOutputDevice& Ar);

	/** Minimum single axis scale for render resolution */
	static const int32 MinResolutionScale = 50;

	/** Maximum single axis scale for render resolution */
	static const int32 MaxResolutionScale = 100;
}

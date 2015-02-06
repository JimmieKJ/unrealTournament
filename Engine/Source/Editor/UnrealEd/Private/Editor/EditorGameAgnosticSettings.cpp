// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

UEditorGameAgnosticSettings::UEditorGameAgnosticSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCopyStarterContentPreference = true;
	bEditorAnalyticsEnabled = true;
	AutoScalabilityWorkScaleAmount = 1;
}

void UEditorGameAgnosticSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	const FName Name = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;
	if (Name == FName(TEXT("bLoadTheMostRecentlyLoadedProjectAtStartup")))
	{
		const FString& AutoLoadProjectFileName = IProjectManager::Get().GetAutoLoadProjectFileName();
		if ( GEditor->GetGameAgnosticSettings().bLoadTheMostRecentlyLoadedProjectAtStartup )
		{
			// Form or overwrite the file that is read at load to determine the most recently loaded project file
			FFileHelper::SaveStringToFile(FPaths::GetProjectFilePath(), *AutoLoadProjectFileName);
		}
		else
		{
			// Remove the file. It's possible for bLoadTheMostRecentlyLoadedProjectAtStartup to be set before FPaths::GetProjectFilePath() is valid, so we need to distinguish the two cases.
			IFileManager::Get().Delete(*AutoLoadProjectFileName);
		}
	}

	GEditor->SaveGameAgnosticSettings();
}

void UEditorGameAgnosticSettings::LoadScalabilityBenchmark()
{
	check(!GEditorGameAgnosticIni.IsEmpty());

	const TCHAR* Section = TEXT("EngineBenchmarkResult");

	Scalability::FQualityLevels Temporary;

	if (IsScalabilityBenchmarkValid())
	{
		GConfig->GetInt(Section, TEXT("ResolutionQuality"),		Temporary.ResolutionQuality,	GEditorGameAgnosticIni);
		GConfig->GetInt(Section, TEXT("ViewDistanceQuality"),		Temporary.ViewDistanceQuality,	GEditorGameAgnosticIni);
		GConfig->GetInt(Section, TEXT("AntiAliasingQuality"),		Temporary.AntiAliasingQuality,	GEditorGameAgnosticIni);
		GConfig->GetInt(Section, TEXT("ShadowQuality"),			Temporary.ShadowQuality,		GEditorGameAgnosticIni);
		GConfig->GetInt(Section, TEXT("PostProcessQuality"),		Temporary.PostProcessQuality,	GEditorGameAgnosticIni);
		GConfig->GetInt(Section, TEXT("TextureQuality"),			Temporary.TextureQuality,		GEditorGameAgnosticIni);
		GConfig->GetInt(Section, TEXT("EffectsQuality"),			Temporary.EffectsQuality,		GEditorGameAgnosticIni);
		EngineBenchmarkResult = Temporary;
	}
}

void UEditorGameAgnosticSettings::AutoApplyScalabilityBenchmark()
{
	const TCHAR* Section = TEXT("EngineBenchmarkResult");

	FScopedSlowTask SlowTask(0, NSLOCTEXT("UnrealEd", "RunningEngineBenchmark", "Running engine benchmark..."));
	SlowTask.MakeDialog();


	Scalability::FQualityLevels Temporary = Scalability::BenchmarkQualityLevels(AutoScalabilityWorkScaleAmount);

	GConfig->SetBool(Section, TEXT("Valid"), true, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("ResolutionQuality"), Temporary.ResolutionQuality, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("ViewDistanceQuality"), Temporary.ViewDistanceQuality, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("AntiAliasingQuality"), Temporary.AntiAliasingQuality, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("ShadowQuality"), Temporary.ShadowQuality, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("PostProcessQuality"), Temporary.PostProcessQuality, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("TextureQuality"), Temporary.TextureQuality, GEditorGameAgnosticIni);
	GConfig->SetInt(Section, TEXT("EffectsQuality"), Temporary.EffectsQuality, GEditorGameAgnosticIni);

	Scalability::SetQualityLevels(Temporary);
	Scalability::SaveState(GEditorGameAgnosticIni);
}

bool UEditorGameAgnosticSettings::IsScalabilityBenchmarkValid() const
{
	const TCHAR* Section = TEXT("EngineBenchmarkResult");

	bool bIsValid = false;
	GConfig->GetBool(Section, TEXT("Valid"), bIsValid, GEditorGameAgnosticIni);

	return bIsValid;
}
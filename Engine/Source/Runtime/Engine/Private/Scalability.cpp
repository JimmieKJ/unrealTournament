// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Scalability.h"
#include "SynthBenchmark.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

static TAutoConsoleVariable<float> CVarResolutionQuality(
	TEXT("sg.ResolutionQuality"),
	100.0f,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 10..100, default: 100"),
	ECVF_ScalabilityGroup);

static TAutoConsoleVariable<int32> CVarViewDistanceQuality(
	TEXT("sg.ViewDistanceQuality"),
	3,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 0:low, 1:med, 2:high, 3:epic, default: 3"),
	ECVF_ScalabilityGroup);

static TAutoConsoleVariable<int32> CVarAntiAliasingQuality(
	TEXT("sg.AntiAliasingQuality"),
	3,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 0:low, 1:med, 2:high, 3:epic, default: 3"),
	ECVF_ScalabilityGroup);

static TAutoConsoleVariable<int32> CVarShadowQuality(
	TEXT("sg.ShadowQuality"),
	3,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 0:low, 1:med, 2:high, 3:epic, default: 3"),
	ECVF_ScalabilityGroup);

static TAutoConsoleVariable<int32> CVarPostProcessQuality(
	TEXT("sg.PostProcessQuality"),
	3,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 0:low, 1:med, 2:high, 3:epic, default: 3"),
	ECVF_ScalabilityGroup);

static TAutoConsoleVariable<int32> CVarTextureQuality(
	TEXT("sg.TextureQuality"),
	3,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 0:low, 1:med, 2:high, 3:epic, default: 3"),
	ECVF_ScalabilityGroup);

static TAutoConsoleVariable<int32> CVarEffectsQuality(
	TEXT("sg.EffectsQuality"),
	3,
	TEXT("Scalability quality state (internally used by scalability system, ini load/save or using SCALABILITY console command)\n")
	TEXT(" 0:low, 1:med, 2:high, 3:epic, default: 3"),
	ECVF_ScalabilityGroup);

namespace Scalability
{
	// Select a the correct quality level for the given benchmark value and thresholds
static int32 ComputeOptionFromPerfIndex(const FString& GroupName, float CPUPerfIndex, float GPUPerfIndex)
{
	// Some code defaults in case the ini file can not be read or has dirty data
	float PerfIndex = FMath::Min(CPUPerfIndex, GPUPerfIndex);
	float Index01 = 20;
	float Index12 = 50;
	float Index23 = 70;

	if (GConfig)
	{
		const FString ArrayKey = FString(TEXT("PerfIndexThresholds_")) + GroupName;
		TArray<FString> PerfIndexThresholds;
		GConfig->GetSingleLineArray(TEXT("ScalabilitySettings"), *ArrayKey, PerfIndexThresholds, GScalabilityIni);

		// This array takes on the form: "TypeString Index01 Index12 Index23"
		if (PerfIndexThresholds.Num() > 3)
		{
			const FString TypeString = PerfIndexThresholds[0];
			bool bTypeValid = false;
			if (TypeString == TEXT("CPU"))
			{
				PerfIndex = CPUPerfIndex;
				bTypeValid = true;
			}
			else if (TypeString == TEXT("GPU"))
			{
				PerfIndex = GPUPerfIndex;
				bTypeValid = true;
			}
			else if (TypeString == TEXT("Min"))
			{
				PerfIndex = FMath::Min(CPUPerfIndex, GPUPerfIndex);
				bTypeValid = true;
			}

			if (bTypeValid)
			{
				Index01 = FCString::Atof(*PerfIndexThresholds[1]);
				Index12 = FCString::Atof(*PerfIndexThresholds[2]);
				Index23 = FCString::Atof(*PerfIndexThresholds[3]);
			}
		}
	}

	if(PerfIndex < Index01)
	{
		return 0;
	}
	if(PerfIndex < Index12)
	{
		return 1;
	}
	if(PerfIndex < Index23)
	{
		return 2;
	}

	return 3;
}

// Extract the name and quality level from an ini section name. Sections in the ini file are named <GroupName>@<QualityLevel> 
static bool SplitSectionName(const FString& InSectionName, FString& OutSectionGroupName, int32& OutQualityLevel)
{
	bool bSuccess = false;
	FString Left, Right;

	if (InSectionName.Split(TEXT("@"), &Left, &Right))
	{
		OutSectionGroupName = Left;
		OutQualityLevel = FCString::Atoi(*Right);
		bSuccess = true;
	}

	return bSuccess;
}

// Try and match the current cvar state against the scalability sections too see if one matches. OutQualityLevel will be set to -1 if none match
static void InferCurrentQualityLevel(const FString& InGroupName, int32& OutQualityLevel, TArray<FString>* OutCVars)
{
	TArray<FString> SectionNames;
	GConfig->GetSectionNames(GScalabilityIni, SectionNames);
	OutQualityLevel = -1;

	for (int32 SectionNameIndex = 0; SectionNameIndex < SectionNames.Num(); ++SectionNameIndex)
	{
		FString GroupName;
		int32 GroupQualityLevel;

		if (SplitSectionName(SectionNames[SectionNameIndex], GroupName, GroupQualityLevel))
		{
			if (GroupName == FString(InGroupName))
			{
				TArray<FString> CVarData;
				GConfig->GetSection(*SectionNames[SectionNameIndex], CVarData, GScalabilityIni);

				bool bAllMatch = true;

				// Check all cvars against current state to see if they match
				for (int32 DataIndex = 0; DataIndex < CVarData.Num(); ++DataIndex)
				{
					const FString& CVarString = CVarData[DataIndex];
					FString CVarName, CVarValue;
					if (CVarString.Split(TEXT("="), &CVarName, &CVarValue))
					{
						IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
						if (CVar)
						{
							float CurrentValue = CVar->GetFloat();
							float ValueToCheck = FCString::Atof(*CVarValue);

							if (ValueToCheck != CurrentValue)
							{
								bAllMatch = false;
								break;
							}
						}
					}
				}

				if (bAllMatch)
				{
					OutQualityLevel = FMath::Max(OutQualityLevel, GroupQualityLevel);
					if (OutCVars)
					{
						*OutCVars = CVarData;
					}
				}
			}
		}
	}
}

static void SetGroupQualityLevel(const TCHAR* InGroupName, int32 InQualityLevel)
{
	InQualityLevel = FMath::Clamp(InQualityLevel, 0, 3);

//	UE_LOG(LogConsoleResponse, Display, TEXT("  %s %d"), InGroupName, InQualityLevel);

	ApplyCVarSettingsGroupFromIni(InGroupName, InQualityLevel, *GScalabilityIni, ECVF_SetByScalability);
}

static void SetResolutionQualityLevel(float InResolutionQualityLevel)
{
	InResolutionQualityLevel = FMath::Clamp(InResolutionQualityLevel, Scalability::MinResolutionScale, Scalability::MaxResolutionScale);

//	UE_LOG(LogConsoleResponse, Display, TEXT("  ResolutionQuality %.2f"), "", InResolutionQualityLevel);

	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));

	// if it wasn't created yet we either need to change the order or store like we do for ini loading
	check(CVar);

	CVar->Set(InResolutionQualityLevel, ECVF_SetByScalability);
}

void OnChangeResolutionQuality(IConsoleVariable* Var)
{
	SetResolutionQualityLevel(Var->GetFloat());
}
void OnChangeViewDistanceQuality(IConsoleVariable* Var)
{
	SetGroupQualityLevel(TEXT("ViewDistanceQuality"), Var->GetInt());
}
void OnChangeAntiAliasingQuality(IConsoleVariable* Var)
{
	SetGroupQualityLevel(TEXT("AntiAliasingQuality"), Var->GetInt());
}
void OnChangeShadowQuality(IConsoleVariable* Var)
{
	SetGroupQualityLevel(TEXT("ShadowQuality"), Var->GetInt());
}
void OnChangePostProcessQuality(IConsoleVariable* Var)
{
	SetGroupQualityLevel(TEXT("PostProcessQuality"), Var->GetInt());
}
void OnChangeTextureQuality(IConsoleVariable* Var)
{
	SetGroupQualityLevel(TEXT("TextureQuality"), Var->GetInt());
}
void OnChangeEffectsQuality(IConsoleVariable* Var)
{
	SetGroupQualityLevel(TEXT("EffectsQuality"), Var->GetInt());
}

void InitScalabilitySystem()
{
	// needed only once
	{
		static bool bInit = false;

		if(bInit)
		{
			return;
		}

		bInit = true;
	}

	CVarResolutionQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeResolutionQuality));
	CVarViewDistanceQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeViewDistanceQuality));
	CVarAntiAliasingQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeAntiAliasingQuality));
	CVarShadowQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeShadowQuality));
	CVarPostProcessQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangePostProcessQuality));
	CVarTextureQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeTextureQuality));
	CVarEffectsQuality.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeEffectsQuality));
}

/** Get the percentage scale for a given quality level */
static int32 GetRenderScaleLevelFromQualityLevel(int32 InQualityLevel)
{
	check(InQualityLevel >= 0 && InQualityLevel <=3);
	static const int32 ScalesForQuality[4] = { 50, 71, 87, 100 }; // Single axis scales which give 25/50/75/100% area scales
	return ScalesForQuality[InQualityLevel];
}

FQualityLevels BenchmarkQualityLevels(uint32 WorkScale, float CPUMultiplier, float GPUMultiplier)
{
	ensure((CPUMultiplier > 0.0f) && (GPUMultiplier > 0.0f));

	// benchmark the system

	FQualityLevels Results;

	FSynthBenchmarkResults SynthBenchmark;
	ISynthBenchmark::Get().Run(SynthBenchmark, true, WorkScale);

	const float CPUPerfIndex = SynthBenchmark.ComputeCPUPerfIndex() * CPUMultiplier;
	const float GPUPerfIndex = SynthBenchmark.ComputeGPUPerfIndex() * GPUMultiplier;

	// decide on the actual quality needed
	Results.ResolutionQuality = GetRenderScaleLevelFromQualityLevel(ComputeOptionFromPerfIndex(TEXT("ResolutionQuality"), CPUPerfIndex, GPUPerfIndex));
	Results.ViewDistanceQuality = ComputeOptionFromPerfIndex(TEXT("ViewDistanceQuality"), CPUPerfIndex, GPUPerfIndex);
	Results.AntiAliasingQuality = ComputeOptionFromPerfIndex(TEXT("AntiAliasingQuality"), CPUPerfIndex, GPUPerfIndex);
	Results.ShadowQuality = ComputeOptionFromPerfIndex(TEXT("ShadowQuality"), CPUPerfIndex, GPUPerfIndex);
	Results.PostProcessQuality = ComputeOptionFromPerfIndex(TEXT("PostProcessQuality"), CPUPerfIndex, GPUPerfIndex);
	Results.TextureQuality = ComputeOptionFromPerfIndex(TEXT("TextureQuality"), CPUPerfIndex, GPUPerfIndex);
	Results.EffectsQuality = ComputeOptionFromPerfIndex(TEXT("EffectsQuality"), CPUPerfIndex, GPUPerfIndex);
	Results.CPUBenchmarkResults = CPUPerfIndex;
	Results.GPUBenchmarkResults = GPUPerfIndex;

	return Results;
}

static void PrintGroupInfo(const TCHAR* InGroupName, bool bInInfoMode)
{
	int32 QualityLevel = -1;
	TArray<FString> CVars;
	InferCurrentQualityLevel(InGroupName, QualityLevel, &CVars);

	FString GroupQualityLevelDisplayName = QualityLevel == -1 ? TEXT("(custom)") : FString::FromInt(QualityLevel);

	UE_LOG(LogConsoleResponse, Display, TEXT("  %s (0..3): %s"), InGroupName, *GroupQualityLevelDisplayName);

	if (bInInfoMode)
	{
		if (QualityLevel != -1)
		{
			for (int32 CVarDataIndex = 0; CVarDataIndex < CVars.Num(); ++CVarDataIndex)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("    %s"), *CVars[CVarDataIndex]);
			}
		}
	}
}

void ProcessCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bPrintUsage = true;
	bool bPrintCurrentSettings = true;
	bool bInfoMode = false;
	FString Token;

	float CPUBenchmarkValue = -1.0f;
	float GPUBenchmarkValue = -1.0f;

	// Parse command line
	if (FParse::Token(Cmd, Token, true))
	{
		if (Token == TEXT("auto"))
		{
			FQualityLevels State = Scalability::BenchmarkQualityLevels();
			Scalability::SetQualityLevels(State);
			Scalability::SaveState(GIsEditor ? GEditorSettingsIni : GGameUserSettingsIni);
			bPrintUsage = false;
			bPrintCurrentSettings = true;
			CPUBenchmarkValue = State.CPUBenchmarkResults;
			GPUBenchmarkValue = State.GPUBenchmarkResults;
		}
		else if (Token == TEXT("reapply"))
		{
			FQualityLevels State = Scalability::GetQualityLevels();
			Scalability::SetQualityLevels(State);
			bPrintUsage = false;
		}
		else if (Token.IsNumeric())
		{
			FQualityLevels QualityLevels;

			int32 RequestedQualityLevel = FCString::Atoi(*Token);
			QualityLevels.SetFromSingleQualityLevel(RequestedQualityLevel);
			SetQualityLevels(QualityLevels);
			Scalability::SaveState(GIsEditor ? GEditorSettingsIni : GGameUserSettingsIni);

			bPrintUsage = false;
		}
		else
		{
			UE_LOG(LogConsoleResponse, Error, TEXT("Scalability unknown parameter"));
			bPrintCurrentSettings = false;
		}
	}

	if (bPrintUsage)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Scalability Usage:"));
		UE_LOG(LogConsoleResponse, Display, TEXT("  \"Scalability\" (Print scalability usage and information)"));
		UE_LOG(LogConsoleResponse, Display, TEXT("  \"Scalability [0..3]\" (Set all scalability groups to the specified quality level and save state)"));
		UE_LOG(LogConsoleResponse, Display, TEXT("  \"Scalability reapply\" (apply the state of the scalability group (starting with 'sg.') console variables)"));
		UE_LOG(LogConsoleResponse, Display, TEXT("  \"Scalability auto\" (Run synth benchmark and adjust the scalability levels for your system and save state)"));
	}

	if (bPrintCurrentSettings)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Current Scalability Settings:"));

		PrintGroupInfo(TEXT("ResolutionQuality"), bInfoMode);
		PrintGroupInfo(TEXT("ViewDistanceQuality"), bInfoMode);
		PrintGroupInfo(TEXT("AntiAliasingQuality"), bInfoMode);
		PrintGroupInfo(TEXT("ShadowQuality"), bInfoMode);
		PrintGroupInfo(TEXT("PostProcessQuality"), bInfoMode);
		PrintGroupInfo(TEXT("TextureQuality"), bInfoMode);
		PrintGroupInfo(TEXT("EffectsQuality"), bInfoMode);

		if (CPUBenchmarkValue >= 0.0f)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("CPU benchmark value: %f"), CPUBenchmarkValue);
		}
		if (GPUBenchmarkValue >= 0.0f)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("GPU benchmark value: %f"), GPUBenchmarkValue);
		}
	}
}

void SetQualityLevels(const FQualityLevels& QualityLevels)
{
	CVarResolutionQuality.AsVariable()->Set(QualityLevels.ResolutionQuality, ECVF_SetByScalability);
	CVarViewDistanceQuality.AsVariable()->Set(QualityLevels.ViewDistanceQuality, ECVF_SetByScalability);
	CVarAntiAliasingQuality.AsVariable()->Set(QualityLevels.AntiAliasingQuality, ECVF_SetByScalability);
	CVarShadowQuality.AsVariable()->Set(QualityLevels.ShadowQuality, ECVF_SetByScalability);
	CVarPostProcessQuality.AsVariable()->Set(QualityLevels.PostProcessQuality, ECVF_SetByScalability);
	CVarTextureQuality.AsVariable()->Set(QualityLevels.TextureQuality, ECVF_SetByScalability);
	CVarEffectsQuality.AsVariable()->Set(QualityLevels.EffectsQuality, ECVF_SetByScalability);
}

FQualityLevels GetQualityLevels()
{
	FQualityLevels Ret;

	// only suggested way to get the current state - don't get CVars directly
	Ret.ResolutionQuality = CVarResolutionQuality.GetValueOnGameThread();
	Ret.ViewDistanceQuality = CVarViewDistanceQuality.GetValueOnGameThread();
	Ret.AntiAliasingQuality = CVarAntiAliasingQuality.GetValueOnGameThread();
	Ret.ShadowQuality = CVarShadowQuality.GetValueOnGameThread();
	Ret.PostProcessQuality = CVarPostProcessQuality.GetValueOnGameThread();
	Ret.TextureQuality = CVarTextureQuality.GetValueOnGameThread();
	Ret.EffectsQuality = CVarEffectsQuality.GetValueOnGameThread();

	return Ret;
}

void FQualityLevels::SetBenchmarkFallback()
{
	GetRenderScaleLevelFromQualityLevel(2);
	ResolutionQuality = 100.0f;
}

void FQualityLevels::SetDefaults()
{
	SetFromSingleQualityLevel(3);
}

void FQualityLevels::SetFromSingleQualityLevel(int32 Value)
{
	// clamp in the range "low" to "epic"
	Value = FMath::Clamp(Value, 0, 3);

	ResolutionQuality = GetRenderScaleLevelFromQualityLevel(Value);
	ViewDistanceQuality = Value;
	AntiAliasingQuality = Value;
	ShadowQuality = Value;
	PostProcessQuality = Value;
	TextureQuality = Value;
	EffectsQuality = Value;
}

// Returns the overall value if all settings are set to the same thing
// @param Value -1:custom 0:low, 1:medium, 2:high, 3:epic
int32 FQualityLevels::GetSingleQualityLevel() const
{
	int32 Result = ViewDistanceQuality;

	const int32 Target = ViewDistanceQuality;
	if ((Target == AntiAliasingQuality) && (Target == ShadowQuality) && (Target == PostProcessQuality) && (Target == TextureQuality) && (Target == EffectsQuality))
	{
		if (GetRenderScaleLevelFromQualityLevel(Target) == ResolutionQuality)
		{
			return Target;
		}
	}

	return -1;
}

void LoadState(const FString& IniName)
{
	check(!IniName.IsEmpty());

	// todo: could be done earlier
	InitScalabilitySystem();

	FQualityLevels State;

	const TCHAR* Section = TEXT("ScalabilityGroups");

	// looks like cvars but here we just use the name for the ini
	GConfig->GetFloat(Section, TEXT("sg.ResolutionQuality"), State.ResolutionQuality, IniName);
	GConfig->GetInt(Section, TEXT("sg.ViewDistanceQuality"), State.ViewDistanceQuality, IniName);
	GConfig->GetInt(Section, TEXT("sg.AntiAliasingQuality"), State.AntiAliasingQuality, IniName);
	GConfig->GetInt(Section, TEXT("sg.ShadowQuality"), State.ShadowQuality, IniName);
	GConfig->GetInt(Section, TEXT("sg.PostProcessQuality"), State.PostProcessQuality, IniName);
	GConfig->GetInt(Section, TEXT("sg.TextureQuality"), State.TextureQuality, IniName);
	GConfig->GetInt(Section, TEXT("sg.EffectsQuality"), State.EffectsQuality, IniName);

	SetQualityLevels(State);
}

void SaveState(const FString& IniName)
{
	check(!IniName.IsEmpty());

	FQualityLevels State = GetQualityLevels();

	const TCHAR* Section = TEXT("ScalabilityGroups");

	// looks like cvars but here we just use the name for the ini
	GConfig->SetFloat(Section, TEXT("sg.ResolutionQuality"), State.ResolutionQuality, IniName);
	GConfig->SetInt(Section, TEXT("sg.ViewDistanceQuality"), State.ViewDistanceQuality, IniName);
	GConfig->SetInt(Section, TEXT("sg.AntiAliasingQuality"), State.AntiAliasingQuality, IniName);
	GConfig->SetInt(Section, TEXT("sg.ShadowQuality"), State.ShadowQuality, IniName);
	GConfig->SetInt(Section, TEXT("sg.PostProcessQuality"), State.PostProcessQuality, IniName);
	GConfig->SetInt(Section, TEXT("sg.TextureQuality"), State.TextureQuality, IniName);
	GConfig->SetInt(Section, TEXT("sg.EffectsQuality"), State.EffectsQuality, IniName);
}

void RecordQualityLevelsAnalytics(bool bAutoApplied)
{
	if( FEngineAnalytics::IsAvailable() )
	{
		FQualityLevels State = GetQualityLevels();

		TArray<FAnalyticsEventAttribute> Attributes;

		Attributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionQuality"), State.ResolutionQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("ViewDistanceQuality"), State.ViewDistanceQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("AntiAliasingQuality"), State.AntiAliasingQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("ShadowQuality"), State.ShadowQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("PostProcessQuality"), State.PostProcessQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("TextureQuality"), State.TextureQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("EffectsQuality"), State.EffectsQuality));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("AutoAppliedSettings"), bAutoApplied));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Performance.ScalabiltySettings"), Attributes);
	}
}

}



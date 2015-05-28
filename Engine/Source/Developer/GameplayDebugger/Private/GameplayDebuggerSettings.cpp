// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivate.h"
#include "Misc/CoreMisc.h"
#include "GameplayDebuggingComponent.h"
#include "GameplayDebuggingHUDComponent.h"
#include "GameplayDebuggingReplicator.h"
#include "GameplayDebuggingControllerComponent.h"
#include "AISystem.h"
#include "GameplayDebuggerSettings.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#endif // WITH_EDITOR

UGameplayDebuggerSettings::UGameplayDebuggerSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		/*read base default values from Engine config file. It can be overridden (locally) by Editor settings*/
		GConfig->GetString(TEXT("GameplayDebuggerSettings"), TEXT("NameForGameView1"), CustomViewNames.GameView1, GEngineIni);
		GConfig->GetString(TEXT("GameplayDebuggerSettings"), TEXT("NameForGameView2"), CustomViewNames.GameView2, GEngineIni);
		GConfig->GetString(TEXT("GameplayDebuggerSettings"), TEXT("NameForGameView3"), CustomViewNames.GameView3, GEngineIni);
		GConfig->GetString(TEXT("GameplayDebuggerSettings"), TEXT("NameForGameView4"), CustomViewNames.GameView4, GEngineIni);
		GConfig->GetString(TEXT("GameplayDebuggerSettings"), TEXT("NameForGameView5"), CustomViewNames.GameView5, GEngineIni);
		GConfig->GetString(TEXT("GameplayDebuggerSettings"), TEXT("NameForGameView5"), CustomViewNames.GameView5, GEngineIni);
		
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("OverHead"), OverHead, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("Basic"), Basic, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("BehaviorTree"), BehaviorTree, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("EQS"), EQS, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("EnableEQSOnHUD"), EnableEQSOnHUD, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("Perception"), Perception, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("GameView1"), GameView1, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("GameView2"), GameView2, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("GameView3"), GameView3, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("GameView4"), GameView4, GEngineIni);
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("GameView5"), GameView5, GEngineIni);

#if ADD_LEVEL_EDITOR_EXTENSIONS
		bExtendViewportMenu = false;
		GConfig->GetBool(TEXT("GameplayDebuggerSettings"), TEXT("bExtendViewportMenu"), bExtendViewportMenu, GEngineIni);
#endif
	}
}

#if WITH_EDITOR
void UGameplayDebuggerSettings::PostInitProperties()
{
	Super::PostInitProperties();

#define UPDATE_GAMEVIEW_DISPLAYNAME(__GameView__) \
			{\
		UProperty* Prop = FindField<UProperty>(UGameplayDebuggerSettings::StaticClass(), TEXT(#__GameView__));\
		if (Prop)\
				{\
			Prop->SetPropertyFlags(CPF_Edit);\
			Prop->SetMetaData(TEXT("DisplayName"), CustomViewNames.__GameView__.Len() > 0 ? *CustomViewNames.__GameView__ : TEXT(#__GameView__)); \
		}\
	}
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView1);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView2);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView3);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView4);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView5);
#undef UPDATE_GAMEVIEW_DISPLAYNAME
}

void UGameplayDebuggerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

#define UPDATE_GAMEVIEW_DISPLAYNAME(__GameView__) \
	if (MemberPropertyName == (TEXT("CustomViewNames"))) \
	{\
		if (Name == FName(TEXT(#__GameView__)) ) \
		{ \
			UProperty* Prop = FindField<UProperty>(UGameplayDebuggerSettings::StaticClass(), *Name.ToString());\
			check(Prop);\
			Prop->SetPropertyFlags(CPF_Edit); \
			Prop->SetMetaData(TEXT("DisplayName"), CustomViewNames.__GameView__.Len() > 0 ? *CustomViewNames.__GameView__ : TEXT(#__GameView__)); \
		}\
	}

	UPDATE_GAMEVIEW_DISPLAYNAME(GameView1);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView2);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView3);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView4);
	UPDATE_GAMEVIEW_DISPLAYNAME(GameView5);
#undef UPDATE_GAMEVIEW_DISPLAYNAME

	SaveConfig();

	SettingChangedEvent.Broadcast(Name);
}
#endif

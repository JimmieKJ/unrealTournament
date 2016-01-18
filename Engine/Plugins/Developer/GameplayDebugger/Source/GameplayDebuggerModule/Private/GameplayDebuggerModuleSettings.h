// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GameplayDebuggerSettings.h: Declares the UGameplayDebuggerSettings class.
=============================================================================*/
#pragma once

#include "EngineMinimal.h"
#include "InputCore.h"
#include "GameFramework/PlayerInput.h"
#include "GameplayDebuggerModuleSettings.generated.h"

/** An Input Chord is a key and the modifier keys that are to be held with it. */
USTRUCT()
struct FInputConfig
{
	GENERATED_USTRUCT_BODY()

	/** The Key is the core of the chord. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FKey Key;

	/** Whether the shift key is part of the chord.  */
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bShift : 1;

	/** Whether the control key is part of the chord.  */
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bCtrl : 1;

	/** Whether the alt key is part of the chord.  */
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bAlt : 1;

	/** Whether the command key is part of the chord.  */
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bCmd : 1;

	FInputConfig() {}

	FInputConfig(const FKey InKey, const bool bInShift, const bool bInCtrl, const bool bInAlt, const bool bInCmd)
		: Key(InKey)
		, bShift(bInShift)
		, bCtrl(bInCtrl)
		, bAlt(bInAlt)
		, bCmd(bInCmd)
	{
	}

	operator FInputChord() { return FInputChord(Key, !!bShift, !!bCtrl, !!bAlt, !!bCmd); }
};

USTRUCT()
struct FActionInputConfig : public FInputActionKeyMapping
{
	GENERATED_USTRUCT_BODY()
};

USTRUCT()
struct FInputConfiguration
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig SelectPlayer;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig ZeroCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig OneCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig TwoCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig ThreeCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig FourCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig FiveCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig SixCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig SevenCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig EightCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig NineCategory;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig NextMenuLine;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig PreviousMenuLine;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig DebugCamera;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig OnScreenDebugMessages;

	UPROPERTY(EditAnywhere, config, Category = "AlternateInput")
	FInputConfig GameHUD;
};

class UGameplayDebuggerBaseObject;

USTRUCT()
struct FGameplayDebuggerClassSettings
{
	GENERATED_USTRUCT_BODY()

	FGameplayDebuggerClassSettings() {}

	FGameplayDebuggerClassSettings(const FString& InClassName, const FString& InClassPath) : ClassName(InClassName), ClassPath(InClassPath) {}

	UPROPERTY(VisibleAnywhere, config, Category = "Classes")
	FString ClassName;

	UPROPERTY(VisibleAnywhere, config, Category = "Classes")
	FString ClassPath;
};

USTRUCT()
struct FGameplayDebuggerCategorySettings
{
	GENERATED_USTRUCT_BODY()

	FGameplayDebuggerCategorySettings() {}

	FGameplayDebuggerCategorySettings(const FString& InCategoryName, bool InSimulate, bool InPIE) : CategoryName(InCategoryName), bSimulate(InSimulate), bPIE(InPIE) {}

	UPROPERTY(VisibleAnywhere, config, Category = "Categories")
	FString CategoryName;

	UPROPERTY(EditAnywhere, config, Category = "Categories")
	uint32 bSimulate : 1;

	UPROPERTY(EditAnywhere, config, Category = "Categories")
	uint32 bPIE : 1;

	UPROPERTY(EditAnywhere, config, Category = "Classes")
	TArray<FGameplayDebuggerClassSettings> ArrayOfClasses;
};

UCLASS(config = GameplayDebugger, defaultconfig)
class UGameplayDebuggerModuleSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Canvas")
	FVector2D MenuStart;

	UPROPERTY(EditAnywhere, config, Category = "Canvas")
	FVector2D DebugInfoStart;

	UPROPERTY(config, EditAnywhere, Category = "Input")
	FActionInputConfig ActivationAction;

	UPROPERTY(config)
	bool bUseAlternateKeys;

	UPROPERTY(config)
	bool bDoNotUseAlternateKeys;

	UPROPERTY(EditAnywhere, config, Category = "Defaults")
	bool bHighlightSelectedActor;

	UPROPERTY(EditAnywhere, config, Category = "Input", meta = (EditCondition = "bDoNotUseAlternateKeys"))
	FInputConfiguration InputConfiguration;

	UPROPERTY(EditAnywhere, config, Category = "Input", meta = (EditCondition = "bUseAlternateKeys"))
	FInputConfiguration AlternateInputConfiguration;
	
public:
#if WITH_EDITOR
	// Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject Interface
#endif

	virtual void BeginDestroy() override;

	DECLARE_EVENT_OneParam(UGameplayDebuggerModuleSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged() { return SettingChangedEvent; }

	bool UseAlternateKeys() { return bUseAlternateKeys; }

	FInputConfiguration& GetInputConfiguration() { return bUseAlternateKeys ? AlternateInputConfiguration : InputConfiguration; }

	const TArray<FGameplayDebuggerCategorySettings>& GetCategories();

	const TArray<UClass*> GetAllGameplayDebuggerClasses();

	void RegisterClass(UClass* InClass);

	void RegisterClass(class UGameplayDebuggerBaseObject* InClass);

	void UnregisterClass(class UGameplayDebuggerBaseObject* BaseObject);

	void LoadAnyMissingClasses();

	void UpdateCategories(bool bUpdateConfig = true);
protected:
	void UpdateConfigFileWithCheckout();

	UPROPERTY(EditAnywhere, Category = "Defaults")
	bool bRegenerateConfiguration;

	UPROPERTY(EditAnywhere, config, EditFixedSize, NoClear, Category = "Defaults")
	TArray<FGameplayDebuggerCategorySettings> Categories;

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;

	TArray<UClass*> ManualClasses;
};

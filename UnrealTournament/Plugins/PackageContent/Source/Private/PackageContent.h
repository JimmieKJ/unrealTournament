// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LevelEditor.h"
#include "Editor/ClassViewer/Public/ClassViewerModule.h"
#include "Editor/ClassViewer/Public/ClassViewerFilter.h"
#include "UTWeapon.h"
#include "UTHat.h"
#include "UTTaunt.h"
#include "UTCharacterContent.h"
#include "UTCrosshair.h"
#include "UTMutator.h"

class FPackageContent : public TSharedFromThis< FPackageContent >
{
public:
	~FPackageContent();

	static TSharedRef< FPackageContent > Create();

	void PackageWeapon(UClass* WeaponClass);
	void PackageHat(UClass* HatClass);
	void PackageTaunt(UClass* TauntClass);
	void PackageMutator(UClass* MutatorClass);
	void PackageCharacter(UClass* CharacterClass);
	void PackageCrosshair(UClass* CrosshairClass);

	void CreateUATTask(const FString& CommandLine, const FString& DLCName, const FText& TaskName, const FText &TaskShortName, const FSlateBrush* TaskIcon);
private:
	FPackageContent(const TSharedRef< FUICommandList >& InActionList);
	void Initialize();

	void PackageDLC(const FString& DLCName, const FText& TaskName, const FText& TaskShortName);

	void OpenPackageLevelWindow();
	void OpenPackageWeaponWindow();
	void OpenPackageHatWindow();
	void OpenPackageTauntWindow();
	void OpenPackageMutatorWindow();
	void OpenPackageCharacterWindow();
	void OpenPackageCrosshairWindow();
	
	void CreatePackageContentMenu(FToolBarBuilder& Builder);
	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);
	TSharedRef< SWidget > GenerateOpenPackageMenuContent();

	TSharedRef< FUICommandList > ActionList;

	TSharedPtr< FExtender > MenuExtender;

	struct EventData
	{
		FString EventName;
		FString DLCName;
		double StartTime;
	};

	// Handles clicking the packager notification item's Cancel button.
	static void HandleUatCancelButtonClicked(TSharedPtr<FMonitoredProcess> PackagerProcess);

	// Handles clicking the hyper link on a packager notification item.
	static void HandleUatHyperlinkNavigate();

	// Handles canceled packager processes.
	static void HandleUatProcessCanceled(TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName, EventData Event);

	// Handles the completion of a packager process.
	static void HandleUatProcessCompleted(int32 ReturnCode, TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName, EventData Event);

	// Handles packager process output.
	static void HandleUatProcessOutput(FString Output, TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName);

	/** A default window size for the package dialog */
	static const FVector2D DEFAULT_WINDOW_SIZE;

	TAttribute<FText> PackageDialogTitle;
};

class SPackageContentDialog : public SCompoundWidget
{
public:
	enum EPackageContentDialogMode
	{
		PACKAGE_Weapon,
		PACKAGE_Hat,
		PACKAGE_Character,
		PACKAGE_Taunt,
		PACKAGE_Crosshair,
		PACKAGE_Mutator,
	};

	SLATE_BEGIN_ARGS(SPackageContentDialog)
		: _PackageContent(),
		_DialogMode(PACKAGE_Weapon)
		{}
		SLATE_ARGUMENT(TSharedPtr<FPackageContent>, PackageContent)
		SLATE_ARGUMENT(EPackageContentDialogMode, DialogMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow);

	void ClassChosen(UClass* ChosenClass);

protected:
	TSharedPtr<FPackageContent> PackageContent;

	TSharedPtr<SWindow> ParentWindow;
	EPackageContentDialogMode DialogMode;
};

class FHatClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (NULL != InClass)
		{
			const bool bCosmeticBased = InClass->IsChildOf(AUTCosmetic::StaticClass());
			const bool bBlueprintType = InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			return bCosmeticBased && bBlueprintType;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bCosmeticBased = InUnloadedClassData->IsChildOf(AUTCosmetic::StaticClass());
		const bool bBlueprintType = InUnloadedClassData->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		return bCosmeticBased && bBlueprintType;
	}
};

class FWeaponClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (NULL != InClass)
		{
			const bool bWeaponBased = InClass->IsChildOf(AUTWeapon::StaticClass());
			const bool bBlueprintType = InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			return bWeaponBased && bBlueprintType;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bWeaponBased = InUnloadedClassData->IsChildOf(AUTWeapon::StaticClass());
		const bool bBlueprintType = InUnloadedClassData->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		return bWeaponBased && bBlueprintType;
	}
};

class FTauntClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (NULL != InClass)
		{
			const bool bTauntBased = InClass->IsChildOf(AUTTaunt::StaticClass());
			const bool bBlueprintType = InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			return bTauntBased && bBlueprintType;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bTauntBased = InUnloadedClassData->IsChildOf(AUTTaunt::StaticClass());
		const bool bBlueprintType = InUnloadedClassData->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		return bTauntBased && bBlueprintType;
	}
};

class FCharacterClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (NULL != InClass)
		{
			const bool bCharacterContentBased = InClass->IsChildOf(AUTCharacterContent::StaticClass());
			const bool bBlueprintType = InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			return bCharacterContentBased && bBlueprintType;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bCharacterContentBased = InUnloadedClassData->IsChildOf(AUTCharacterContent::StaticClass());
		const bool bBlueprintType = InUnloadedClassData->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		return bCharacterContentBased && bBlueprintType;
	}
};

class FCrosshairClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (NULL != InClass)
		{
			const bool bCrosshairContentBased = InClass->IsChildOf(UUTCrosshair::StaticClass());
			const bool bBlueprintType = InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			return bCrosshairContentBased && bBlueprintType;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bCrosshairContentBased = InUnloadedClassData->IsChildOf(UUTCrosshair::StaticClass());
		const bool bBlueprintType = InUnloadedClassData->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		return bCrosshairContentBased && bBlueprintType;
	}
};

class FMutatorClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (NULL != InClass)
		{
			const bool bMutatorContentBased = InClass->IsChildOf(AUTMutator::StaticClass());
			const bool bBlueprintType = InClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			return bMutatorContentBased && bBlueprintType;
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bMutatorContentBased = InUnloadedClassData->IsChildOf(AUTMutator::StaticClass());
		const bool bBlueprintType = InUnloadedClassData->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		return bMutatorContentBased && bBlueprintType;
	}
};
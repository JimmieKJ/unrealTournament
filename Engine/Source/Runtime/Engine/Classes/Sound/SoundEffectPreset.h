// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#if WITH_EDITOR
#include "Widgets/SWidget.h"
#include "Developer/AssetTools/Public/AssetTypeCategories.h"
#include "Developer/AssetTools/Public/IAssetTypeActions.h"
#endif
#include "SoundEffectPreset.generated.h"

class FAssetData;
class FMenuBuilder;
class IToolkitHost;
class USoundEffectPreset;

#if WITH_EDITOR
class USoundEffectPreset;

class FAssetTypeActions_SoundEffectPreset : public IAssetTypeActions
{
public:
	FAssetTypeActions_SoundEffectPreset(USoundEffectPreset* InEffectPreset);

	// IAssetTypeActions Implementation
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual void AssetsActivated(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override;
	virtual bool CanFilter() override;
	virtual bool CanLocalize() const override;
	virtual bool CanMerge() const override;
	virtual void Merge(UObject* InObject) override;
	virtual void Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset, const FOnMergeResolved& ResolutionCallback) override;
	virtual bool ShouldForceWorldCentric() override;
	virtual void PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override;
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
	virtual FText GetAssetDescription(const class FAssetData& AssetData) const override;
	virtual bool IsImportedAsset() const override;
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return FColor(20.0f, 20.0f, 20.0f); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }

private:

	USoundEffectPreset* EffectPreset;
};
#endif

/** Raw data container for arbitrary preset UStructs */
struct FPresetSettings
{
	void* Data;
	uint32 Size;
	UScriptStruct* Struct;

	FPresetSettings()
		: Data(nullptr)
		, Size(0)
		, Struct(nullptr)
	{}
};

UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectPreset : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual void* GetSettings() { return nullptr; }
	virtual uint32 GetSettingsSize() const { return 0; }
	virtual UScriptStruct* GetSettingsStruct() const { return nullptr; }
	virtual UClass* GetEffectClass() const { return nullptr; }
	virtual FText GetAssetActionName() const { return FText(); }

	void RegisterAssetTypeAction();
	
	const FPresetSettings& GetPresetSettings() const
	{
		return PresetSettings;
	}

private:
	FPresetSettings PresetSettings;
};



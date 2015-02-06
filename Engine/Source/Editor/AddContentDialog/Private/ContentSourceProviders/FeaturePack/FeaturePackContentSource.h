// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPakPlatformFile;

/** A content source which represents a content upack. */
class FFeaturePackContentSource : public IContentSource
{
public:
	FFeaturePackContentSource(FString InFeaturePackPath);

	virtual TArray<FLocalizedText> GetLocalizedNames() override;
	virtual TArray<FLocalizedText> GetLocalizedDescriptions() override;
	
	virtual EContentSourceCategory GetCategory() override;
	virtual TArray<FLocalizedText> GetLocalizedAssetTypes() override;
	virtual FString GetClassTypesUsed() override;

	virtual TSharedPtr<FImageData> GetIconData() override;
	virtual TArray<TSharedPtr<FImageData>> GetScreenshotData() override;

	virtual bool InstallToProject(FString InstallPath) override;
	virtual bool IsDataValid() const override;
	
	virtual ~FFeaturePackContentSource();
	
private:
	bool LoadPakFileToBuffer(FPakPlatformFile& PakPlatformFile, FString Path, TArray<uint8>& Buffer);
	FString GetFocusAssetName() const;

private:
	FString FeaturePackPath;
	TArray<FLocalizedText> LocalizedNames;
	TArray<FLocalizedText> LocalizedDescriptions;
	EContentSourceCategory Category;
	TSharedPtr<FImageData> IconData;
	TArray<TSharedPtr<FImageData>> ScreenshotData;
	TArray<FLocalizedText> LocalizedAssetTypesList;
	FString ClassTypes;
	bool bPackValid;
	FString FocusAssetIdent;
};
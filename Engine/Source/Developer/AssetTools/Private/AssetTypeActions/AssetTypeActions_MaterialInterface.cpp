// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Editor/MaterialEditor/Public/MaterialEditorModule.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "AssetToolsModule.h"
#include "Materials/MaterialInstanceConstant.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_MaterialInterface::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto MaterialInterfaces = GetTypedWeakObjectPtrs<UMaterialInterface>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Material_NewMIC", "Create Material Instance"),
		LOCTEXT("Material_NewMICTooltip", "Creates a parameterized material using this material as a base."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.MaterialInstanceActor"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_MaterialInterface::ExecuteNewMIC, MaterialInterfaces )
			)
		);
}

UThumbnailInfo* FAssetTypeActions_MaterialInterface::GetThumbnailInfo(UObject* Asset) const
{
	UMaterialInterface* MaterialInterface = CastChecked<UMaterialInterface>(Asset);
	UThumbnailInfo* ThumbnailInfo = MaterialInterface->ThumbnailInfo;
	if ( ThumbnailInfo == NULL )
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfoWithPrimitive>(MaterialInterface);
		MaterialInterface->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_MaterialInterface::ExecuteNewMIC(TArray<TWeakObjectPtr<UMaterialInterface>> Objects)
{
	const FString DefaultSuffix = TEXT("_Inst");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Create an appropriate and unique name 
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialInstanceConstant::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
				Factory->InitialParent = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialInstanceConstant::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE
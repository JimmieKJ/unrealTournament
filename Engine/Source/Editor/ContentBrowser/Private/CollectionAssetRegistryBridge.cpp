// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "CollectionAssetRegistryBridge.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

/** The collection manager doesn't know how to follow redirectors, this class provides it with that knowledge */
class FCollectionRedirectorFollower : public ICollectionRedirectorFollower
{
public:
	FCollectionRedirectorFollower()
		: AssetRegistryModule(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
	}

	virtual bool FixupObject(const FName& InObjectPath, FName& OutNewObjectPath) override
	{
		OutNewObjectPath = NAME_None;

		if (InObjectPath.ToString().StartsWith(TEXT("/Script/")))
		{
			// We can't use FindObject while we're saving
			if (!GIsSavingPackage)
			{
				const FString ClassPathStr = InObjectPath.ToString();

				UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ClassPathStr);
				if (!FoundClass)
				{
					// Use the linker to search for class name redirects (from the loaded ActiveClassRedirects)
					const FString ClassName = FPackageName::ObjectPathToObjectName(ClassPathStr);
					const FName NewClassName = FLinkerLoad::FindNewNameForClass(*ClassName, false);

					if (!NewClassName.IsNone())
					{
						// Our new class name might be lacking the path, so try and find it so we can use the full path in the collection
						FoundClass = FindObject<UClass>(ANY_PACKAGE, *NewClassName.ToString());
						if (FoundClass)
						{
							OutNewObjectPath = *FoundClass->GetPathName();
						}
					}
				}
			}
		}
		else
		{
			// Use the asset registry to avoid loading the object
			FAssetData ObjectAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(InObjectPath);
			while (ObjectAssetData.IsValid() && ObjectAssetData.IsRedirector())
			{
				// Get the destination object from the meta-data rather than load the redirector object, as 
				// loading a redirector will also load the object it points to, which could cause a large hitch
				const FString* DestinationObjectStrPtr = ObjectAssetData.TagsAndValues.Find("DestinationObject");
				if (DestinationObjectStrPtr)
				{
					FString DestinationObjectPath = *DestinationObjectStrPtr;
					ConstructorHelpers::StripObjectClass(DestinationObjectPath);
					ObjectAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*DestinationObjectPath);
				}
				else
				{
					ObjectAssetData = FAssetData();
				}
			}

			OutNewObjectPath = ObjectAssetData.ObjectPath;
		}

		return OutNewObjectPath != NAME_None && InObjectPath != OutNewObjectPath;
	}

private:
	FAssetRegistryModule& AssetRegistryModule;
};

FCollectionAssetRegistryBridge::FCollectionAssetRegistryBridge()
{
	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FCollectionAssetRegistryBridge::OnAssetRemoved);
	AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FCollectionAssetRegistryBridge::OnAssetRenamed);
	
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FCollectionAssetRegistryBridge::OnAssetRegistryLoadComplete);
	}
	else
	{
		OnAssetRegistryLoadComplete();
	}
}

FCollectionAssetRegistryBridge::~FCollectionAssetRegistryBridge()
{
	// Load the asset registry module to unregister delegates
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
		AssetRegistryModule.Get().OnAssetRenamed().RemoveAll(this);
		AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);
	}
}

void FCollectionAssetRegistryBridge::OnAssetRegistryLoadComplete()
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// We've found all the assets, let the collections manager fix up its references now so that it doesn't reference any redirectors
	FCollectionRedirectorFollower RedirectorFollower;
	CollectionManagerModule.Get().HandleFixupRedirectors(RedirectorFollower);
}

void FCollectionAssetRegistryBridge::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// Notify the collections manager that an asset has been renamed
	CollectionManagerModule.Get().HandleObjectRenamed(*OldObjectPath, AssetData.ObjectPath);
}

void FCollectionAssetRegistryBridge::OnAssetRemoved(const FAssetData& AssetData)
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	if (AssetData.IsRedirector())
	{
		// Notify the collections manager that a redirector has been removed
		// This will attempt to re-save any collections that still have a reference to this redirector in their on-disk collection data
		CollectionManagerModule.Get().HandleRedirectorDeleted(AssetData.ObjectPath);
	}
	else
	{
		// Notify the collections manager that an asset has been removed
		CollectionManagerModule.Get().HandleObjectDeleted(AssetData.ObjectPath);
	}
}

#undef LOCTEXT_NAMESPACE

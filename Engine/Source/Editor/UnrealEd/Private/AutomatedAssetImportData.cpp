// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomatedAssetImportData.h"
#include "Factories/Factory.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY(LogAutomatedImport);

UAutomatedAssetImportData::UAutomatedAssetImportData()
	: GroupName()
	, bReplaceExisting(false)
	, bSkipReadOnly(false)
{

}

bool UAutomatedAssetImportData::IsValid() const
{
	// This data is valid if there is at least one filename to import, there is a valid destination path
	// and either no factory was supported (automatic factory picking) or a valid factory was found
	return Filenames.Num() > 0 && !DestinationPath.IsEmpty() && (FactoryName.IsEmpty() || Factory != nullptr);
}

void UAutomatedAssetImportData::Initialize()
{
	if(Filenames.Num() > 0)
	{
		// IF a factory doesn't have a vaild full path assume it is an unreal internal factory
		if(!FactoryName.IsEmpty() && !FactoryName.StartsWith("/Script/"))
		{
			FactoryName = TEXT("/Script/UnrealEd.") + FactoryName;
		}

		if(!FactoryName.IsEmpty())
		{
			UClass* FactoryClass = LoadClass<UObject>(nullptr, *FactoryName, nullptr, LOAD_None, nullptr);
			if(FactoryClass)
			{
				UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);

				if(NewFactory->bEditorImport)
				{
					// Check that every file can be imported
					TArray<FString> InvalidFilesForFactory;
					for(const FString& Filename : Filenames)
					{
						if(!NewFactory->FactoryCanImport(Filename))
						{
							InvalidFilesForFactory.Add(Filename);
						}
					}

					if(InvalidFilesForFactory.Num() > 0)
					{
						UE_LOG(LogAutomatedImport, Error, TEXT("Factory %s could not import one or more of the source files"), *FactoryName);
						for(const FString& InvalidFiles : InvalidFilesForFactory)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("    %s"), *InvalidFiles);
						}
					}
					else
					{
						// All files are valid. Use this factory
						Factory = NewFactory;
					}
				}
			}
			else
			{
				UE_LOG(LogAutomatedImport, Error, TEXT("Factory %s could not be found"), *FactoryName);
			}
		}
		else
		{
			UE_LOG(LogAutomatedImport, Log, TEXT("Factory was not specified, will be set automatically"));
		}
	}

	if(!DestinationPath.IsEmpty() && !DestinationPath.StartsWith("/Game"))
	{
		DestinationPath = TEXT("/Game") / DestinationPath;
	}

	FString PackagePath;
	FString FailureReason;
	if(!FPackageName::TryConvertFilenameToLongPackageName(DestinationPath, PackagePath, &FailureReason))
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Invalid Destination Path (%s): %s"), *DestinationPath, *FailureReason);
		DestinationPath.Empty();
	}
	else
	{
		// Package path is what we will use for importing.  So use that as the dest path now that it has been created
		DestinationPath = PackagePath;
	}
}

FString UAutomatedAssetImportData::GetDisplayName() const
{
	return GroupName.Len() > 0 ? GroupName : GetName();
}

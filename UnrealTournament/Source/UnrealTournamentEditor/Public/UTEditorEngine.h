// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "UTEditorEngine.generated.h"

UCLASS(CustomConstructor)
class UUTEditorEngine : public UEditorEngine
{
	GENERATED_UCLASS_BODY()

	UUTEditorEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	// hacky location for this because InitEditor() isn't virtual
	virtual void InitializeObjectReferences() override
	{
		Super::InitializeObjectReferences();

		if (FCoreUObjectDelegates::StringAssetReferenceLoaded.IsBound())
		{
			// manually add FStringClassReferences and FStringAssetReferences in native UClasses to the cook list
			// this will not happen by default because that is done via Serialize() which does not happen for native CDOs
			UScriptStruct* FStringClassReferenceStruct = FindObject<UScriptStruct>(NULL, TEXT("/Script/CoreUObject.StringClassReference"), true);
			UScriptStruct* FStringAssetReferenceStruct = FindObject<UScriptStruct>(NULL, TEXT("/Script/CoreUObject.StringAssetReference"), true);
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->HasAnyClassFlags(CLASS_Native))
				{
					for (TFieldIterator<UStructProperty> PropIt(*It, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
					{
						if (!PropIt->HasAnyPropertyFlags(CPF_EditorOnly) && (PropIt->Struct == FStringClassReferenceStruct || PropIt->Struct == FStringAssetReferenceStruct))
						{
							FString ContentPath;
							PropIt->ExportTextItem(ContentPath, PropIt->ContainerPtrToValuePtr<void>(It->GetDefaultObject()), NULL, NULL, 0, NULL);
							if (!ContentPath.IsEmpty() && ContentPath != TEXT("None"))
							{
								FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(ContentPath);
							}
						}
					}
					for (TFieldIterator<UAssetObjectProperty> PropIt(*It, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
					{
						if (!PropIt->HasAnyPropertyFlags(CPF_EditorOnly))
						{
							FString ContentPath;
							PropIt->ExportTextItem(ContentPath, PropIt->ContainerPtrToValuePtr<void>(It->GetDefaultObject()), NULL, NULL, 0, NULL);
							if (!ContentPath.IsEmpty() && ContentPath != TEXT("None"))
							{
								FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(ContentPath);
							}
						}
					}
				}
			}
		}
	}

	UT_LOADMAP_DEFINITION()
};

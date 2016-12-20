// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournamentEditor.h"
#include "UTEditorEngine.h"

void UUTEditorEngine::InitializeObjectReferences()
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
				for (TFieldIterator<UArrayProperty> PropIt(*It, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
				{
					if (!PropIt->HasAnyPropertyFlags(CPF_EditorOnly))
					{
						UStructProperty* StructProp = Cast<UStructProperty>(PropIt->Inner);
						if (StructProp != nullptr && (StructProp->Struct == FStringClassReferenceStruct || StructProp->Struct == FStringAssetReferenceStruct))
						{
							FScriptArray* ArrayData = PropIt->ContainerPtrToValuePtr<FScriptArray>(It->GetDefaultObject());
							for (int32 i = 0; i < ArrayData->Num(); i++)
							{
								FString ContentPath;
								StructProp->ExportTextItem(ContentPath, (uint8*)ArrayData->GetData() + (i * StructProp->GetSize()), NULL, NULL, 0, NULL);
								if (!ContentPath.IsEmpty() && ContentPath != TEXT("None"))
								{
									FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(ContentPath);
								}
							}
						}
					}
				}
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
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HeaderProvider.h"
#include "UnrealHeaderTool.h"
#include "UnrealTypeDefinitionInfo.h"
#include "ClassMaps.h"

FHeaderProvider::FHeaderProvider(EHeaderProviderSourceType InType, const FString& InId, bool bInAutoInclude/* = false*/)
	: Type(InType), Id(InId), Cache(nullptr), bAutoInclude(bInAutoInclude)
{

}

template <class Predicate>
bool TryFindSourceFileWithPredicate(FUnrealSourceFile*& Out, Predicate Pred)
{
	for (const TPair<FString, TSharedRef<FUnrealSourceFile>>& MapPair : GUnrealSourceFilesMap)
	{
		if (Pred(MapPair.Value.Get()))
		{
			Out = &(MapPair.Value.Get());
			return true;
		}
	}

	return false;
}


FUnrealSourceFile* FHeaderProvider::Resolve()
{
	if (Type != EHeaderProviderSourceType::Resolved)
	{
		if (Type == EHeaderProviderSourceType::ClassName)
		{
			for (const auto& Pair : GTypeDefinitionInfoMap)
			{
				if (Pair.Key->GetFName() == *Id)
				{
					Cache = &Pair.Value->GetUnrealSourceFile();
					break;
				}
			}
		}
		else
		{
			auto* SourceFileSharedPtr = GUnrealSourceFilesMap.Find(Id);

			if (SourceFileSharedPtr != nullptr)
			{
				Cache = &SourceFileSharedPtr->Get();
			}
			else
			{
				if (!TryFindSourceFileWithPredicate(Cache, [this](const FUnrealSourceFile& SourceFile) { return SourceFile.GetIncludePath() == Id; }))
				{
					FString SlashId     = TEXT("/") + Id;
					FString BackslashId = TEXT("\\") + Id;
				TryFindSourceFileWithPredicate(Cache,
						[&SlashId, &BackslashId](const FUnrealSourceFile& SourceFile)
					{
							return SourceFile.GetFilename().EndsWith(SlashId) || SourceFile.GetFilename().EndsWith(BackslashId);
					}
				);
			}
		}
		}

		Type = EHeaderProviderSourceType::Resolved;
	}

	return Cache;
}

FString FHeaderProvider::ToString() const
{
	return FString::Printf(TEXT("%s %s"), Type == EHeaderProviderSourceType::ClassName ? TEXT("class") : TEXT("file"), *Id);
}

const FString& FHeaderProvider::GetId() const
{
	return Id;
}

EHeaderProviderSourceType FHeaderProvider::GetType() const
{
	return Type;
}

const FUnrealSourceFile* FHeaderProvider::GetResolved() const
{
	check(Type == EHeaderProviderSourceType::Resolved);
	return Cache;
}

FUnrealSourceFile* FHeaderProvider::GetResolved()
{
	check(Type == EHeaderProviderSourceType::Resolved);
	return Cache;
}

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B)
{
	if (A.Cache != B.Cache)
	{
		return false;
	}

	if (A.Cache == nullptr)
	{
		return A.Id == B.Id && A.Type == B.Type;
	}

	return true;
}

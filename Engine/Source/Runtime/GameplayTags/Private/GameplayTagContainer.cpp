// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

const FGameplayTagContainer FGameplayTagContainer::EmptyContainer;

FGameplayTagContainer::FGameplayTagContainer()
{}

FGameplayTagContainer::FGameplayTagContainer(FGameplayTagContainer const& Other)
{
	*this = Other;
}

FGameplayTagContainer::FGameplayTagContainer(const FGameplayTag& Tag)
{
	AddTag(Tag);
}

FGameplayTagContainer& FGameplayTagContainer::operator=(FGameplayTagContainer const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}
	GameplayTags.Empty(Other.GameplayTags.Num());
	GameplayTags.Append(Other.GameplayTags);

	return *this;
}

bool FGameplayTagContainer::operator==(FGameplayTagContainer const& Other) const
{
	if (GameplayTags.Num() != Other.GameplayTags.Num())
	{
		return false;
	}
	return Filter(Other, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit).Num() == this->Num();
}

bool FGameplayTagContainer::operator!=(FGameplayTagContainer const& Other) const
{
	if (GameplayTags.Num() != Other.GameplayTags.Num())
	{
		return true;
	}
	return Filter(Other, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit).Num() != this->Num();
}

bool FGameplayTagContainer::HasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const
{
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
	for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
	{
		if (TagManager.GameplayTagsMatch(*It, TagMatchType, TagToCheck, TagToCheckMatchType) == true)
		{
			return true;
		}
	}
	return false;
}

FGameplayTagContainer FGameplayTagContainer::GetGameplayTagParents() const
{
	FGameplayTagContainer ResultContainer;
	ResultContainer.AppendTags(*this);

	for (TArray<FGameplayTag>::TConstIterator TagIterator(GameplayTags); TagIterator; ++TagIterator)
	{
		FGameplayTagContainer ParentTags = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagParents(*TagIterator);
		ResultContainer.AppendTags(ParentTags);
	}

	return ResultContainer;
}

FGameplayTagContainer FGameplayTagContainer::Filter(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType) const
{
	FGameplayTagContainer ResultContainer;
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

	
	for (TArray<FGameplayTag>::TConstIterator OtherIt(OtherContainer.GameplayTags); OtherIt; ++OtherIt)
	{
		for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
		{
			if (TagManager.GameplayTagsMatch(*It, TagMatchType, *OtherIt, OtherTagMatchType) == true)
			{
				ResultContainer.AddTag(*It);
			}
		}
	}

	return ResultContainer;
}

bool FGameplayTagContainer::DoesTagContainerMatch(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, EGameplayContainerMatchType ContainerMatchType) const
{
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();

	for (TArray<FGameplayTag>::TConstIterator OtherIt(OtherContainer.GameplayTags); OtherIt; ++OtherIt)
	{
		bool bTagFound = false;

		for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
		{
			if (TagManager.GameplayTagsMatch(*It, TagMatchType, *OtherIt, OtherTagMatchType) == true)
			{
				if (ContainerMatchType == EGameplayContainerMatchType::Any)
				{
					return true;
				}

				bTagFound = true;

				// we only need one match per tag in OtherContainer, so don't bother looking for more
				break;
			}
		}

		if (ContainerMatchType == EGameplayContainerMatchType::All && bTagFound == false)
		{
			return false;
		}
	}

	// if we've reached this far then either we are looking for any match and didn't find one (return false) or we're looking for all matches and didn't miss one (return true).
	check(ContainerMatchType == EGameplayContainerMatchType::All || ContainerMatchType == EGameplayContainerMatchType::Any);
	return ContainerMatchType == EGameplayContainerMatchType::All;
}


bool FGameplayTagContainer::MatchesAll(FGameplayTagContainer const& Other, bool bCountEmptyAsMatch) const
{
	if (Other.Num() == 0)
	{
		return bCountEmptyAsMatch;
	}

	return DoesTagContainerMatch(Other, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::All);
}

bool FGameplayTagContainer::MatchesAny(FGameplayTagContainer const& Other, bool bCountEmptyAsMatch) const
{
	if (Other.Num() == 0)
	{
		return bCountEmptyAsMatch;
	}

	return DoesTagContainerMatch(Other, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::Any);
}

void FGameplayTagContainer::AppendTags(FGameplayTagContainer const& Other)
{
	//add all the tags
	for(TArray<FGameplayTag>::TConstIterator It(Other.GameplayTags); It; ++It)
	{
		AddTag(*It);
	}
}

void FGameplayTagContainer::AddTag(const FGameplayTag& TagToAdd)
{
	if (TagToAdd.IsValid())
	{
		// Don't want duplicate tags
		GameplayTags.AddUnique(TagToAdd);
	}
}

void FGameplayTagContainer::AddTagFast(const FGameplayTag& TagToAdd)
{
	GameplayTags.Add(TagToAdd);
}

void FGameplayTagContainer::RemoveTag(FGameplayTag TagToRemove)
{
	GameplayTags.Remove(TagToRemove);
}

void FGameplayTagContainer::RemoveAllTags(int32 Slack)
{
	GameplayTags.Empty(Slack);
}

bool FGameplayTagContainer::Serialize(FArchive& Ar)
{
	const bool bOldTagVer = Ar.UE4Ver() < VER_UE4_GAMEPLAY_TAG_CONTAINER_TAG_TYPE_CHANGE;
	
	if (bOldTagVer)
	{
		Ar << Tags_DEPRECATED;
	}
	else
	{
		Ar << GameplayTags;
	}
	
	if (Ar.IsLoading())
	{
		// Regardless of version, want loading to have a chance to handle redirects
		RedirectTags();

		// If loading old version, add old tags to the new gameplay tags array so they can be saved out with the new version
		if (bOldTagVer)
		{
			UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
			for (auto It = Tags_DEPRECATED.CreateConstIterator(); It; ++It)
			{
				FGameplayTag Tag = TagManager.RequestGameplayTag(*It);
				TagManager.AddLeafTagToContainer(*this, Tag);
			}
		}
	}

	return true;
}

void FGameplayTagContainer::RedirectTags()
{
	FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIni );
	UGameplayTagsManager& TagManager = IGameplayTagsModule::Get().GetGameplayTagsManager();
	
	for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
	{
		if (It.Key() == TEXT("GameplayTagRedirects"))
		{
			FName OldTagName = NAME_None;
			FName NewTagName;

			FParse::Value( *It.Value(), TEXT("OldTagName="), OldTagName );
			FParse::Value( *It.Value(), TEXT("NewTagName="), NewTagName );

			int32 TagIndex = 0;
			if (Tags_DEPRECATED.Find(OldTagName, TagIndex))
			{
				Tags_DEPRECATED.RemoveAt(TagIndex);
				Tags_DEPRECATED.AddUnique(NewTagName);
			}

			FGameplayTag OldTag = TagManager.RequestGameplayTag(OldTagName);
			FGameplayTag NewTag = TagManager.RequestGameplayTag(NewTagName);

			if (HasTag(OldTag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
			{
				RemoveTag(OldTag);
				AddTag(NewTag);
			}
		}
	}
}

int32 FGameplayTagContainer::Num() const
{
	return GameplayTags.Num();
}

FString FGameplayTagContainer::ToString() const
{
	FString RetString = TEXT("(GameplayTags=(");
	for (int i = 0; i < GameplayTags.Num(); ++i)
	{
		RetString += TEXT("(TagName=\"");
		RetString += GameplayTags[i].ToString();
		RetString += TEXT("\")");
		if (i < GameplayTags.Num() - 1)
		{
			RetString += TEXT(",");
		}
	}
	RetString += TEXT("))");
	return RetString;
}

FString FGameplayTagContainer::ToStringSimple() const
{
	FString RetString;
	for (int i = 0; i < GameplayTags.Num(); ++i)
	{
		RetString += TEXT("\"");
		RetString += GameplayTags[i].ToString();
		RetString += TEXT("\"");
		if (i < GameplayTags.Num() - 1)
		{
			RetString += TEXT(", ");
		}
	}
	return RetString;
}

FText FGameplayTagContainer::ToMatchingText(EGameplayContainerMatchType MatchType, bool bInvertCondition) const
{
	enum class EMatchingTypes : int8
	{
		Inverted	= 0x01,
		All			= 0x02
	};

#define LOCTEXT_NAMESPACE "FGameplayTagContainer"
	const FText MatchingDescription[] =
	{
		LOCTEXT("MatchesAnyGameplayTags", "Has any tags in set: {GameplayTagSet}"),
		LOCTEXT("NotMatchesAnyGameplayTags", "Does not have any tags in set: {GameplayTagSet}"),
		LOCTEXT("MatchesAllGameplayTags", "Has all tags in set: {GameplayTagSet}"),
		LOCTEXT("NotMatchesAllGameplayTags", "Does not have all tags in set: {GameplayTagSet}")
	};
#undef LOCTEXT_NAMESPACE

	int32 DescriptionIndex = bInvertCondition ? static_cast<int32>(EMatchingTypes::Inverted) : 0;
	switch (MatchType)
	{
		case EGameplayContainerMatchType::All:
			DescriptionIndex |= static_cast<int32>(EMatchingTypes::All);
			break;

		case EGameplayContainerMatchType::Any:
			break;

		default:
			UE_LOG(LogGameplayTags, Warning, TEXT("Invalid value for TagsToMatch (EGameplayContainerMatchType) %d.  Should only be Any or All."), static_cast<int32>(MatchType));
			break;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("GameplayTagSet"), FText::FromString(*ToString()));
	return FText::Format(MatchingDescription[DescriptionIndex], Arguments);
}

FGameplayTag::FGameplayTag()
	: TagName(NAME_None)
{
}

bool FGameplayTag::Matches(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().GameplayTagsMatch(*this, MatchTypeOne, Other, MatchTypeTwo);
}

bool FGameplayTag::IsValid() const
{
	return (TagName != NAME_None);
}

FGameplayTag::FGameplayTag(FName Name)
	: TagName(Name)
{
	check(IGameplayTagsModule::Get().GetGameplayTagsManager().ValidateTagCreation(Name));
}

FGameplayTag FGameplayTag::RequestDirectParent() const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagDirectParent(*this);
}

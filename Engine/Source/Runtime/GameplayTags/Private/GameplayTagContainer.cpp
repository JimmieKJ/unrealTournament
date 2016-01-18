// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

const FGameplayTagContainer FGameplayTagContainer::EmptyContainer;
const FGameplayTagQuery FGameplayTagQuery::EmptyQuery;

DEFINE_STAT(STAT_FGameplayTagContainer_HasTag);
DEFINE_STAT(STAT_FGameplayTagContainer_DoesTagContainerMatch);
DEFINE_STAT(STAT_UGameplayTagsManager_GameplayTagsMatch);




/** Helper class to parse/eval query token streams. */
class FQueryEvaluator
{
public:
	FQueryEvaluator(FGameplayTagQuery const& Q)
		: Query(Q), 
		CurStreamIdx(0), 
		Version(EGameplayTagQueryStreamVersion::LatestVersion), 
		bReadError(false)
	{}

	/** Evaluates the query against the given tag container and returns the result (true if matching, false otherwise). */
	bool Eval(FGameplayTagContainer const& Tags);

	/** Parses the token stream into an FExpr. */
	void Read(struct FGameplayTagQueryExpression& E);

private:
	FGameplayTagQuery const& Query;
	int32 CurStreamIdx;
	int32 Version;
	bool bReadError;

	bool EvalAnyTagsMatch(FGameplayTagContainer const& Tags, bool bSkip);
	bool EvalAllTagsMatch(FGameplayTagContainer const& Tags, bool bSkip);
	bool EvalNoTagsMatch(FGameplayTagContainer const& Tags, bool bSkip);

	bool EvalAnyExprMatch(FGameplayTagContainer const& Tags, bool bSkip);
	bool EvalAllExprMatch(FGameplayTagContainer const& Tags, bool bSkip);
	bool EvalNoExprMatch(FGameplayTagContainer const& Tags, bool bSkip);

	bool EvalExpr(FGameplayTagContainer const& Tags, bool bSkip = false);
	void ReadExpr(struct FGameplayTagQueryExpression& E);

#if WITH_EDITOR
public:
	UEditableGameplayTagQuery* CreateEditableQuery();

private:
	UEditableGameplayTagQueryExpression* ReadEditableQueryExpr(UObject* ExprOuter);
	void ReadEditableQueryTags(UEditableGameplayTagQueryExpression* EditableQueryExpr);
	void ReadEditableQueryExprList(UEditableGameplayTagQueryExpression* EditableQueryExpr);
#endif // WITH_EDITOR

	/** Returns the next token in the stream. If there's a read error, sets bReadError and returns zero, so be sure to check that. */
	uint8 GetToken()
	{
		if (Query.QueryTokenStream.IsValidIndex(CurStreamIdx))
		{
			return Query.QueryTokenStream[CurStreamIdx++];
		}
		
		UE_LOG(LogGameplayTags, Warning, TEXT("Error parsing FGameplayTagQuery!"));
		bReadError = true;
		return 0;
	}
};

bool FQueryEvaluator::Eval(FGameplayTagContainer const& Tags)
{
	CurStreamIdx = 0;

	// start parsing the set
	Version = GetToken();
	if (bReadError)
	{
		return false;
	}
	
	bool bRet = false;

	uint8 const bHasRootExpression = GetToken();
	if (!bReadError && bHasRootExpression)
	{
		bRet = EvalExpr(Tags);

	}

	ensure(CurStreamIdx == Query.QueryTokenStream.Num());
	return bRet;
}

void FQueryEvaluator::Read(FGameplayTagQueryExpression& E)
{
	E = FGameplayTagQueryExpression();
	CurStreamIdx = 0;

	if (Query.QueryTokenStream.Num() > 0)
	{
		// start parsing the set
		Version = GetToken();
		if (!bReadError)
		{
			uint8 const bHasRootExpression = GetToken();
			if (!bReadError && bHasRootExpression)
			{
				ReadExpr(E);
			}
		}

		ensure(CurStreamIdx == Query.QueryTokenStream.Num());
	}
}

void FQueryEvaluator::ReadExpr(FGameplayTagQueryExpression& E)
{
	E.ExprType = (EGameplayTagQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return;
	}
	
	if (E.UsesTagSet())
	{
		// parse tag set
		int32 NumTags = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumTags; ++Idx)
		{
			int32 const TagIdx = GetToken();
			if (bReadError)
			{
				return;
			}

			FGameplayTag Tag = Query.GetTagFromIndex(TagIdx);
			E.AddTag(Tag);
		}
	}
	else
	{
		// parse expr set
		int32 NumExprs = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumExprs; ++Idx)
		{
			FGameplayTagQueryExpression Exp;
			ReadExpr(Exp);
			Exp.AddExpr(Exp);
		}
	}
}


bool FQueryEvaluator::EvalAnyTagsMatch(FGameplayTagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;
	bool Result = false;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FGameplayTag Tag = Query.GetTagFromIndex(TagIdx);

			bool bHasTag = Tags.HasTag(Tag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit);

			if (bHasTag)
			{
				// one match is sufficient for a true result!
				bShortCircuit = true;
				Result = true;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalAllTagsMatch(FGameplayTagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FGameplayTag const Tag = Query.GetTagFromIndex(TagIdx);
			bool const bHasTag = Tags.HasTag(Tag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit);

			if (bHasTag == false)
			{
				// one failed match is sufficient for a false result
				bShortCircuit = true;
				Result = false;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalNoTagsMatch(FGameplayTagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}
	
	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FGameplayTag const Tag = Query.GetTagFromIndex(TagIdx);
			bool const bHasTag = Tags.HasTag(Tag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit);

			if (bHasTag == true)
			{
				// one match is sufficient for a false result
				bShortCircuit = true;
				Result = false;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalAnyExprMatch(FGameplayTagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume false until proven otherwise
	bool Result = false;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == true)
			{
				// one match is sufficient for true result
				Result = true;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}
bool FQueryEvaluator::EvalAllExprMatch(FGameplayTagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == false)
			{
				// one fail is sufficient for false result
				Result = false;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}
bool FQueryEvaluator::EvalNoExprMatch(FGameplayTagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == true)
			{
				// one match is sufficient for fail result
				Result = false;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}


bool FQueryEvaluator::EvalExpr(FGameplayTagContainer const& Tags, bool bSkip)
{
	EGameplayTagQueryExprType::Type const ExprType = (EGameplayTagQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return false;
	}

	// emit exprdata
	switch (ExprType)
	{
	case EGameplayTagQueryExprType::AnyTagsMatch:
		return EvalAnyTagsMatch(Tags, bSkip);
	case EGameplayTagQueryExprType::AllTagsMatch:
		return EvalAllTagsMatch(Tags, bSkip);
	case EGameplayTagQueryExprType::NoTagsMatch:
		return EvalNoTagsMatch(Tags, bSkip);

	case EGameplayTagQueryExprType::AnyExprMatch:
		return EvalAnyExprMatch(Tags, bSkip);
	case EGameplayTagQueryExprType::AllExprMatch:
		return EvalAllExprMatch(Tags, bSkip);
	case EGameplayTagQueryExprType::NoExprMatch:
		return EvalNoExprMatch(Tags, bSkip);
	}

	check(false);
	return false;
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

FGameplayTagContainer& FGameplayTagContainer::operator=(FGameplayTagContainer&& Other)
{
	GameplayTags = MoveTemp(Other.GameplayTags);
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

bool FGameplayTagContainer::ComplexHasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const
{
	check(TagMatchType != EGameplayTagMatchType::Explicit || TagToCheckMatchType != EGameplayTagMatchType::Explicit);
	UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();

	if (TagMatchType != EGameplayTagMatchType::Explicit)
	{
		for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
		{
			const FGameplayTagContainer* Parents = TagManager.GetAllParentsContainer(*It);
			if (Parents && Parents->HasTag(TagToCheck, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
			{
				return true;
			}
		}
	}
	else
	{
		const FGameplayTagContainer* Parents = TagManager.GetAllParentsContainer(TagToCheck);
		if (Parents && DoesTagContainerMatch(*Parents, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::Any))
		{
			return true;
		}

	}
	return false;
}

#if CHECK_TAG_OPTIMIZATIONS
bool FGameplayTagContainer::HasTagOriginal(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_HasTag);

	UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();
	for (TArray<FGameplayTag>::TConstIterator It(this->GameplayTags); It; ++It)
	{
		if (TagManager.GameplayTagsMatch(*It, TagMatchType, TagToCheck, TagToCheckMatchType) == true)
		{
			return true;
		}
	}
	return false;
}
#endif

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::RemoveTagByExplicitName"), STAT_FGameplayTagContainer_RemoveTagByExplicitName, STATGROUP_GameplayTags);

bool FGameplayTagContainer::RemoveTagByExplicitName(const FName& TagName)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_RemoveTagByExplicitName);

	for (auto GameplayTag : this->GameplayTags)
	{
		if (GameplayTag.GetTagName() == TagName)
		{
			RemoveTag(GameplayTag);
			return true;
		}
	}

	return false;
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::GetGameplayTagParents"), STAT_FGameplayTagContainer_GetGameplayTagParents, STATGROUP_GameplayTags);

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

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::Filter"), STAT_FGameplayTagContainer_Filter, STATGROUP_GameplayTags);

FGameplayTagContainer FGameplayTagContainer::Filter(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType) const
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_Filter);

	FGameplayTagContainer ResultContainer;
	UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();

	
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

bool FGameplayTagContainer::DoesTagContainerMatchComplex(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, EGameplayContainerMatchType ContainerMatchType) const
{
	UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();

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

bool FGameplayTagContainer::MatchesQuery(const FGameplayTagQuery& Query) const
{
	return Query.Matches(*this);
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::AppendTags"), STAT_FGameplayTagContainer_AppendTags, STATGROUP_GameplayTags);

void FGameplayTagContainer::AppendTags(FGameplayTagContainer const& Other)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_AppendTags);

	//add all the tags
	for(TArray<FGameplayTag>::TConstIterator It(Other.GameplayTags); It; ++It)
	{
		AddTag(*It);
	}
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::AppendMatchingTags"), STAT_FGameplayTagContainer_AppendMatchingTags, STATGROUP_GameplayTags);


void FGameplayTagContainer::AppendMatchingTags(FGameplayTagContainer const& OtherA, FGameplayTagContainer const& OtherB)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_AppendMatchingTags);

	for(TArray<FGameplayTag>::TConstIterator It(OtherA.GameplayTags); It; ++It)
	{
		if (OtherB.HasTag(*It, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::IncludeParentTags))
		{
			AddTag(*It);		
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::AddTag"), STAT_FGameplayTagContainer_AddTag, STATGROUP_GameplayTags);


void FGameplayTagContainer::AddTag(const FGameplayTag& TagToAdd)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_AddTag);

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

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::RemoveTag"), STAT_FGameplayTagContainer_RemoveTag, STATGROUP_GameplayTags);

bool FGameplayTagContainer::RemoveTag(FGameplayTag TagToRemove)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_RemoveTag);

	return GameplayTags.RemoveSingle(TagToRemove) > 0;
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTagContainer::RemoveTags"), STAT_FGameplayTagContainer_RemoveTags, STATGROUP_GameplayTags);

void FGameplayTagContainer::RemoveTags(FGameplayTagContainer TagsToRemove)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_RemoveTags);

	for (auto Tag : TagsToRemove)
	{
		RemoveTag(Tag);
	}
}

void FGameplayTagContainer::RemoveAllTags(int32)
{
	GameplayTags.Reset();
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
		UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();

		// If loading old version, add old tags to the new gameplay tags array so they can be saved out with the new version
		// This needs to happen 
		// NOTE: DeprecatedTagNamesNotFoundInTagMap should be removed along with the bOldTagVer when we remove backwards
		// compatibility, and the signature of RedirectTagsForContainer (below) should be changed appropriately as well.
		TSet<FName> DeprecatedTagNamesNotFoundInTagMap;
		if (bOldTagVer)
		{
			for (auto It = Tags_DEPRECATED.CreateConstIterator(); It; ++It)
			{
				const bool bErrorIfNotFound = false;
				FGameplayTag Tag = TagManager.RequestGameplayTag(*It, bErrorIfNotFound);
				if (Tag.IsValid())
				{
					TagManager.AddLeafTagToContainer(*this, Tag);
				}
				else
				{	// For tags not found in the current table, add them to a list to be checked when handling
					// redirection (below).
					DeprecatedTagNamesNotFoundInTagMap.Add(*It);
				}
			}
		}

		// Rename any tags that may have changed by the ini file.  Redirects can happen regardless of version.
		// Regardless of version, want loading to have a chance to handle redirects
		TagManager.RedirectTagsForContainer(*this, DeprecatedTagNamesNotFoundInTagMap);
	}

	return true;
}

int32 FGameplayTagContainer::Num() const
{
	return GameplayTags.Num();
}

FString FGameplayTagContainer::ToString() const
{
	FString RetString = TEXT("(GameplayTags=");
	if (GameplayTags.Num() > 0)
	{
		RetString += TEXT("(");

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
		RetString += TEXT(")");
	}
	RetString += TEXT(")");

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

bool FGameplayTagContainer::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	uint8 NumTags;
	if (Ar.IsSaving())
	{
		NumTags = GameplayTags.Num();
		Ar << NumTags;
		for (FGameplayTag& Tag : GameplayTags)
		{
			Tag.NetSerialize(Ar, Map, bOutSuccess);
		}
	}
	else
	{
		Ar << NumTags;
		GameplayTags.Empty(NumTags);
		GameplayTags.AddDefaulted(NumTags);
		for (uint8 idx = 0; idx < NumTags; ++idx)
		{
			GameplayTags[idx].NetSerialize(Ar, Map, bOutSuccess);
		}

	}

	bOutSuccess  = true;
	return true;
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

bool FGameplayTag::ComplexMatches(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().ComplexGameplayTagsMatch(*this, MatchTypeOne, Other, MatchTypeTwo);
}

#if CHECK_TAG_OPTIMIZATIONS
bool FGameplayTag::MatchesOriginal(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().GameplayTagsMatchOriginal(*this, MatchTypeOne, Other, MatchTypeTwo);
}
#endif

FGameplayTag::FGameplayTag(FName Name)
	: TagName(Name)
{
	check(IGameplayTagsModule::Get().GetGameplayTagsManager().ValidateTagCreation(Name));
}

FGameplayTag FGameplayTag::RequestDirectParent() const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagDirectParent(*this);
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTag::NetSerialize"), STAT_FGameplayTag_NetSerialize, STATGROUP_GameplayTags);

bool FGameplayTag::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTag_NetSerialize);

	UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();

	uint8 bHasName = (TagName != NAME_None);
	uint8 bHasNetIndex = 0;
	FGameplayTagNetIndex NetIndex = INVALID_TAGNETINDEX;

	if (Ar.IsSaving())
	{
		NetIndex = TagManager.GetNetIndexFromTag(*this);
		if (NetIndex != INVALID_TAGNETINDEX)
		{
			// If we have a valid net index, serialize with that
			bHasNetIndex = true;
		}
	}

	// Serialize if we have a name at all or are empty
	Ar.SerializeBits(&bHasName, 1);

	if (bHasName)
	{
		Ar.SerializeBits(&bHasNetIndex, 1);
		// If we have a net index serialize that, otherwise serialize as a name
		if (bHasNetIndex)
		{
			Ar << NetIndex;
		}
		else
		{
			Ar << TagName;
		}

		if (Ar.IsLoading() && bHasNetIndex)
		{
			TagName = TagManager.GetTagNameFromNetIndex(NetIndex);
		}
	}
	else
	{
		TagName = NAME_None;
	}

	bOutSuccess = true;
	return true;
}



FGameplayTagQuery::FGameplayTagQuery()
	: TokenStreamVersion(EGameplayTagQueryStreamVersion::LatestVersion)
{
}

FGameplayTagQuery::FGameplayTagQuery(FGameplayTagQuery const& Other)
{
	*this = Other;
}

FGameplayTagQuery::FGameplayTagQuery(FGameplayTagQuery&& Other)
{
	*this = MoveTemp(Other);
}

/** Assignment/Equality operators */
FGameplayTagQuery& FGameplayTagQuery::operator=(FGameplayTagQuery const& Other)
{
	if (this != &Other)
	{
		TokenStreamVersion = Other.TokenStreamVersion;
		TagDictionary = Other.TagDictionary;
		QueryTokenStream = Other.QueryTokenStream;
		UserDescription = Other.UserDescription;
		AutoDescription = Other.AutoDescription;
	}
	return *this;
}

FGameplayTagQuery& FGameplayTagQuery::operator=(FGameplayTagQuery&& Other)
{
	TokenStreamVersion = Other.TokenStreamVersion;
	TagDictionary = MoveTemp(Other.TagDictionary);
	QueryTokenStream = MoveTemp(Other.QueryTokenStream);
	UserDescription = MoveTemp(Other.UserDescription);
	AutoDescription = MoveTemp(Other.AutoDescription);
	return *this;
}

bool FGameplayTagQuery::Matches(FGameplayTagContainer const& Tags) const
{
	FQueryEvaluator QE(*this);
	return QE.Eval(Tags);
}

bool FGameplayTagQuery::IsEmpty() const
{
	return (QueryTokenStream.Num() == 0);
}

void FGameplayTagQuery::Clear()
{
	*this = FGameplayTagQuery::EmptyQuery;
}

void FGameplayTagQuery::GetQueryExpr(FGameplayTagQueryExpression& OutExpr) const
{
	// build the FExpr tree from the token stream and return it
	FQueryEvaluator QE(*this);
	QE.Read(OutExpr);
}

void FGameplayTagQuery::Build(FGameplayTagQueryExpression& RootQueryExpr, FString InUserDescription)
{
	TokenStreamVersion = EGameplayTagQueryStreamVersion::LatestVersion;
	UserDescription = InUserDescription;

	// Reserve size here is arbitrary, goal is to minimizing reallocs while being respectful of mem usage
	QueryTokenStream.Reset(128);
	TagDictionary.Reset();

	// add stream version first
	QueryTokenStream.Add(EGameplayTagQueryStreamVersion::LatestVersion);

	// emit the query
	QueryTokenStream.Add(1);		// true to indicate is has a root expression
	RootQueryExpr.EmitTokens(QueryTokenStream, TagDictionary);
}

// static 
FGameplayTagQuery FGameplayTagQuery::BuildQuery(FGameplayTagQueryExpression& RootQueryExpr, FString InDescription)
{
	FGameplayTagQuery Q;
	Q.Build(RootQueryExpr, InDescription);
	return Q;
}

//static 
FGameplayTagQuery FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer const& InTags)
{
	return FGameplayTagQuery::BuildQuery
	(
		FGameplayTagQueryExpression()
		.AnyTagsMatch()
		.AddTags(InTags)
	);
}

//static
FGameplayTagQuery FGameplayTagQuery::MakeQuery_MatchAllTags(FGameplayTagContainer const& InTags)
{
	return FGameplayTagQuery::BuildQuery
		(
		FGameplayTagQueryExpression()
		.AllTagsMatch()
		.AddTags(InTags)
		);
}

// static
FGameplayTagQuery FGameplayTagQuery::MakeQuery_MatchNoTags(FGameplayTagContainer const& InTags)
{
	return FGameplayTagQuery::BuildQuery
		(
		FGameplayTagQueryExpression()
		.NoTagsMatch()
		.AddTags(InTags)
		);
}


#if WITH_EDITOR

UEditableGameplayTagQuery* FQueryEvaluator::CreateEditableQuery()
{
	CurStreamIdx = 0;

	UEditableGameplayTagQuery* const EditableQuery = NewObject<UEditableGameplayTagQuery>(GetTransientPackage(), NAME_None, RF_Transactional);

	// start parsing the set
	Version = GetToken();
	if (!bReadError)
	{
		uint8 const bHasRootExpression = GetToken();
		if (!bReadError && bHasRootExpression)
		{
			EditableQuery->RootExpression = ReadEditableQueryExpr(EditableQuery);
		}
	}
	ensure(CurStreamIdx == Query.QueryTokenStream.Num());

	EditableQuery->UserDescription = Query.UserDescription;

	return EditableQuery;
}

UEditableGameplayTagQueryExpression* FQueryEvaluator::ReadEditableQueryExpr(UObject* ExprOuter)
{
	EGameplayTagQueryExprType::Type const ExprType = (EGameplayTagQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return nullptr;
	}

	UClass* ExprClass = nullptr;
	switch (ExprType)
	{
	case EGameplayTagQueryExprType::AnyTagsMatch:
		ExprClass = UEditableGameplayTagQueryExpression_AnyTagsMatch::StaticClass();
		break;
	case EGameplayTagQueryExprType::AllTagsMatch:
		ExprClass = UEditableGameplayTagQueryExpression_AllTagsMatch::StaticClass();
		break;
	case EGameplayTagQueryExprType::NoTagsMatch:
		ExprClass = UEditableGameplayTagQueryExpression_NoTagsMatch::StaticClass();
		break;
	case EGameplayTagQueryExprType::AnyExprMatch:
		ExprClass = UEditableGameplayTagQueryExpression_AnyExprMatch::StaticClass();
		break;
	case EGameplayTagQueryExprType::AllExprMatch:
		ExprClass = UEditableGameplayTagQueryExpression_AllExprMatch::StaticClass();
		break;
	case EGameplayTagQueryExprType::NoExprMatch:
		ExprClass = UEditableGameplayTagQueryExpression_NoExprMatch::StaticClass();
		break;
	}

	UEditableGameplayTagQueryExpression* NewExpr = nullptr;
	if (ExprClass)
	{
		NewExpr = NewObject<UEditableGameplayTagQueryExpression>(ExprOuter, ExprClass, NAME_None, RF_Transactional);
		if (NewExpr)
		{
			switch (ExprType)
			{
			case EGameplayTagQueryExprType::AnyTagsMatch:
			case EGameplayTagQueryExprType::AllTagsMatch:
			case EGameplayTagQueryExprType::NoTagsMatch:
				ReadEditableQueryTags(NewExpr);
				break;
			case EGameplayTagQueryExprType::AnyExprMatch:
			case EGameplayTagQueryExprType::AllExprMatch:
			case EGameplayTagQueryExprType::NoExprMatch:
				ReadEditableQueryExprList(NewExpr);
				break;
			}
		}
	}

	return NewExpr;
}

void FQueryEvaluator::ReadEditableQueryTags(UEditableGameplayTagQueryExpression* EditableQueryExpr)
{
	// find the tag container to read into
	FGameplayTagContainer* Tags = nullptr;
	if (EditableQueryExpr->IsA(UEditableGameplayTagQueryExpression_AnyTagsMatch::StaticClass()))
	{
		Tags = &((UEditableGameplayTagQueryExpression_AnyTagsMatch*)EditableQueryExpr)->Tags;
	}
	else if (EditableQueryExpr->IsA(UEditableGameplayTagQueryExpression_AllTagsMatch::StaticClass()))
	{
		Tags = &((UEditableGameplayTagQueryExpression_AllTagsMatch*)EditableQueryExpr)->Tags;
	}
	else if (EditableQueryExpr->IsA(UEditableGameplayTagQueryExpression_NoTagsMatch::StaticClass()))
	{
		Tags = &((UEditableGameplayTagQueryExpression_NoTagsMatch*)EditableQueryExpr)->Tags;
	}
	ensure(Tags);

	if (Tags)
	{
		// parse tag set
		int32 const NumTags = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumTags; ++Idx)
		{
			int32 const TagIdx = GetToken();
			if (bReadError)
			{
				return;
			}

			FGameplayTag const Tag = Query.GetTagFromIndex(TagIdx);
			Tags->AddTag(Tag);
		}
	}
}

void FQueryEvaluator::ReadEditableQueryExprList(UEditableGameplayTagQueryExpression* EditableQueryExpr)
{
	// find the tag container to read into
	TArray<UEditableGameplayTagQueryExpression*>* ExprList = nullptr;
	if (EditableQueryExpr->IsA(UEditableGameplayTagQueryExpression_AnyExprMatch::StaticClass()))
	{
		ExprList = &((UEditableGameplayTagQueryExpression_AnyExprMatch*)EditableQueryExpr)->Expressions;
	}
	else if (EditableQueryExpr->IsA(UEditableGameplayTagQueryExpression_AllExprMatch::StaticClass()))
	{
		ExprList = &((UEditableGameplayTagQueryExpression_AllExprMatch*)EditableQueryExpr)->Expressions;
	}
	else if (EditableQueryExpr->IsA(UEditableGameplayTagQueryExpression_NoExprMatch::StaticClass()))
	{
		ExprList = &((UEditableGameplayTagQueryExpression_NoExprMatch*)EditableQueryExpr)->Expressions;
	}
	ensure(ExprList);

	if (ExprList)
	{
		// parse expr set
		int32 const NumExprs = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumExprs; ++Idx)
		{
			UEditableGameplayTagQueryExpression* const NewExpr = ReadEditableQueryExpr(EditableQueryExpr);
			ExprList->Add(NewExpr);
		}
	}
}

UEditableGameplayTagQuery* FGameplayTagQuery::CreateEditableQuery()
{
	FQueryEvaluator QE(*this);
	return QE.CreateEditableQuery();
}

void FGameplayTagQuery::BuildFromEditableQuery(UEditableGameplayTagQuery& EditableQuery)
{
	QueryTokenStream.Reset();
	TagDictionary.Reset();

	UserDescription = EditableQuery.UserDescription;

	// add stream version first
	QueryTokenStream.Add(EGameplayTagQueryStreamVersion::LatestVersion);
	EditableQuery.EmitTokens(QueryTokenStream, TagDictionary, &AutoDescription);
}

FString UEditableGameplayTagQuery::GetTagQueryExportText(FGameplayTagQuery const& TagQuery)
{
	TagQueryExportText_Helper = TagQuery;
	UProperty* const TQProperty = FindField<UProperty>(GetClass(), TEXT("TagQueryExportText_Helper"));

	FString OutString;
	TQProperty->ExportTextItem(OutString, (void*)&TagQueryExportText_Helper, (void*)&TagQueryExportText_Helper, this, 0);
	return OutString;
}

void UEditableGameplayTagQuery::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	if (DebugString)
	{
		// start with a fresh string
		DebugString->Empty();
	}

	if (RootExpression)
	{
		TokenStream.Add(1);		// true if has a root expression
		RootExpression->EmitTokens(TokenStream, TagDictionary, DebugString);
	}
	else
	{
		TokenStream.Add(0);		// false if no root expression
		if (DebugString)
		{
			DebugString->Append(TEXT("undefined"));
		}
	}
}

void UEditableGameplayTagQueryExpression::EmitTagTokens(FGameplayTagContainer const& TagsToEmit, TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	uint8 const NumTags = (uint8)TagsToEmit.Num();
	TokenStream.Add(NumTags);

	bool bFirstTag = true;

	for (auto T : TagsToEmit)
	{
		int32 TagIdx = TagDictionary.AddUnique(T);
		check(TagIdx <= 255);
		TokenStream.Add((uint8)TagIdx);

		if (DebugString)
		{
			if (bFirstTag == false)
			{
				DebugString->Append(TEXT(","));
			}

			DebugString->Append(TEXT(" "));
			DebugString->Append(T.ToString());
		}

		bFirstTag = false;
	}
}

void UEditableGameplayTagQueryExpression::EmitExprListTokens(TArray<UEditableGameplayTagQueryExpression*> const& ExprList, TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	uint8 const NumExprs = (uint8)ExprList.Num();
	TokenStream.Add(NumExprs);

	bool bFirstExpr = true;
	
	for (auto E : ExprList)
	{
		if (DebugString)
		{
			if (bFirstExpr == false)
			{
				DebugString->Append(TEXT(","));
			}

			DebugString->Append(TEXT(" "));
		}

		if (E)
		{
			E->EmitTokens(TokenStream, TagDictionary, DebugString);
		}
		else
		{
			// null expression
			TokenStream.Add(EGameplayTagQueryExprType::Undefined);
			if (DebugString)
			{
				DebugString->Append(TEXT("undefined"));
			}
		}

		bFirstExpr = false;
	}
}

void UEditableGameplayTagQueryExpression_AnyTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EGameplayTagQueryExprType::AnyTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ANY("));
	}

	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableGameplayTagQueryExpression_AllTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EGameplayTagQueryExprType::AllTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ALL("));
	}
	
	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableGameplayTagQueryExpression_NoTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EGameplayTagQueryExprType::NoTagsMatch);
	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableGameplayTagQueryExpression_AnyExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EGameplayTagQueryExprType::AnyExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ANY("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableGameplayTagQueryExpression_AllExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EGameplayTagQueryExprType::AllExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ALL("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableGameplayTagQueryExpression_NoExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EGameplayTagQueryExprType::NoExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" NONE("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}
#endif	// WITH_EDITOR


FGameplayTagQueryExpression& FGameplayTagQueryExpression::AddTag(FName TagName)
{
	FGameplayTag const Tag = IGameplayTagsModule::RequestGameplayTag(TagName);
	return AddTag(Tag);
}

void FGameplayTagQueryExpression::EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary) const
{
	// emit exprtype
	TokenStream.Add(ExprType);

	// emit exprdata
	switch (ExprType)
	{
	case EGameplayTagQueryExprType::AnyTagsMatch:
	case EGameplayTagQueryExprType::AllTagsMatch:
	case EGameplayTagQueryExprType::NoTagsMatch:
	{
		// emit tagset
		uint8 NumTags = (uint8)TagSet.Num();
		TokenStream.Add(NumTags);

		for (auto Tag : TagSet)
		{
			int32 TagIdx = TagDictionary.AddUnique(Tag);
			check(TagIdx <= 254);		// we reserve token 255 for internal use, so 254 is max unique tags
			TokenStream.Add((uint8)TagIdx);
		}
	}
	break;

	case EGameplayTagQueryExprType::AnyExprMatch:
	case EGameplayTagQueryExprType::AllExprMatch:
	case EGameplayTagQueryExprType::NoExprMatch:
	{
		// emit tagset
		uint8 NumExprs = (uint8)ExprSet.Num();
		TokenStream.Add(NumExprs);

		for (auto& E : ExprSet)
		{
			E.EmitTokens(TokenStream, TagDictionary);
		}
	}
	break;
	default:
		break;
	}
}


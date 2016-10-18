// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"
#include "PropertyTag.h"
#include "Engine/PackageMapClient.h"

const FGameplayTagContainer FGameplayTagContainer::EmptyContainer;
const FGameplayTagQuery FGameplayTagQuery::EmptyQuery;

DEFINE_STAT(STAT_FGameplayTagContainer_HasTag);
DEFINE_STAT(STAT_FGameplayTagContainer_DoesTagContainerMatch);
DEFINE_STAT(STAT_UGameplayTagsManager_GameplayTagsMatch);

/**
 *	Replicates a tag in a packed format:
 *	-A segment of NetIndexFirstBitSegment bits are always replicated.
 *	-Another bit is replicated to indicate "more"
 *	-If "more", then another segment of (MaxBits - NetIndexFirstBitSegment) length is replicated.
 *	
 *	This format is basically the same as SerializeIntPacked, except that there are only 2 segments and they are not the same size.
 *	The gameplay tag system is able to exploit knoweledge in what tags are frequently replicated to ensure they appear in the first segment.
 *	Making frequently replicated tags as cheap as possible. 
 *	
 *	
 *	Setting up your project to take advantage of the packed format.
 *	-Run a normal networked game on non shipping build. 
 *	-After some time, run console command "GameplayTags.PrintReport" or set "GameplayTags.PrintReportOnShutdown 1" cvar.
 *	-This will generate information on the server log about what tags replicate most frequently.
 *	-Take this list and put it in DefaultGameplayTags.ini.
 *	-CommonlyReplicatedTags is the ordered list of tags.
 *	-NetIndexFirstBitSegment is the number of bits (not including the "more" bit) for the first segment.
 *
 */
void SerializeTagNetIndexPacked(FArchive& Ar, FGameplayTagNetIndex& Value, const int32 NetIndexFirstBitSegment, const int32 MaxBits)
{
	// Case where we have no segment or the segment is larger than max bits
	if (NetIndexFirstBitSegment <= 0 || NetIndexFirstBitSegment >= MaxBits)
	{
		if (Ar.IsLoading())
		{
			Value = 0;
		}
		Ar.SerializeBits(&Value, MaxBits);
		return;
	}


	const uint32 BitMasks[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff};
	const uint32 MoreBits[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};

	const int32 FirstSegment = NetIndexFirstBitSegment;
	const int32 SecondSegment = MaxBits - NetIndexFirstBitSegment;

	if (Ar.IsSaving())
	{
		uint32 Mask = BitMasks[FirstSegment];
		if (Value > Mask)
		{
			uint32 FirstDataSegment = ((Value & Mask) | MoreBits[FirstSegment+1]);
			uint32 SecondDataSegment = (Value >> FirstSegment);

			uint32 SerializedValue = FirstDataSegment | (SecondDataSegment << (FirstSegment+1));				

			Ar.SerializeBits(&SerializedValue, MaxBits + 1);
		}
		else
		{
			uint32 SerializedValue = Value;
			Ar.SerializeBits(&SerializedValue, NetIndexFirstBitSegment + 1);
		}

	}
	else
	{
		uint32 FirstData = 0;
		Ar.SerializeBits(&FirstData, FirstSegment + 1);
		uint32 More = FirstData & MoreBits[FirstSegment+1];
		if (More)
		{
			uint32 SecondData = 0;
			Ar.SerializeBits(&SecondData, SecondSegment);
			Value = (SecondData << FirstSegment);
			Value |= (FirstData & BitMasks[FirstSegment]);
		}
		else
		{
			Value = FirstData;
		}

	}
}


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

	GameplayTags.Reserve(Other.Num());

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
	
	// Only do redirects for real loads, not for duplicates or recompiles
	if (Ar.IsLoading() && Ar.IsPersistent() && !(Ar.GetPortFlags() & PPF_Duplicate) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE))
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
		TagManager.RedirectTagsForContainer(*this, DeprecatedTagNamesNotFoundInTagMap, Ar.GetSerializedProperty());
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
	// 1st bit to indicate empty tag container or not (empty tag containers are frequently replicated). Early out if empty.
	uint8 IsEmpty = GameplayTags.Num() == 0;
	Ar.SerializeBits(&IsEmpty, 1);
	if (IsEmpty)
	{
		if (GameplayTags.Num() > 0)
		{
			GameplayTags.Reset();
		}
		bOutSuccess = true;
		return true;
	}

	// -------------------------------------------------------

	if (Ar.IsSaving())
	{
		uint8 NumTags = GameplayTags.Num();
		uint8 MaxSize = (1 << UGameplayTagsManager::NumBitsForContainerSize);
		if (!ensureMsgf(NumTags < MaxSize, TEXT("TagContainer has %d elements when max is %d! Tags: %s"), NumTags, MaxSize, *ToStringSimple()))
		{
			NumTags = MaxSize - 1;
		}
		
		Ar.SerializeBits(&NumTags, UGameplayTagsManager::NumBitsForContainerSize);
		for (int32 idx=0; idx < NumTags;++idx)
		{
			FGameplayTag& Tag = GameplayTags[idx];
			Tag.NetSerialize_Packed(Ar, Map, bOutSuccess);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			UGameplayTagsManager::NotifyTagReplicated(Tag, true);
#endif
		}
	}
	else
	{
		// No Common Container tags, just replicate this like normal
		uint8 NumTags = 0;
		Ar.SerializeBits(&NumTags, UGameplayTagsManager::NumBitsForContainerSize);

		GameplayTags.Empty(NumTags);
		GameplayTags.AddDefaulted(NumTags);
		for (uint8 idx = 0; idx < NumTags; ++idx)
		{
			GameplayTags[idx].NetSerialize_Packed(Ar, Map, bOutSuccess);
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

bool FGameplayTag::SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
{
	if (Tag.Type == NAME_NameProperty)
	{
		Ar << TagName;
		return true;
	}
	return false;
}

FGameplayTag FGameplayTag::RequestDirectParent() const
{
	return IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagDirectParent(*this);
}

DECLARE_CYCLE_STAT(TEXT("FGameplayTag::NetSerialize"), STAT_FGameplayTag_NetSerialize, STATGROUP_GameplayTags);

bool FGameplayTag::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Ar.IsSaving())
	{
		UGameplayTagsManager::NotifyTagReplicated(*this, false);
	}
#endif

	NetSerialize_Packed(Ar, Map, bOutSuccess);

	bOutSuccess = true;
	return true;
}

static TSharedPtr<FNetFieldExportGroup> CreateNetfieldExportGroupForNetworkGameplayTags(const UGameplayTagsManager& TagManager, const TCHAR* NetFieldExportGroupName)
{
	TSharedPtr<FNetFieldExportGroup> NetFieldExportGroup = TSharedPtr<FNetFieldExportGroup>(new FNetFieldExportGroup());

	const TArray<TSharedPtr<FGameplayTagNode>>& NetworkGameplayTagNodeIndex = TagManager.GetNetworkGameplayTagNodeIndex();

	NetFieldExportGroup->PathName = NetFieldExportGroupName;
	NetFieldExportGroup->NetFieldExports.SetNum(NetworkGameplayTagNodeIndex.Num());

	for (int32 i = 0; i < NetworkGameplayTagNodeIndex.Num(); i++)
	{
		FNetFieldExport NetFieldExport(
			i,
			0,
			NetworkGameplayTagNodeIndex[i]->GetCompleteTag().ToString(),
			TEXT(""));

		NetFieldExportGroup->NetFieldExports[i] = NetFieldExport;
	}

	return NetFieldExportGroup;
}

bool FGameplayTag::NetSerialize_Packed(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SCOPE_CYCLE_COUNTER(STAT_FGameplayTag_NetSerialize);

	UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();	

	if (TagManager.ShouldUseFastReplication())
	{
		FGameplayTagNetIndex NetIndex = INVALID_TAGNETINDEX;

		UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(Map);
		const bool bIsReplay = PackageMapClient && PackageMapClient->GetConnection() && PackageMapClient->GetConnection()->InternalAck;

		TSharedPtr<FNetFieldExportGroup> NetFieldExportGroup;

		if (bIsReplay)
		{
			// For replays, use a net field export group to guarantee we can send the name reliably (without having to rely on the client having a deterministic NetworkGameplayTagNodeIndex array)
			const TCHAR* NetFieldExportGroupName = TEXT("NetworkGameplayTagNodeIndex");

			// Find this net field export group
			NetFieldExportGroup = PackageMapClient->GetNetFieldExportGroup(NetFieldExportGroupName);

			if (Ar.IsSaving())
			{
				// If we didn't find it, we need to create it (only when saving though, it should be here on load since it was exported at save time)
				if (!NetFieldExportGroup.IsValid())
				{
					NetFieldExportGroup = CreateNetfieldExportGroupForNetworkGameplayTags(TagManager, NetFieldExportGroupName);

					PackageMapClient->AddNetFieldExportGroup(NetFieldExportGroupName, NetFieldExportGroup);
				}

				NetIndex = TagManager.GetNetIndexFromTag(*this);

				if (NetIndex != TagManager.InvalidTagNetIndex && NetIndex != INVALID_TAGNETINDEX)
				{
					PackageMapClient->TrackNetFieldExport(NetFieldExportGroup.Get(), NetIndex);
				}
				else
				{
					NetIndex = INVALID_TAGNETINDEX;		// We can't save InvalidTagNetIndex, since the remote side could have a different value for this
				}
			}

			uint32 NetIndex32 = NetIndex;
			Ar.SerializeIntPacked(NetIndex32);
			NetIndex = NetIndex32;

			if (Ar.IsLoading())
			{
				// Get the tag name from the net field export group entry
				if (NetIndex != INVALID_TAGNETINDEX && ensure(NetFieldExportGroup.IsValid()) && ensure(NetIndex < NetFieldExportGroup->NetFieldExports.Num()))
				{
					TagName = FName(*NetFieldExportGroup->NetFieldExports[NetIndex].Name);

					// Validate the tag name
					const FGameplayTag Tag = IGameplayTagsModule::RequestGameplayTag(TagName, false);

					// Warn (once) if the tag isn't found
					if (!Tag.IsValid() && !NetFieldExportGroup->NetFieldExports[NetIndex].bIncompatible)
					{ 
						UE_LOG(LogGameplayTags, Warning, TEXT( "Gameplay tag not found (marking incompatible): %s"), *TagName.ToString());
						NetFieldExportGroup->NetFieldExports[NetIndex].bIncompatible = true;
					}

					TagName = Tag.TagName;
				}
				else
				{
					TagName = NAME_None;
				}
			}

			bOutSuccess = true;
			return true;
		}

		if (Ar.IsSaving())
		{
			NetIndex = TagManager.GetNetIndexFromTag(*this);
			
			SerializeTagNetIndexPacked(Ar, NetIndex, UGameplayTagsManager::NetIndexFirstBitSegment, UGameplayTagsManager::NetIndexTrueBitNum);
		}
		else
		{
			SerializeTagNetIndexPacked(Ar, NetIndex, UGameplayTagsManager::NetIndexFirstBitSegment, UGameplayTagsManager::NetIndexTrueBitNum);
			TagName = TagManager.GetTagNameFromNetIndex(NetIndex);
		}
	}
	else
	{
		Ar << TagName;
	}

	bOutSuccess = true;
	return true;
}

void FGameplayTag::PostSerialize(const FArchive& Ar)
{
	// This only happens for tags that are not nested inside a container, containers handle redirectors themselves
	// Only do redirects for real loads, not for duplicates or recompiles
	if (Ar.IsLoading() && Ar.IsPersistent() && !(Ar.GetPortFlags() & PPF_Duplicate) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE))
	{
		UGameplayTagsManager& TagManager = IGameplayTagsModule::GetGameplayTagsManager();

		// Rename any tags that may have changed by the ini file.
		TagManager.RedirectSingleGameplayTag(*this, Ar.GetSerializedProperty());
	}
}

bool FGameplayTag::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	FString ImportedTag = TEXT("");
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedTag, true);
	if (!NewBuffer)
	{
		// Failed to read buffer. Maybe normal ImportText will work.
		return false;
	}

	if (ImportedTag == TEXT("None") || ImportedTag.IsEmpty())
	{
		// TagName was none
		TagName = NAME_None;
		return true;
	}

	if (ImportedTag[0] == '(')
	{
		// Let normal ImportText handle this. It appears to be prepared for it.
		return false;
	}

	FName ImportedTagName = FName(*ImportedTag);
	if (IGameplayTagsModule::Get().GetGameplayTagsManager().ValidateTagCreation(ImportedTagName))
	{
		// We found the tag. Assign it here.
		TagName = ImportedTagName;
		return true;
	}

	// Let normal ImportText try.
	return false;
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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static void GameplayTagPrintReplicationMap()
{
	UGameplayTagsManager::PrintReplicationFrequencyReport();
}

FAutoConsoleCommand GameplayTagPrintReplicationMapCmd(
	TEXT("GameplayTags.PrintReport"), 
	TEXT( "Prints frequency of gameplay tags" ), 
	FConsoleCommandDelegate::CreateStatic(GameplayTagPrintReplicationMap)
);


static void TagPackingTest()
{
	for (int32 TotalNetIndexBits=1; TotalNetIndexBits <= 16; ++TotalNetIndexBits)
	{
		for (int32 NetIndexBitsPerComponent=0; NetIndexBitsPerComponent <= TotalNetIndexBits; NetIndexBitsPerComponent++)
		{
			for (int32 NetIndex=0; NetIndex < FMath::Pow(2, TotalNetIndexBits); ++NetIndex)
			{
				FGameplayTagNetIndex NI = NetIndex;

				FNetBitWriter	BitWriter(nullptr, 1024 * 8);
				SerializeTagNetIndexPacked(BitWriter, NI, NetIndexBitsPerComponent, TotalNetIndexBits);

				FNetBitReader	Reader(nullptr, BitWriter.GetData(), BitWriter.GetNumBits());

				FGameplayTagNetIndex NewIndex;
				SerializeTagNetIndexPacked(Reader, NewIndex, NetIndexBitsPerComponent, TotalNetIndexBits);

				if (ensureAlways((NewIndex == NI)) == false)
				{
					NetIndex--;
					continue;
				}
			}
		}
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("TagPackingTest completed!"));

}

FAutoConsoleCommand TagPackingTestCmd(
	TEXT("GameplayTags.PackingTest"), 
	TEXT( "Prints frequency of gameplay tags" ), 
	FConsoleCommandDelegate::CreateStatic(TagPackingTest)
);

#endif
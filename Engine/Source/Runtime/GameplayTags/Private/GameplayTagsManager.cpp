// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"
#include "GameplayTagsSettings.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#define LOCTEXT_NAMESPACE "GameplayTagManager"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TMap<FGameplayTag, int32>	UGameplayTagsManager::ReplicationCountMap;
TMap<FGameplayTag, int32>	UGameplayTagsManager::ReplicationCountMap_SingleTags;
TMap<FGameplayTag, int32>	UGameplayTagsManager::ReplicationCountMap_Containers;
#endif

int32 UGameplayTagsManager::NetIndexFirstBitSegment=16;
int32 UGameplayTagsManager::NetIndexTrueBitNum=16;
int32 UGameplayTagsManager::NumBitsForContainerSize=6;

UGameplayTagsManager::UGameplayTagsManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	RegisteredObjectReimport = false;
#endif
}

void UGameplayTagsManager::LoadGameplayTagTables(TArray<FString>& TagTableNames)
{
	if (GameplayTagTables.Num() == 0 && TagTableNames.Num() > 0)
	{
		for (auto It(TagTableNames.CreateConstIterator()); It; ++It)
		{
			const FString& FileName = *It;
			UDataTable* TagTable = LoadObject<UDataTable>(NULL, *FileName, NULL, LOAD_None, NULL);

			// Handle case where the module is dynamically-loaded within a LoadPackage stack, which would otherwise
			// result in the tag table not having its RowStruct serialized in time. Without the RowStruct, the tags manager
			// will not be initialized correctly.
			if (TagTable && IsLoading())
			{
				FLinkerLoad* TagLinker = TagTable->GetLinker();
				if (TagLinker)
				{
					TagTable->GetLinker()->Preload(TagTable);
				}
			}
			GameplayTagTables.Add(TagTable);
		}
	}

#if WITH_EDITOR
	// Hook into notifications for object re-imports so that the gameplay tag tree can be reconstructed if the table changes
	if (GIsEditor && GameplayTagTables.Num() > 0 && !RegisteredObjectReimport)
	{
		RegisteredObjectReimport = true;
		FEditorDelegates::OnAssetPostImport.AddUObject(this, &UGameplayTagsManager::OnObjectReimported);
	}
#endif
}

void UGameplayTagsManager::GetAllNodesForTag_Recurse(TArray<FString>& Tags, int32 CurrentTagDepth, TSharedPtr<FGameplayTagNode> CurrentTagNode, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const
{
	CurrentTagDepth++;
	if(Tags.Num() > CurrentTagDepth)
	{
		// search for the subsequent tags in the hierarchy
		TArray< TSharedPtr<FGameplayTagNode> > CurrentChildrenTags;
		CurrentChildrenTags = CurrentTagNode->GetChildTagNodes();
		for(int32 TagIdx = 0; TagIdx < CurrentChildrenTags.Num(); TagIdx++)
		{
			FString CurrentTagName = CurrentChildrenTags[TagIdx].Get()->GetSimpleTag().ToString();
			if(CurrentTagName == Tags[CurrentTagDepth])
			{
				CurrentTagNode = CurrentChildrenTags[TagIdx];
				OutTagArray.Add(CurrentTagNode);
				GetAllNodesForTag_Recurse(Tags, CurrentTagDepth, CurrentTagNode, OutTagArray);
				break;
			}
		}
	}
}

void UGameplayTagsManager::GetAllNodesForTag( const FString& Tag, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray ) const
{
	TArray<FString> Tags;
	TArray<TSharedPtr<FGameplayTagNode>>& GameplayRootTags = GameplayRootTag->GetChildTagNodes();
	OutTagArray.Empty();
	if(Tag.ParseIntoArray( Tags, TEXT( "." ), true ) > 0)
	{
		int32 CurrentTagDepth = 0;
		// find the first node of the tag
		TSharedPtr<FGameplayTagNode> CurrentTagNode = NULL;
		for(int32 TagIdx = 0; TagIdx < GameplayRootTags.Num(); TagIdx++)
		{
			FString TagName = GameplayRootTags[TagIdx].Get()->GetSimpleTag().ToString();
			if(TagName == Tags[CurrentTagDepth])
			{
				CurrentTagNode = GameplayRootTags[TagIdx];
				break;
			}
		}

		if(CurrentTagNode.IsValid())
		{
			// add it to the list of tags
			OutTagArray.Add(CurrentTagNode);
			GetAllNodesForTag_Recurse(Tags, CurrentTagDepth, CurrentTagNode, OutTagArray);
		}
	}	
}

struct FCompareFGameplayTagNodeByTag
{
	FORCEINLINE bool operator()( const TSharedPtr<FGameplayTagNode>& A, const TSharedPtr<FGameplayTagNode>& B ) const
	{
		return (A->GetSimpleTag().Compare(B->GetSimpleTag())) < 0;
	}
};

void UGameplayTagsManager::ConstructGameplayTagTree()
{
	if (!GameplayRootTag.IsValid())
	{
		GameplayRootTag = MakeShareable(new FGameplayTagNode());
		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Construct from data asset"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		
			for (auto It(GameplayTagTables.CreateIterator()); It; It++)
			{
				if (*It)
				{
					PopulateTreeFromDataTable(*It);
				}
			}
		}

		if (ShouldImportTagsFromINI())
		{
			UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: ImportINI"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			// ------------------------------------------------------------------------------------------------------------
			// Update path: Check for old tags in DefaultEngine.ini (we'll push them to the UGameplayTagSettings class).
			// ------------------------------------------------------------------------------------------------------------
			TArray<FString> EngineConfigTags;
			GConfig->GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("GameplayTags"), EngineConfigTags, GEngineIni);
			
			if (EngineConfigTags.Num() > 0)
			{
				if (MutableDefault->GameplayTags.Num() == 0)
				{
					MutableDefault->GameplayTags.Append(EngineConfigTags);
				}
			}

			// ---------------------------------------------------------------
			//	Developer Tags
			// ---------------------------------------------------------------

			{
				// Read all tags from the ini
				DeveloperTags.Items.Reset();
				TArray<FString> FilesInDirectory;
				IFileManager::Get().FindFilesRecursive(FilesInDirectory, *(FPaths::GameConfigDir() / TEXT("Tags")), TEXT("*.ini"), true, false);
				for (FString& FileName : FilesInDirectory)
				{
					UE_LOG(LogGameplayTags, Display, TEXT("File: %s"), *FileName);

					auto& NewItem = DeveloperTags.Items[DeveloperTags.Items.AddDefaulted()];
					NewItem.IniName = FileName;

					GConfig->GetArray(TEXT("UserTags"), TEXT("GameplayTags"), NewItem.Tags, FileName);

					MutableDefault->GameplayTags.Append(NewItem.Tags);
				}
			}
			

			// ---------------------------------------------------------------

			// Load any GameplayTagSettings from config (their default object)
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* Class = *ClassIt;
				if (!Class->IsChildOf<UGameplayTagsSettings>() || Class->HasAnyClassFlags(CLASS_Abstract))
				{
					continue;
				}

#if WITH_EDITOR
				Class->GetDefaultObject<UGameplayTagsSettings>()->SortTags();
#endif
				for (FString& TagStr : Class->GetDefaultObject<UGameplayTagsSettings>()->GameplayTags)
				{
					FGameplayTagTableRow TableRow;
					TableRow.Tag = TagStr;
					AddTagTableRow(TableRow);
				}

				// Grab the commonly replicated tags
				CommonlyReplicatedTags.Reserve(CommonlyReplicatedTags.Num() + Class->GetDefaultObject<UGameplayTagsSettings>()->CommonlyReplicatedTags.Num());
				for (const FString& Str : Class->GetDefaultObject<UGameplayTagsSettings>()->CommonlyReplicatedTags)
				{
					FGameplayTag Tag = RequestGameplayTag(FName(*Str));
					if (Tag.IsValid())
					{
						CommonlyReplicatedTags.Add(Tag);
					}
					else
					{
						UE_LOG(LogGameplayTags, Warning, TEXT("%s was found in the CommonlyReplicatedTags list but doesn't appear to be a valid tag!"), *Str);
					}
				}

				NetIndexFirstBitSegment = Class->GetDefaultObject<UGameplayTagsSettings>()->NetIndexFirstBitSegment;
			}
			GameplayRootTag->GetChildTagNodes().Sort(FCompareFGameplayTagNodeByTag());
		}

		bUseFastReplication = false;
		GConfig->GetBool(TEXT("GameplayTags"), TEXT("FastReplication"), bUseFastReplication, GEngineIni);

		GConfig->GetInt(TEXT("GameplayTags"), TEXT("NumBitsForContainerSize"), NumBitsForContainerSize, GEngineIni);

		if (ShouldUseFastReplication())
		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Reconstruct NetIndex"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			ConstructNetIndex();
		}

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: GameplayTagTreeChangedEvent.Broadcast"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			GameplayTagTreeChangedEvent.Broadcast();
		}

		// Update the TagRedirects map
		TagRedirects.Empty();
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, true, GEngineIni);
		for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
		{
			if (It.Key() == TEXT("GameplayTagRedirects"))
			{
				FName OldTagName = NAME_None;
				FName NewTagName;

				if (FParse::Value(*It.Value().GetValue(), TEXT("OldTagName="), OldTagName))
				{
					if (FParse::Value(*It.Value().GetValue(), TEXT("NewTagName="), NewTagName))
					{
						if (ensureMsgf(!TagRedirects.Contains(OldTagName), TEXT("Old tag %s is being redirected to more than one tag. Please remove all the redirections except for one."), *OldTagName.ToString()))
						{
							FGameplayTag OldTag = RequestGameplayTag(OldTagName, false); //< This only succeeds if OldTag is in the Table!
							if (OldTag.IsValid())
							{
								UE_LOG(LogGameplayTags, Warning,
									TEXT("Old tag (%s) which is being redirected still exists in the table!  Generally you should "
									TEXT("remove the old tags from the table when you are redirecting to new tags, or else users will ")
									TEXT("still be able to add the old tags to containers.")), *OldTagName.ToString()
									);
							}

							FGameplayTag NewTag = (NewTagName != NAME_None) ? RequestGameplayTag(NewTagName, false) : FGameplayTag();
							if (!NewTag.IsValid() && NewTagName != NAME_None)
							{
								UE_LOG(LogGameplayTags, Warning, TEXT("Invalid new tag %s!  Cannot replace old tag %s."),
									*NewTagName.ToString(), *OldTagName.ToString());
							}
							else
							{
								// Populate the map
								TagRedirects.Add(OldTagName, NewTag);
							}
						}
					}
				}
			}
		}
	}
}

void UGameplayTagsManager::ConstructNetIndex()
{
	NetworkGameplayTagNodeIndex.Empty();

	GameplayTagNodeMap.GenerateValueArray(NetworkGameplayTagNodeIndex);

	NetworkGameplayTagNodeIndex.Sort(FCompareFGameplayTagNodeByTag());

	// Put the common indices up front
	for (int32 CommonIdx=0; CommonIdx < CommonlyReplicatedTags.Num(); ++CommonIdx)
	{
		int32 BaseIdx=0;
		FGameplayTag& Tag = CommonlyReplicatedTags[CommonIdx];

		bool Found = false;
		for (int32 findidx=0; findidx < NetworkGameplayTagNodeIndex.Num(); ++findidx)
		{
			if (NetworkGameplayTagNodeIndex[findidx]->GetCompleteTag() == Tag.GetTagName())
			{
				NetworkGameplayTagNodeIndex.Swap(findidx, CommonIdx);
				Found = true;
				break;
			}
		}

		// A non fatal error should have been thrown when parsing the CommonlyReplicatedTags list. If we make it here, something is seriously wrong.
		checkf( Found, TEXT("Tag %s not found in NetworkGameplayTagNodeIndex"), *Tag.ToString() );
	}

	InvalidTagNetIndex = NetworkGameplayTagNodeIndex.Num()+1;
	NetIndexTrueBitNum = FMath::CeilToInt(FMath::Log2(InvalidTagNetIndex));
	
	// This should never be smaller than NetIndexTrueBitNum
	NetIndexFirstBitSegment = FMath::Min<int64>(NetIndexFirstBitSegment, NetIndexTrueBitNum);

	// This is now sorted and it should be the same on both client and server
	if (NetworkGameplayTagNodeIndex.Num() >= INVALID_TAGNETINDEX)
	{
		ensureMsgf(false, TEXT("Too many tags in dictionary for networking! Remove tags or increase tag net index size"));

		NetworkGameplayTagNodeIndex.SetNum(INVALID_TAGNETINDEX - 1);
	}

	for (FGameplayTagNetIndex i = 0; i < NetworkGameplayTagNodeIndex.Num(); i++)
	{
		if (NetworkGameplayTagNodeIndex[i].IsValid())
		{
			NetworkGameplayTagNodeIndex[i]->NetIndex = i;
		}
	}
}

FName UGameplayTagsManager::GetTagNameFromNetIndex(FGameplayTagNetIndex Index)
{
	if (Index >= NetworkGameplayTagNodeIndex.Num())
	{
		// Ensure Index is the invalid index. If its higher than that, then something is wrong.
		ensureMsgf(Index == InvalidTagNetIndex, TEXT("Received invalid tag net index %d! Tag index is out of sync on client!"), Index);
		return NAME_None;
	}
	return NetworkGameplayTagNodeIndex[Index]->GetCompleteTag();
}

FGameplayTagNetIndex UGameplayTagsManager::GetNetIndexFromTag(const FGameplayTag &InTag)
{
	const TSharedPtr<FGameplayTagNode>* GameplayTagNode = GameplayTagNodeMap.Find(InTag);

	if (GameplayTagNode && GameplayTagNode->IsValid())
	{
		return (*GameplayTagNode)->GetNetIndex();
	}

	return InvalidTagNetIndex;
}

bool UGameplayTagsManager::ShouldImportTagsFromINI()
{
	bool ImportFromINI = false;
	GConfig->GetBool(TEXT("GameplayTags"), TEXT("ImportTagsFromConfig"), ImportFromINI, GEngineIni);
	return ImportFromINI;
}

/** TEMP - Returns true if we should warn on invalid (missing) tags */
bool UGameplayTagsManager::ShouldWarnOnInvalidTags()
{
	bool ImportFromINI = true;
	GConfig->GetBool(TEXT("GameplayTags"), TEXT("WarnOnInvalidTags"), ImportFromINI, GEngineIni);
	return ImportFromINI;
}

void UGameplayTagsManager::RedirectTagsForContainer(FGameplayTagContainer& Container, TSet<FName>& DeprecatedTagNamesNotFoundInTagMap, UProperty* SerializingProperty)
{
	TSet<FName> NamesToRemove;
	TSet<const FGameplayTag*> TagsToAdd;

	// First populate the NamesToRemove and TagsToAdd sets by finding tags in the container that have redirects
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName TagName = TagIt->GetTagName();
		const FGameplayTag* NewTag = TagRedirects.Find(TagName);
		if (NewTag)
		{
			NamesToRemove.Add(TagName);
			if (NewTag->IsValid())
			{
				TagsToAdd.Add(NewTag);
				DeprecatedTagNamesNotFoundInTagMap.Remove(NewTag->GetTagName());
			}
		}
#if WITH_EDITOR
		else
		{
			// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
			FGameplayTag OldTag = RequestGameplayTag(TagName, false);
			if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
			{
				UE_LOG(LogGameplayTags, Warning, TEXT("Invalid GameplayTag %s found while loading property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
			}
		}
#endif
	}

	// Add additional tags to the TagsToAdd set from the deprecated list if they weren't already added above
	for (FName AdditionalDeprecatedTag : DeprecatedTagNamesNotFoundInTagMap)
	{
		const FGameplayTag* NewTag = TagRedirects.Find(AdditionalDeprecatedTag);
		if (NewTag && NewTag->IsValid())
		{
			TagsToAdd.Add(NewTag);
		}
	}

	// Remove all tags from the NamesToRemove set
	for (FName RemoveName : NamesToRemove)
	{
		FGameplayTag OldTag = RequestGameplayTag(RemoveName, false);
		if (OldTag.IsValid())
		{
			Container.RemoveTag(OldTag);
		}
		else
		{
			FGameplayTagRedirectHelper::RemoveTagByExplicitName(Container, RemoveName);
		}
	}

	// Add all tags from the TagsToAdd set
	for (const FGameplayTag* AddTag : TagsToAdd)
	{
		check(AddTag);
		Container.AddTag(*AddTag);
	}
}

void UGameplayTagsManager::RedirectSingleGameplayTag(FGameplayTag& Tag, UProperty* SerializingProperty)
{
	const FName TagName = Tag.GetTagName();
	const FGameplayTag* NewTag = TagRedirects.Find(TagName);
	if (NewTag)
	{
		if (NewTag->IsValid())
		{
			Tag = *NewTag;
		}
	}
#if WITH_EDITOR
	else if (TagName != NAME_None)
	{
		// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
		FGameplayTag OldTag = RequestGameplayTag(TagName, false);
		if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
		{
			UE_LOG(LogGameplayTags, Warning, TEXT("Invalid GameplayTag %s found while loading property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
		}
	}
#endif
}

void UGameplayTagsManager::PopulateTreeFromDataTable(class UDataTable* InTable)
{
	checkf(GameplayRootTag.IsValid(), TEXT("ConstructGameplayTagTree() must be called before PopulateTreeFromDataTable()"));
	static const FString ContextString(TEXT("UGameplayTagsManager::PopulateTreeFromDataTable"));
	
	TArray<FGameplayTagTableRow*> TagTableRows;
	InTable->GetAllRows<FGameplayTagTableRow>(ContextString, TagTableRows);

	for (const FGameplayTagTableRow* TagRow : TagTableRows)
	{
		if (TagRow)
		{
			AddTagTableRow(*TagRow);
		}
	}
	GameplayRootTag->GetChildTagNodes().Sort(FCompareFGameplayTagNodeByTag());
}

void UGameplayTagsManager::AddTagTableRow(const FGameplayTagTableRow& TagRow)
{
	TArray< TSharedPtr<FGameplayTagNode> >& GameplayRootTags = GameplayRootTag->GetChildTagNodes();

	// Split the tag text on the "." delimiter to establish tag depth and then insert each tag into the
	// gameplay tag tree
	TArray<FString> SubTags;
	TagRow.Tag.ParseIntoArray(SubTags, TEXT("."), true);

	if (SubTags.Num() > 0)
	{
		int32 InsertionIdx = InsertTagIntoNodeArray(*SubTags[0], NULL, GameplayRootTags, SubTags.Num() == 1 ? TagRow.CategoryText : FText());
		TSharedPtr<FGameplayTagNode> CurNode = GameplayRootTags[InsertionIdx];

		for (int32 SubTagIdx = 1; SubTagIdx < SubTags.Num(); ++SubTagIdx)
		{
			TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();
			FText Description;
			if (SubTagIdx == SubTags.Num() - 1)
			{
				Description = TagRow.CategoryText;
			}
			InsertionIdx = InsertTagIntoNodeArray(*SubTags[SubTagIdx], CurNode, ChildTags, Description);

			CurNode = ChildTags[InsertionIdx];
		}
	}
}

UGameplayTagsManager::~UGameplayTagsManager()
{
	DestroyGameplayTagTree();
}

void UGameplayTagsManager::DestroyGameplayTagTree()
{
	if (GameplayRootTag.IsValid())
	{
		GameplayRootTag->ResetNode();
		GameplayRootTag.Reset();
	}
}

int32 UGameplayTagsManager::InsertTagIntoNodeArray(FName Tag, TWeakPtr<FGameplayTagNode> ParentNode, TArray< TSharedPtr<FGameplayTagNode> >& NodeArray, FText CategoryDescription)
{
	int32 InsertionIdx = INDEX_NONE;

	// See if the tag is already in the array
	for (int32 CurIdx = 0; CurIdx < NodeArray.Num(); ++CurIdx)
	{
		if (NodeArray[CurIdx].IsValid() && NodeArray[CurIdx].Get()->GetSimpleTag() == Tag)
		{
			InsertionIdx = CurIdx;
			break;
		}
	}

	// Insert the tag at the end of the array if not already found
	if (InsertionIdx == INDEX_NONE)
	{
		TSharedPtr<FGameplayTagNode> TagNode = MakeShareable(new FGameplayTagNode(Tag, ParentNode, CategoryDescription));
		InsertionIdx = NodeArray.Add(TagNode);

		FGameplayTag GameplayTag = FGameplayTag(TagNode->GetCompleteTag());

		{
#if WITH_EDITOR
			// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
			// This function is not generically threadsafe.
			FScopeLock Lock(&GameplayTagMapCritical);
#endif
			GameplayTagMap.Add(TagNode->GetCompleteTag(), GameplayTag);
		}

		GameplayTagNodeMap.Add(GameplayTag, TagNode);

		TSet<FName> ParentSet;
		GetAllParentNodeNames(ParentSet, TagNode);
		for (auto Name : ParentSet)
		{
			TagNode->Parents.AddTagFast(FGameplayTag(Name));
		}
	}
	else if (NodeArray[InsertionIdx]->CategoryDescription.IsEmpty() && !CategoryDescription.IsEmpty())
	{
		// Fill in category description for nodes that were added by virtue of being a parent node
		NodeArray[InsertionIdx]->CategoryDescription = CategoryDescription;
	}

	return InsertionIdx;
}

int32 UGameplayTagsManager::GetBestTagCategoryDescription(FString Tag, FText& OutDescription) const
{
	// get all the nodes that make up this tag
	TArray< TSharedPtr<FGameplayTagNode> > TagItems;
	GetAllNodesForTag(Tag, TagItems);
	
	// find the deepest tag in the list (last is deepest based on how this array was constructed)
	TSharedPtr<FGameplayTagNode> BestDescriptionNode;
	for(int32 TagIdx = TagItems.Num() - 1; TagIdx >= 0; TagIdx--)
	{
		OutDescription = TagItems[TagIdx]->GetCategoryDescription();
		if(!OutDescription.IsEmpty())
		{
			return TagIdx;
		}
	}

	return -1;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UGameplayTagsManager::PrintReplicationFrequencyReport()
{
	UE_LOG(LogGameplayTags, Warning, TEXT("================================="));
	UE_LOG(LogGameplayTags, Warning, TEXT("Gameplay Tags Replication Report"));

	UE_LOG(LogGameplayTags, Warning, TEXT("\nTags replicated solo:"));
	ReplicationCountMap_SingleTags.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_SingleTags)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}
	
	// ---------------------------------------

	UE_LOG(LogGameplayTags, Warning, TEXT("\nTags replicated in containers:"));
	ReplicationCountMap_Containers.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_Containers)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	// ---------------------------------------

	UE_LOG(LogGameplayTags, Warning, TEXT("\nAll Tags replicated:"));
	ReplicationCountMap.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	TMap<int32, int32> SavingsMap;
	int32 BaselineCost = 0;
	for (int32 Bits=1; Bits < NetIndexTrueBitNum; ++Bits)
	{
		int32 TotalSavings = 0;
		BaselineCost = 0;

		FGameplayTagNetIndex ExpectedNetIndex=0;
		for (auto& It : ReplicationCountMap)
		{
			int32 ExpectedCostBits = 0;
			bool FirstSeg = ExpectedNetIndex < FMath::Pow(2, Bits);
			if (FirstSeg)
			{
				// This would fit in the first Bits segment
				ExpectedCostBits = Bits+1;
			}
			else
			{
				// Would go in the second segment, so we pay the +1 cost
				ExpectedCostBits = NetIndexTrueBitNum+1;
			}

			int32 Savings = (NetIndexTrueBitNum - ExpectedCostBits) * It.Value;
			BaselineCost += NetIndexTrueBitNum * It.Value;

			//UE_LOG(LogGameplayTags, Warning, TEXT("[Bits: %d] Tag %s would save %d bits"), Bits, *It.Key.ToString(), Savings);
			ExpectedNetIndex++;
			TotalSavings += Savings;
		}

		SavingsMap.FindOrAdd(Bits) = TotalSavings;
	}

	SavingsMap.ValueSort(TGreater<int32>());
	int32 BestBits = 0;
	for (auto& It : SavingsMap)
	{
		if (BestBits == 0)
		{
			BestBits = It.Key;
		}

		UE_LOG(LogGameplayTags, Warning, TEXT("%d bits would save %d (%.2f)"), It.Key, It.Value, (float)It.Value / (float)BaselineCost);
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("\nSuggested config:"));

	// Write out a nice copy pastable config
	int32 Count=0;
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("+CommonlyReplicatedTags=%s"), *It.Key.ToString());

		if (Count == FMath::Pow(2, BestBits))
		{
			// Print a blank line out, indicating tags after this are not necessary but still may be useful if the user wants to manually edit the list.
			UE_LOG(LogGameplayTags, Warning, TEXT(""));
		}

		if (Count++ >= FMath::Pow(2, BestBits+1))
		{
			break;
		}
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("NetIndexFirstBitSegment=%d"), BestBits);

	UE_LOG(LogGameplayTags, Warning, TEXT("================================="));
}

void UGameplayTagsManager::NotifyTagReplicated(FGameplayTag Tag, bool WasInContainer)
{
	ReplicationCountMap.FindOrAdd(Tag)++;

	if (WasInContainer)
	{
		ReplicationCountMap_Containers.FindOrAdd(Tag)++;
	}
	else
	{
		ReplicationCountMap_SingleTags.FindOrAdd(Tag)++;
	}
	
}
#endif

#if WITH_EDITOR

static void RecursiveRootTagSearch(const FString& InFilterString, const TArray<TSharedPtr<FGameplayTagNode> >& GameplayRootTags, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray)
{
	FString CurrentFilter, RestOfFilter;
	if (!InFilterString.Split(TEXT("."), &CurrentFilter, &RestOfFilter))
	{
		CurrentFilter = InFilterString;
	}

	for (int32 iTag = 0; iTag < GameplayRootTags.Num(); ++iTag)
	{
		FString RootTagName = GameplayRootTags[iTag].Get()->GetSimpleTag().ToString();

		if (RootTagName.Equals(CurrentFilter) == true)
		{
			if (RestOfFilter.IsEmpty())
			{
				// We've reached the end of the filter, add tags
				OutTagArray.Add(GameplayRootTags[iTag]);
			}
			else
			{
				// Recurse into our children
				RecursiveRootTagSearch(RestOfFilter, GameplayRootTags[iTag]->GetChildTagNodes(), OutTagArray);
			}
		}		
	}
}

void UGameplayTagsManager::GetFilteredGameplayRootTags( const FString& InFilterString, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray )
{
	TArray<FString> Filters;
	TArray<TSharedPtr<FGameplayTagNode>>& GameplayRootTags = GameplayRootTag->GetChildTagNodes();

	OutTagArray.Empty();
	if( InFilterString.ParseIntoArray( Filters, TEXT( "," ), true ) > 0 )
	{
		// Check all filters in the list
		for (int32 iFilter = 0; iFilter < Filters.Num(); ++iFilter)
		{
			RecursiveRootTagSearch(Filters[iFilter], GameplayRootTags, OutTagArray);
		}
	}
	else
	{
		// No Filters just return them all
		OutTagArray = GameplayRootTags;
	}
}

void UGameplayTagsManager::OnObjectReimported(UFactory* ImportFactory, UObject* InObject)
{
	// Re-construct the gameplay tag tree if the base table is re-imported
	if (GIsEditor && !IsRunningCommandlet() && InObject && GameplayTagTables.Contains(Cast<UDataTable>(InObject)))
	{
		DestroyGameplayTagTree();
		ConstructGameplayTagTree();
	}
}

TSharedPtr<FGameplayTagNode> UGameplayTagsManager::FindTagNode(FName TagName) const
{
	return FindTagNode(GameplayRootTag, TagName);
}

void GameplayTagsUpdateSourceControl(FString RelativeConfigFilePath)
{
	FString ConfigPath = FPaths::ConvertRelativePathToFull(RelativeConfigFilePath);

	if (ISourceControlModule::Get().IsEnabled())
	{
		FText ErrorMessage;

		if (!SourceControlHelpers::CheckoutOrMarkForAdd(ConfigPath, FText::FromString(ConfigPath), NULL, ErrorMessage))
		{
			FNotificationInfo Info(ErrorMessage);
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
	else
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ConfigPath, false))
		{
			FText NotificationErrorText = FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(ConfigPath));

			FNotificationInfo Info(NotificationErrorText);
			Info.ExpireDuration = 3.0f;

			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

void UGameplayTagsManager::AddNewGameplayTagToINI(FString NewTag)
{
	if(NewTag.IsEmpty())
	{
		return;
	}

	if (ShouldImportTagsFromINI() == false)
	{
		return;
	}

	
	UGameplayTagsSettings*			Settings = GetMutableDefault<UGameplayTagsSettings>();
	UGameplayTagsDeveloperSettings* DevSettings = GetMutableDefault<UGameplayTagsDeveloperSettings>();

	if (Settings)
	{
		FString RelativeConfigFilePath = Settings->GetDefaultConfigFilename();
		if (DevSettings && DevSettings->DeveloperConfigName.IsEmpty() == false)
		{
			// Write this tag to the Developer's ini file
			FDeveloperTags& DeveloperTags = IGameplayTagsModule::Get().GetGameplayTagsManager().DeveloperTags;

			// Get config filename
			RelativeConfigFilePath.ReplaceInline(TEXT("DefaultGameplayTags"), *FString::Printf(TEXT("Tags/%s"), *DevSettings->DeveloperConfigName));

			// See if there is already an entry for this one
			int32 idx=0;
			for(; idx < DeveloperTags.Items.Num(); ++idx)
			{
				if (DeveloperTags.Items[idx].IniName == RelativeConfigFilePath)
				{
					break;
				}
			}

			// Create new entry if necessary
			if (DeveloperTags.Items.IsValidIndex(idx) == false)
			{
				idx = DeveloperTags.Items.AddDefaulted();
			}

			// Setup the entry and add the tag
			DeveloperTags.Items[idx].IniName = RelativeConfigFilePath;
			DeveloperTags.Items[idx].Tags.Add(NewTag);

			// Write back out to config
			GConfig->SetArray(TEXT("UserTags"), TEXT("GameplayTags"), DeveloperTags.Items[idx].Tags, RelativeConfigFilePath);

			// Flush once to possible write the file to disk for the first time
			GConfig->Flush(false, RelativeConfigFilePath);

			// Source Control
			GameplayTagsUpdateSourceControl(RelativeConfigFilePath);

			// Flush again in the case where the file already existed
			GConfig->Flush(false, RelativeConfigFilePath);
			
		}
		else
		{
			// Write this tag to the global ini file
			Settings->GameplayTags.Add(NewTag);
			Settings->SortTags();

			// Backup the full list before dumping to config
			TArray<FString> Backup = Settings->GameplayTags;

			// Remove DeveloperTags from the master list
			FDeveloperTags& DeveloperTags = IGameplayTagsModule::Get().GetGameplayTagsManager().DeveloperTags;
			for(int32 idx=0; idx < DeveloperTags.Items.Num(); ++idx)
			{
				for (FString& Str : DeveloperTags.Items[idx].Tags)
				{
					Settings->GameplayTags.Remove(Str);
				}
			}			
						
			// Source Control
			GameplayTagsUpdateSourceControl(RelativeConfigFilePath);

			Settings->UpdateDefaultConfigFile();
			
			GConfig->Flush(false, RelativeConfigFilePath);

			// Restore
			Settings->GameplayTags = Backup;
		}

		// ------------------------------------------------

		

		// ------------------------------------------------

		IGameplayTagsModule::Get().GetGameplayTagsManager().DestroyGameplayTagTree();

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("ConstructGameplayTagTree GameplayTag tables after adding new tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			IGameplayTagsModule::Get().GetGameplayTagsManager().ConstructGameplayTagTree();
		}
	}
	
}

#endif // WITH_EDITOR

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::RequestGameplayTag"), STAT_UGameplayTagsManager_RequestGameplayTag, STATGROUP_GameplayTags);

FGameplayTag UGameplayTagsManager::RequestGameplayTag(FName TagName, bool ErrorIfNotFound) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_RequestGameplayTag);
#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&GameplayTagMapCritical);
#endif

	const FGameplayTag* Tag = GameplayTagMap.Find(TagName);
	if (!Tag)
	{ 
		if (ErrorIfNotFound)
		{
			static TSet<FName> MissingTagName;
			if (!MissingTagName.Contains(TagName))
			{
				ensureAlwaysMsgf(false, TEXT("Requested Tag %s was not found. Check tag data table."), *TagName.ToString());
				MissingTagName.Add(TagName);
			}
		}
		return FGameplayTag();
	}
	return *Tag;
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagParents(const FGameplayTag& GameplayTag) const
{
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(GameplayTag);
	AddParentTags(TagContainer, GameplayTag);
	return TagContainer;
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagChildren(const FGameplayTag& GameplayTag) const
{
	FGameplayTagContainer TagContainer;
	// Note this purposefully does not include the passed in GameplayTag in the container.
	AddChildrenTags(TagContainer, GameplayTag, true);
	return TagContainer;
}

FGameplayTag UGameplayTagsManager::RequestGameplayTagDirectParent(const FGameplayTag& GameplayTag) const
{
	const TSharedPtr<FGameplayTagNode>* GameplayTagNode = GameplayTagNodeMap.Find(GameplayTag);
	if (GameplayTagNode)
	{
		TSharedPtr<FGameplayTagNode> Parent = (*GameplayTagNode)->GetParentTagNode().Pin();
		if (Parent.IsValid())
		{
			const FGameplayTag* Tag = GameplayTagNodeMap.FindKey(Parent);
			if (Tag)
			{
				return *Tag;
			}
		}
	}
	return FGameplayTag();
}

bool UGameplayTagsManager::AddLeafTagToContainer(FGameplayTagContainer& TagContainer, const FGameplayTag& Tag)
{
	// Check tag is not already in container
	if (TagContainer.HasTag(Tag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
	{
		return true;
	}

	// Check tag is not just a parent of an existing tag in the container
	for (auto It = TagContainer.CreateConstIterator(); It; ++It)
	{
		FGameplayTagContainer ParentTags = RequestGameplayTagParents(*It);
		if (ParentTags.HasTag(Tag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
		{
			return false;
		}
	}

	// Remove any tags in the container that are a parent to this tag
	FGameplayTagContainer ParentTags = RequestGameplayTagParents(Tag);
	for (auto It = ParentTags.CreateConstIterator(); It; ++It)
	{
		if (TagContainer.HasTag(*It, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
		{
			TagContainer.RemoveTag(*It);
		}
	}

	// Add the tag
	TagContainer.AddTag(Tag);
	return true;
}

DECLARE_CYCLE_STAT(TEXT("AddParentTags"), STAT_AddParentTags, STATGROUP_GameplayTags);


void UGameplayTagsManager::AddParentTags(FGameplayTagContainer& TagContainer, const FGameplayTag& GameplayTag) const
{
	SCOPE_CYCLE_COUNTER(STAT_AddParentTags);

	const TSharedPtr<FGameplayTagNode>* GameplayTagNode = GameplayTagNodeMap.Find(GameplayTag);
	if (GameplayTagNode)
	{
		TSharedPtr<FGameplayTagNode> Parent = (*GameplayTagNode)->GetParentTagNode().Pin();
		if (Parent.IsValid())
		{
			const FGameplayTag* Tag = GameplayTagNodeMap.FindKey(Parent);
			if (Tag)
			{
				TagContainer.AddTag(*Tag);
				AddParentTags(TagContainer, *Tag);
			}
		}
	}
}

void UGameplayTagsManager::AddChildrenTags(FGameplayTagContainer& TagContainer, const FGameplayTag& GameplayTag, bool RecurseAll) const
{
	const TSharedPtr<FGameplayTagNode>* GameplayTagNode = GameplayTagNodeMap.Find(GameplayTag);
	if (GameplayTagNode)
	{
		TArray< TSharedPtr<FGameplayTagNode> >& ChildrenNodes = (*GameplayTagNode)->GetChildTagNodes();
		for (TSharedPtr<FGameplayTagNode> ChildNode : ChildrenNodes)
		{
			if (ChildNode.IsValid())
			{
				const FGameplayTag* Tag = GameplayTagNodeMap.FindKey(ChildNode);
				if (Tag)
				{
					TagContainer.AddTag(*Tag);
					if (RecurseAll)
					{
						AddChildrenTags(TagContainer, *Tag, true);
					}
				}
			}

		}
	}
}

void UGameplayTagsManager::SplitGameplayTagFName(const FGameplayTag& Tag, TArray<FName>& OutNames)
{
	if (const TSharedPtr<FGameplayTagNode>* GameplayTagNodePtr = GameplayTagNodeMap.Find(Tag))
	{
		TSharedPtr<FGameplayTagNode> CurNode = *GameplayTagNodePtr;
		while (CurNode.IsValid())
		{
			OutNames.Insert(CurNode->GetSimpleTag(), 0);
			CurNode = CurNode->GetParentTagNode().Pin();
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::ComplexGameplayTagsMatch ParentsParents"), STAT_UGameplayTagsManager_GameplayTagsMatchParentsParents, STATGROUP_GameplayTags);

bool UGameplayTagsManager::ComplexGameplayTagsMatch(const FGameplayTag& GameplayTagOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& GameplayTagTwo, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
{
	check(MatchTypeOne != EGameplayTagMatchType::Explicit || MatchTypeTwo != EGameplayTagMatchType::Explicit);

	if (MatchTypeOne == EGameplayTagMatchType::IncludeParentTags && MatchTypeTwo == EGameplayTagMatchType::IncludeParentTags)
	{
		SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GameplayTagsMatchParentsParents);
		const TSharedPtr<FGameplayTagNode>* TagNodeOne = GameplayTagNodeMap.Find(GameplayTagOne);
		const TSharedPtr<FGameplayTagNode>* TagNodeTwo = GameplayTagNodeMap.Find(GameplayTagTwo);
		if (TagNodeOne && TagNodeTwo)
		{
			return (*TagNodeOne)->Parents.DoesTagContainerMatch((*TagNodeTwo)->Parents, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::Any);
		}
	}
	else
	{
		checkSlow(MatchTypeOne != EGameplayTagMatchType::Explicit || MatchTypeTwo != EGameplayTagMatchType::Explicit);

		if (MatchTypeOne == EGameplayTagMatchType::IncludeParentTags)
		{
			const TSharedPtr<FGameplayTagNode>* TagNodeOne = GameplayTagNodeMap.Find(GameplayTagOne);
			if (TagNodeOne)
			{
				return (*TagNodeOne)->Parents.HasTag(GameplayTagTwo, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit);
			}
		}
		else
		{
			const TSharedPtr<FGameplayTagNode>* TagNodeTwo = GameplayTagNodeMap.Find(GameplayTagTwo);
			if (TagNodeTwo)
			{
				return (*TagNodeTwo)->Parents.HasTag(GameplayTagOne, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit);
			}
		}
	}
	return false;
}

#if CHECK_TAG_OPTIMIZATIONS

bool UGameplayTagsManager::GameplayTagsMatchOriginal(const FGameplayTag& GameplayTagOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& GameplayTagTwo, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GameplayTagsMatch);
	if (MatchTypeOne == EGameplayTagMatchType::Explicit && MatchTypeTwo == EGameplayTagMatchType::Explicit)
	{
		const TSharedPtr<FGameplayTagNode>* TagNode1 = GameplayTagNodeMap.Find(GameplayTagOne);
		const TSharedPtr<FGameplayTagNode>* TagNode2 = GameplayTagNodeMap.Find(GameplayTagTwo);
		if (TagNode1 && TagNode2)
		{
			return (*TagNode1)->GetCompleteTag() == (*TagNode2)->GetCompleteTag();
		}
		return false;
	}
	static TSet<FName> Tags1;
	static TSet<FName> Tags2;

	check(!Tags1.Num() && !Tags2.Num()); // must be game thread, cannot call this recursively


	if (MatchTypeOne == EGameplayTagMatchType::IncludeParentTags)
	{
		const TSharedPtr<FGameplayTagNode>* TagNode = GameplayTagNodeMap.Find(GameplayTagOne);
		if (TagNode)
		{
			GetAllParentNodeNames(Tags1, *TagNode);
		}
	}
	if (MatchTypeTwo == EGameplayTagMatchType::IncludeParentTags)
	{
		const TSharedPtr<FGameplayTagNode>* TagNode = GameplayTagNodeMap.Find(GameplayTagTwo);
		if (TagNode)
		{
			GetAllParentNodeNames(Tags2, *TagNode);
		}
	}
	bool bResult = false;

	if (MatchTypeOne == EGameplayTagMatchType::Explicit)
	{
		const TSharedPtr<FGameplayTagNode>* TagNode = GameplayTagNodeMap.Find(GameplayTagOne);
		if (TagNode)
		{
			bResult = Tags2.Contains((*TagNode)->GetCompleteTag());
		}
	}
	else if (MatchTypeTwo == EGameplayTagMatchType::Explicit)
	{
		const TSharedPtr<FGameplayTagNode>* TagNode = GameplayTagNodeMap.Find(GameplayTagTwo);
		if (TagNode)
		{
			bResult = Tags1.Contains((*TagNode)->GetCompleteTag());
		}
	}
	else
	{
		for (auto& Tag1 : Tags1)
		{
			if (Tags2.Contains(Tag1))
			{
				bResult = true;
				break;
			}
		}
	}

	Tags1.Reset();
	Tags2.Reset();
	return bResult;
}

#endif

int32 UGameplayTagsManager::GameplayTagsMatchDepth(const FGameplayTag& GameplayTagOne, const FGameplayTag& GameplayTagTwo) const
{
	TSet<FName> Tags1;
	TSet<FName> Tags2;

	const TSharedPtr<FGameplayTagNode>* TagNode = GameplayTagNodeMap.Find(GameplayTagOne);
	if (TagNode)
	{
		GetAllParentNodeNames(Tags1, *TagNode);
	}

	TagNode = GameplayTagNodeMap.Find(GameplayTagTwo);
	if (TagNode)
	{
		GetAllParentNodeNames(Tags2, *TagNode);
	}

	return Tags1.Intersect(Tags2).Num();
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::GetAllParentNodeNames"), STAT_UGameplayTagsManager_GetAllParentNodeNames, STATGROUP_GameplayTags);

void UGameplayTagsManager::GetAllParentNodeNames(TSet<FName>& NamesList, const TSharedPtr<FGameplayTagNode> GameplayTag) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GetAllParentNodeNames);

	NamesList.Add(GameplayTag->GetCompleteTag());
	const TSharedPtr<FGameplayTagNode> Parent = GameplayTag->GetParentTagNode().Pin();
	if (Parent.IsValid())
	{
		GetAllParentNodeNames(NamesList, Parent);
	}
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::ValidateTagCreation"), STAT_UGameplayTagsManager_ValidateTagCreation, STATGROUP_GameplayTags);

bool UGameplayTagsManager::ValidateTagCreation(FName TagName) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_ValidateTagCreation);

	return FindTagNode(GameplayRootTag, TagName).IsValid();
}

TSharedPtr<FGameplayTagNode> UGameplayTagsManager::FindTagNode(TSharedPtr<FGameplayTagNode> Node, FName TagName) const
{
	TSharedPtr<FGameplayTagNode> FoundTag = NULL;
	const TArray< TSharedPtr<FGameplayTagNode> >& CurrentChildrenTags = Node->GetChildTagNodes();
	for (int32 TagIdx = 0; TagIdx < CurrentChildrenTags.Num(); TagIdx++)
	{
		const TSharedPtr<FGameplayTagNode>& TagNode = CurrentChildrenTags[TagIdx];
		if (TagNode->GetCompleteTag() == TagName || TagNode->GetSimpleTag() == TagName)
		{
			FoundTag = CurrentChildrenTags[TagIdx];
			break;
		}
		FoundTag = FindTagNode(TagNode, TagName);
		if (FoundTag.IsValid())
		{
			break;
		}
	}
	return FoundTag;
}

FGameplayTagTableRow::FGameplayTagTableRow(FGameplayTagTableRow const& Other)
{
	*this = Other;
}

FGameplayTagTableRow& FGameplayTagTableRow::operator=(FGameplayTagTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Tag = Other.Tag;
	CategoryText = Other.CategoryText;
	DevComment = Other.DevComment;

	return *this;
}

bool FGameplayTagTableRow::operator==(FGameplayTagTableRow const& Other) const
{
	return (Tag == Other.Tag);
}

bool FGameplayTagTableRow::operator!=(FGameplayTagTableRow const& Other) const
{
	return (Tag != Other.Tag);
}

FGameplayTagNode::FGameplayTagNode(FName InTag, TWeakPtr<FGameplayTagNode> InParentNode, FText InCategoryDescription)
	: Tag(InTag)
	, CompleteTag(NAME_None)
	, CategoryDescription(InCategoryDescription)
	, ParentNode(InParentNode)
	, NetIndex(INVALID_TAGNETINDEX)
{
	TArray<FName> Tags;

	Tags.Add(InTag);

	TWeakPtr<FGameplayTagNode> CurNode = InParentNode;
	while (CurNode.IsValid())
	{
		Tags.Add(CurNode.Pin()->GetSimpleTag());
		CurNode = CurNode.Pin()->GetParentTagNode();
	}

	FString CompleteTagString;
	for (int32 TagIdx = Tags.Num() - 1; TagIdx >= 0; --TagIdx)
	{
		CompleteTagString += Tags[TagIdx].ToString();
		if (TagIdx > 0)
		{
			CompleteTagString += TEXT(".");
		}
	}

	CompleteTag = FName(*CompleteTagString);
}

void FGameplayTagNode::ResetNode()
{
	Tag = NAME_None;
	CompleteTag = NAME_None;
	NetIndex = INVALID_TAGNETINDEX;

	for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
	{
		ChildTags[ChildIdx]->ResetNode();
	}

	ChildTags.Empty();
	ParentNode.Reset();
}

#undef LOCTEXT_NAMESPACE
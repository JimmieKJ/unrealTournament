// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"
#include "GameplayTagsSettings.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#define LOCTEXT_NAMESPACE "GameplayTagManager"

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
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: ImportINI"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif

			// Update path: Check for old tags in DefaultEngine.ini (we'll push them to the UGameplayTagSettings class).
			TArray<FString> EngineConfigTags;
			GConfig->GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("GameplayTags"), EngineConfigTags, GEngineIni);
			if (EngineConfigTags.Num() > 0)
			{
				UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
				if (MutableDefault->GameplayTags.Num() == 0)
				{
					MutableDefault->GameplayTags.Append(EngineConfigTags);
				}
			}

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
				
				for (FString TagStr : Class->GetDefaultObject<UGameplayTagsSettings>()->GameplayTags)
				{
					FGameplayTagTableRow TableRow;
					TableRow.Tag = TagStr;
					AddTagTableRow(TableRow);
				}
			}
			GameplayRootTag->GetChildTagNodes().Sort(FCompareFGameplayTagNodeByTag());
		}

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

				if (FParse::Value(*It.Value(), TEXT("OldTagName="), OldTagName))
				{
					if (FParse::Value(*It.Value(), TEXT("NewTagName="), NewTagName))
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
		ensureMsgf(false, TEXT("Received invalid tag net index %d! Tag index is out of sync on client!"), Index);

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
	return INVALID_TAGNETINDEX;
}

bool UGameplayTagsManager::ShouldImportTagsFromINI()
{
	bool ImportFromINI = false;
	GConfig->GetBool(TEXT("GameplayTags"), TEXT("ImportTagsFromConfig"), ImportFromINI, GEngineIni);
	return ImportFromINI;
}

bool UGameplayTagsManager::ShouldUseFastReplication()
{
	bool ImportFromINI = false;
	GConfig->GetBool(TEXT("GameplayTags"), TEXT("FastReplication"), ImportFromINI, GEngineIni);
	return ImportFromINI;
}

void UGameplayTagsManager::RedirectTagsForContainer(FGameplayTagContainer& Container, TSet<FName>& DeprecatedTagNamesNotFoundInTagMap)
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

void UGameplayTagsManager::PopulateTreeFromDataTable(class UDataTable* InTable)
{
	checkf(GameplayRootTag.IsValid(), TEXT("ConstructGameplayTagTree() must be called before PopulateTreeFromDataTable()"));
	static const FString ContextString(TEXT("UNKNOWN"));
	
	const int32 NumRows = InTable->RowMap.Num();
	for (int32 RowIdx = 0; RowIdx < NumRows; ++RowIdx)
	{
		FGameplayTagTableRow* TagRow = InTable->FindRow<FGameplayTagTableRow>(*FString::Printf(TEXT("%d"), RowIdx), ContextString);
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

	UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
	if (Settings)
	{

		FString RelativeConfigFilePath = Settings->GetDefaultConfigFilename();
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

		Settings->GameplayTags.Add(NewTag);
		Settings->SortTags();
		IGameplayTagsModule::Get().GetGameplayTagsManager().DestroyGameplayTagTree();

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("ConstructGameplayTagTree GameplayTag tables"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			IGameplayTagsModule::Get().GetGameplayTagsManager().ConstructGameplayTagTree();
		}

		Settings->UpdateDefaultConfigFile();
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
			ensureMsgf(false, TEXT("Requested Tag %s was not found. Check tag data table."), *TagName.ToString());
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
	TArray< TSharedPtr<FGameplayTagNode> > CurrentChildrenTags = Node->GetChildTagNodes();
	for (int32 TagIdx = 0; TagIdx < CurrentChildrenTags.Num(); TagIdx++)
	{
		if (CurrentChildrenTags[TagIdx].Get()->GetCompleteTag() == TagName || CurrentChildrenTags[TagIdx].Get()->GetSimpleTag() == TagName)
		{
			FoundTag = CurrentChildrenTags[TagIdx];
			break;
		}
		FoundTag = FindTagNode(CurrentChildrenTags[TagIdx], TagName);
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

FName FGameplayTagNode::GetCompleteTag() const
{
	return CompleteTag;
}

FName FGameplayTagNode::GetSimpleTag() const
{
	return Tag;
}

FText FGameplayTagNode::GetCategoryDescription() const
{
	return CategoryDescription;
}

TArray< TSharedPtr<FGameplayTagNode> >& FGameplayTagNode::GetChildTagNodes()
{
	return ChildTags;
}

const TArray< TSharedPtr<FGameplayTagNode> >& FGameplayTagNode::GetChildTagNodes() const
{
	return ChildTags;
}

TWeakPtr<FGameplayTagNode> FGameplayTagNode::GetParentTagNode() const
{
	return ParentNode;
}

FGameplayTagNetIndex FGameplayTagNode::GetNetIndex() const
{
	return NetIndex;
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
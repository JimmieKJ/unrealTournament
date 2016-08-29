// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h" // Needed for FTableRowBase
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.generated.h"

/** Simple struct for a table row in the gameplay tag table */
USTRUCT()
struct FGameplayTagTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=GameplayTag)
	FString Tag;

	/** Text that describes this category - not all tags have categories */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=GameplayTag)
	FText CategoryText;

	/** Developer comment clarifying the usage of a particular tag, not user facing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GameplayTag)
	FString DevComment;

	/** Constructors */
	FGameplayTagTableRow() { CategoryText = NSLOCTEXT("TAGCATEGORY", "TAGCATEGORYTESTING", "Category and Category"); }
	FGameplayTagTableRow(FGameplayTagTableRow const& Other);

	/** Assignment/Equality operators */
	FGameplayTagTableRow& operator=(FGameplayTagTableRow const& Other);
	bool operator==(FGameplayTagTableRow const& Other) const;
	bool operator!=(FGameplayTagTableRow const& Other) const;
};

/** Simple tree node for gameplay tags */
USTRUCT()
struct FGameplayTagNode
{
	GENERATED_USTRUCT_BODY()
	FGameplayTagNode(){};

	/** Simple constructor */
	FGameplayTagNode(FName InTag, TWeakPtr<FGameplayTagNode> InParentNode, FText InCategoryDescription = FText());

	/**
	 * Get the complete tag for the node, including all parent tags, delimited by periods
	 * 
	 * @return Complete tag for the node
	 */
	GAMEPLAYTAGS_API FName GetCompleteTag() const { return CompleteTag; }

	/**
	 * Get the simple tag for the node (doesn't include any parent tags)
	 * 
	 * @return Simple tag for the node
	 */
	GAMEPLAYTAGS_API FName GetSimpleTag() const { return Tag; }

	/**
	 * Get the category description for the node
	 * 
	 * @return Translatable text that describes this tag category
	 */
	GAMEPLAYTAGS_API FText GetCategoryDescription() const { return CategoryDescription; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	GAMEPLAYTAGS_API TArray< TSharedPtr<FGameplayTagNode> >& GetChildTagNodes() { return ChildTags; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	GAMEPLAYTAGS_API const TArray< TSharedPtr<FGameplayTagNode> >& GetChildTagNodes() const { return ChildTags; }

	/**
	 * Get the parent tag node of this node
	 * 
	 * @return The parent tag node of this node
	 */
	GAMEPLAYTAGS_API TWeakPtr<FGameplayTagNode> GetParentTagNode() const { return ParentNode; }

	/**
	* Get the net index of this node
	*
	* @return The net index of this node
	*/
	GAMEPLAYTAGS_API FGameplayTagNetIndex GetNetIndex() const { return NetIndex; }

	/** Reset the node of all of its values */
	GAMEPLAYTAGS_API void ResetNode();

private:
	/** Tag for the node */
	FName Tag;

	/** Complete tag for the node, including parent tags */
	FName CompleteTag;

	/** Category description of the for the node */
	FText CategoryDescription;

	/** Child gameplay tag nodes */
	TArray< TSharedPtr<FGameplayTagNode> > ChildTags;

	/** Owner gameplay tag node, if any */
	TWeakPtr<FGameplayTagNode> ParentNode;

	FGameplayTagContainer Parents;
	
	/** Net Index of this node */
	FGameplayTagNetIndex NetIndex;

	friend class UGameplayTagsManager;
};

/** Holds global data loaded at startup, is in a singleton UObject so it works properly with hot reload */
UCLASS(config=Engine)
class GAMEPLAYTAGS_API UGameplayTagsManager : public UObject
{
	GENERATED_UCLASS_BODY()

	// Destructor
	~UGameplayTagsManager();

	/**
	 * Helper function to insert a tag into a tag node array
	 * 
	 * @param Tag					Tag to insert
	 * @param ParentNode			Parent node, if any, for the tag
	 * @param NodeArray				Node array to insert the new node into, if necessary (if the tag already exists, no insertion will occur)
	 * @param CategoryDescription	The description of this category
	 * 
	 * @return Index of the node of the tag
	 */
	int32 InsertTagIntoNodeArray(FName Tag, TWeakPtr<FGameplayTagNode> ParentNode, TArray< TSharedPtr<FGameplayTagNode> >& NodeArray, FText CategoryDescription = FText());

	/**
	 * Get the best tag category description
	 *
	 * @param Tag				Tag that we will find all nodes for
	 * @param OutDescription	The Description that will be returned
	 *
	 * @return The index that we got the description from
	 */
	int32 GetBestTagCategoryDescription(FString Tag, FText& OutDescription) const;

	/** 
	 * Gets all nodes that make up a tag, if any (e.g. weapons.ranged.pistol will return the nodes weapons, weapons.ranged, and weapons.ranged.pistol)
	 * 
	 * @param Tag			The . delimited tag we wish to get nodes for
	 * @param OutTagArray	The array of tag nodes that were found
	 */
	void GetAllNodesForTag( const FString& Tag, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray ) const;

	// this is here because the tag tree doesn't start with a node and recursion can't be done until the first node is found
	void GetAllNodesForTag_Recurse(TArray<FString>& Tags, int32 CurrentTagDepth, TSharedPtr<FGameplayTagNode> CurrentTagNode, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray ) const;

#if WITH_EDITOR
	/** Gets a Filtered copy of the GameplayRootTags Array based on the comma delimited filter string passed in*/
	void GetFilteredGameplayRootTags( const FString& InFilterString, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray );

	/** 
	 * Called via delegate when an object is re-imported in the editor
	 * 
	 * @param ImportFactory	Factory responsible for the re-import
	 * @param InObject		Object that was re-imported
	 */
	void OnObjectReimported(class UFactory* ImportFactory, UObject* InObject);

	TSharedPtr<FGameplayTagNode> FindTagNode(FName TagName) const;

	static void AddNewGameplayTagToINI(FString NewTag);

#endif //WITH_EDITOR

	struct FDeveloperTags 
	{
		struct FDeveloperTagsItem
		{
			FString IniName;
			TArray<FString> Tags;
		};

		TArray<FDeveloperTagsItem> Items;
	};

	FDeveloperTags DeveloperTags;

	/** 
	 * Loads the tag tables
	 * 
	 * @param TagTableNames	The names of the tables to load
	 */
	void LoadGameplayTagTables(TArray<FString>& TagTableNames);

	/** Helper function to construct the gameplay tag tree */
	void ConstructGameplayTagTree();

	/** Helper function to destroy the gameplay tag tree */
	void DestroyGameplayTagTree();

	/**
	 * Gets the FGameplayTag that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * 
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * 
	 * @return Will return the corresponding FGameplayTag or an empty one if not found.
	 */
	UFUNCTION(BlueprintCallable, Category="GameplayTags")
	FGameplayTag RequestGameplayTag(FName TagName, bool ErrorIfNotFound=true) const;

	/**
	 * Adds a tag to the container and removes any direct parents, wont add if child already exists
	 *
	 * @param TagContainer	The tag container we want to add tag too
	 * @param Tag			The tag to try and add to container
	 * 
	 * @return True if tag was added
	 */
	bool AddLeafTagToContainer(FGameplayTagContainer& TagContainer, const FGameplayTag& Tag);

	/**
	 * Gets a Tag Container for the supplied tag and all its parents
	 *
	 * @param GameplayTag The Tag to use at the child most tag for this container
	 * 
	 * @return A Tag Container with the supplied tag and all its parents
	 */
	FGameplayTagContainer RequestGameplayTagParents(const FGameplayTag& GameplayTag) const;

	FGameplayTagContainer RequestGameplayTagChildren(const FGameplayTag& GameplayTag) const;

	FGameplayTag RequestGameplayTagDirectParent(const FGameplayTag& GameplayTag) const;

	void SplitGameplayTagFName(const FGameplayTag& Tag, TArray<FName>& OutNames);

	/**
	 * Checks if the tag is allowed to be created
	 *
	 * @return True if valid
	 */
	bool ValidateTagCreation(FName TagName) const;

	/**
	 * Checks FGameplayTagNode and all FGameplayTagNode children to see if a FGameplayTagNode with the name exists
	 *
	 * @param Node		The Node in the the to start searching from
	 * @param TagName	The name of the tag node to search for
	 * 
	 * @return A shared pointer to the FGameplayTagNode found, or NULL if not found.
	 */
	TSharedPtr<FGameplayTagNode> FindTagNode(TSharedPtr<FGameplayTagNode> Node, FName TagName) const;

	/**
	 * Check to see if two FGameplayTags match
	 *
	 * @param GameplayTagOne	The first tag to compare
	 * @param MatchTypeOne		How we compare Tag one, Explicitly or a match with any parents as well
	 * @param GameplayTagTwo	The second tag to compare
	 * @param MatchTypeTwo		How we compare Tag two, Explicitly or a match with any parents as well
	 * 
	 * @return true if there is a match
	 */
	FORCEINLINE_DEBUGGABLE bool GameplayTagsMatch(const FGameplayTag& GameplayTagOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& GameplayTagTwo, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
	{
		SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GameplayTagsMatch);
		bool bResult;
		if (MatchTypeOne == EGameplayTagMatchType::Explicit && MatchTypeTwo == EGameplayTagMatchType::Explicit)
		{
			bResult = GameplayTagOne == GameplayTagTwo;
		}
		else
		{
			bResult = ComplexGameplayTagsMatch(GameplayTagOne, MatchTypeOne, GameplayTagTwo, MatchTypeTwo);
		}
#if CHECK_TAG_OPTIMIZATIONS
		check(bResult == GameplayTagsMatchOriginal(GameplayTagOne, MatchTypeOne, GameplayTagTwo, MatchTypeTwo));
#endif
		return bResult;
	}

	/**
	 * Same as above except MatchTypeOne == EGameplayTagMatchType::Explicit && MatchTypeTwo == EGameplayTagMatchType::Explicit is not tested or handled
	 */
	bool ComplexGameplayTagsMatch(const FGameplayTag& GameplayTagOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& GameplayTagTwo, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const;
#if CHECK_TAG_OPTIMIZATIONS
	bool GameplayTagsMatchOriginal(const FGameplayTag& GameplayTagOne, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& GameplayTagTwo, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const;
#endif

	/**
	* Check to see how closely two FGameplayTags match. Higher values indicate more matching terms in the tags.
	*
	* @param GameplayTagOne	The first tag to compare
	* @param GameplayTagTwo	The second tag to compare
	*
	* @return the length of the longest matching pair
	*/
	int32 GameplayTagsMatchDepth(const FGameplayTag& GameplayTagOne, const FGameplayTag& GameplayTagTwo) const;

	/**
	 * Helper function for GameplayTagsMatch to get a container with all parents.
	 * @param GameplayTag		tag to get parents of
	 * @return					pointer to container of parents, if any
	 */
	FORCEINLINE const FGameplayTagContainer* GetAllParentsContainer(const FGameplayTag& GameplayTag)
	{
		const TSharedPtr<FGameplayTagNode>* TagNode = GameplayTagNodeMap.Find(GameplayTag);
		if (TagNode)
		{
			return &((*TagNode)->Parents);
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogGameplayTags, Warning, TEXT("::GetAllParentsContainer Failed to find tag %s in GameplayTagNodeMap. Is this tag not in the dictionary?"), *GameplayTag.ToString());
#endif
		return nullptr;
	}


	/** Event for when assets are added to the registry */
	DECLARE_EVENT(UGameplayTagsManager, FGameplayTagTreeChanged);
	virtual FGameplayTagTreeChanged& OnGameplayTagTreeChanged() { return GameplayTagTreeChangedEvent; }

	/** Returns true if we should import tags from UGameplayTagsSettings objects (configured by INI files) */
	static bool ShouldImportTagsFromINI();

	/** TEMP - Returns true if we should warn on invalid (missing) tags */
	static bool ShouldWarnOnInvalidTags();

	/** Should use fast replication */
	bool ShouldUseFastReplication()
	{
		return bUseFastReplication;
	}

	/** Handles redirectors for an entire container, will also error on invalid tags */
	void RedirectTagsForContainer(FGameplayTagContainer& Container, TSet<FName>& DeprecatedTagNamesNotFoundInTagMap, UProperty* SerializingProperty);

	/** Handles redirectors for a single tag, will also error on invalid tag. This is only called for when individual tags are serialized on their own */
	void RedirectSingleGameplayTag(FGameplayTag& Tag, UProperty* SerializingProperty);

	/** Gets a tag name from net index and vice versa, used for replication efficiency */
	FName GetTagNameFromNetIndex(FGameplayTagNetIndex Index);
	FGameplayTagNetIndex GetNetIndexFromTag(const FGameplayTag &InTag);

	/** Cached number of bits we need to replicate tags. That is, Log2(Number of Tags). Will always be <= 16. */
	static int32 NetIndexTrueBitNum;
	
	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicatore "more" (more = second segment that is NetIndexTrueBitNum - NetIndexFirstBitSegment) */
	static int32 NetIndexFirstBitSegment;

	/** Numbers of bits to use for replicating container size. This can be set via config. */
	static int32 NumBitsForContainerSize;

	/** This is the actual value for an invalid tag "None". This is computed at runtime as (Total number of tags) + 1 */
	FGameplayTagNetIndex InvalidTagNetIndex;

	/** Cached runtime value for whether we are using fast replication or not. Initialized from config setting. */
	bool bUseFastReplication;
	
	const TArray<TSharedPtr<FGameplayTagNode>>& GetNetworkGameplayTagNodeIndex() const { return NetworkGameplayTagNodeIndex; }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Mechanism for tracking what tags are frequently replicated */

	static void PrintReplicationFrequencyReport();
	static void NotifyTagReplicated(FGameplayTag Tag, bool WasInContainer);

	static TMap<FGameplayTag, int32>	ReplicationCountMap;
	static TMap<FGameplayTag, int32>	ReplicationCountMap_SingleTags;
	static TMap<FGameplayTag, int32>	ReplicationCountMap_Containers;
#endif

private:

	friend class FGameplayTagTest;
	friend class FGameplayEffectsTest;

	/** Helper function to populate the tag tree from each table */
	void PopulateTreeFromDataTable(class UDataTable* Table);

	void AddTagTableRow(const FGameplayTagTableRow& TagRow);

	/**
	 * Helper function for RequestGameplayTagParents to add all parents to the container
	 *
	 * @param TagContainer The container we need to add the parents too
	 * @param GameplayTag The parent we need to check and add to the container
	 */
	void AddParentTags(FGameplayTagContainer& TagContainer, const FGameplayTag& GameplayTag) const;

	void AddChildrenTags(FGameplayTagContainer& TagContainer, const FGameplayTag& GameplayTag, bool RecurseAll=true) const;

	/**
	 * Helper function for GameplayTagsMatch to get all parents when doing a parent match,
	 * NOTE: Must never be made public as it uses the FNames which should never be exposed
	 * 
	 * @param NameList		The list we are adding all parent complete names too
	 * @param GameplayTag	The current Tag we are adding to the list
	 */
	void GetAllParentNodeNames(TSet<FName>& NamesList, const TSharedPtr<FGameplayTagNode> GameplayTag) const;

	/** Constructs the net indices for each tag */
	void ConstructNetIndex();

	/** Roots of gameplay tag nodes */
	TSharedPtr<FGameplayTagNode> GameplayRootTag;

	class GamePlayTagMapWithInverse
	{
	public:
		void Add(FGameplayTag Tag, TSharedPtr<FGameplayTagNode> GameplayTagNode)
		{
			ForwardMap.Add(Tag, GameplayTagNode);
			InverseMap.Add(GameplayTagNode, Tag);
		}

		const TSharedPtr<FGameplayTagNode>* Find(const FGameplayTag& Tag) const
		{
			return ForwardMap.Find(Tag);
		}

		const FGameplayTag* FindKey(const TSharedPtr<FGameplayTagNode>& TagNode) const
		{
			return InverseMap.Find(TagNode);
		}

		void GenerateValueArray(TArray<TSharedPtr<FGameplayTagNode>>& ValueArray) const
		{
			ForwardMap.GenerateValueArray(ValueArray);
		}

	private:
		TMap<FGameplayTag, TSharedPtr<FGameplayTagNode>> ForwardMap;
		TMap<TSharedPtr<FGameplayTagNode>, FGameplayTag> InverseMap;
	};

	/** Map of Tags to Nodes - Internal use only */
	GamePlayTagMapWithInverse GameplayTagNodeMap;

	/** Map of Names to tags - Internal use only */
	TMap<FName, FGameplayTag> GameplayTagMap;

	/** Our aggregated, sorted list of commonly replicated tags. These tags are given lower indices to ensure they replicate in the first bit segment. */
	TArray<FGameplayTag> CommonlyReplicatedTags;

#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This class is not generically threadsafe.
	mutable FCriticalSection GameplayTagMapCritical;
#endif

	/** Sorted list of nodes, used for network replication */
	TArray<TSharedPtr<FGameplayTagNode>> NetworkGameplayTagNodeIndex;

	/** Holds all of the valid gameplay-related tags that can be applied to assets */
	UPROPERTY()
	TArray<UDataTable*> GameplayTagTables;

	/** The delegate to execute when the tag tree changes */
	FGameplayTagTreeChanged GameplayTagTreeChangedEvent;

	/** The map of ini-configured tag redirectors */
	TMap<FName, FGameplayTag> TagRedirects;

#if WITH_EDITOR
	/** Flag to say if we have registered the ObjectReimport, this is needed as use of Tags manager can happen before Editor is ready*/
	bool RegisteredObjectReimport;
#endif
};

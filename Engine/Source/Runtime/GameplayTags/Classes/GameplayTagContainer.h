// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayTags, Log, All);

DECLARE_STATS_GROUP(TEXT("Gameplay Tags"), STATGROUP_GameplayTags, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("FGameplayTagContainer::HasTag"), STAT_FGameplayTagContainer_HasTag, STATGROUP_GameplayTags, GAMEPLAYTAGS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FGameplayTagContainer::DoesTagContainerMatch"), STAT_FGameplayTagContainer_DoesTagContainerMatch, STATGROUP_GameplayTags, GAMEPLAYTAGS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UGameplayTagsManager::GameplayTagsMatch"), STAT_UGameplayTagsManager_GameplayTagsMatch, STATGROUP_GameplayTags, GAMEPLAYTAGS_API);


#define CHECK_TAG_OPTIMIZATIONS (0)

UENUM(BlueprintType)
namespace EGameplayTagMatchType
{
	enum Type
	{
		Explicit,			// This will check for a match against just this tag
		IncludeParentTags,	// This will also check for matches against all parent tags
	};
}

UENUM(BlueprintType)
enum class EGameplayContainerMatchType : uint8
{
	Any,	//	Means the filter is populated by any tag matches in this container.
	All		//	Means the filter is only populated if all of the tags in this container match.
};

typedef uint16 FGameplayTagNetIndex;
#define INVALID_TAGNETINDEX MAX_uint16

USTRUCT(BlueprintType)
struct GAMEPLAYTAGS_API FGameplayTag
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FGameplayTag()
	{
	}
	/** Operators */
	FORCEINLINE_DEBUGGABLE bool operator==(FGameplayTag const& Other) const
	{
		return TagName == Other.TagName;
	}

	FORCEINLINE_DEBUGGABLE bool operator!=(FGameplayTag const& Other) const
	{
		return TagName != Other.TagName;
	}

	FORCEINLINE_DEBUGGABLE bool operator<(FGameplayTag const& Other) const
	{
		return TagName < Other.TagName;
	}
	
	/**
	 * Check to see if two FGameplayTags match
	 *
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param Other			The second tag to compare against
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	FORCEINLINE_DEBUGGABLE bool Matches(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const
	{
		bool bResult;
		if (MatchTypeOne == EGameplayTagMatchType::Explicit && MatchTypeTwo == EGameplayTagMatchType::Explicit)
		{
			bResult = TagName == Other.TagName;
		}
		else
		{
			bResult = ComplexMatches(MatchTypeOne, Other, MatchTypeTwo);
		}
#if CHECK_TAG_OPTIMIZATIONS
		check(bResult == MatchesOriginal(MatchTypeOne, Other, MatchTypeTwo));
#endif
		return bResult;
	}
	/**
	 * Check to see if two FGameplayTags match
	 *
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param Other			The second tag to compare against
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	bool ComplexMatches(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const;
#if CHECK_TAG_OPTIMIZATIONS
	bool MatchesOriginal(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const;
#endif
	/** Returns whether the tag is valid or not; Invalid tags are set to NAME_None and do not exist in the game-specific global dictionary */
	FORCEINLINE bool IsValid() const
	{
		return (TagName != NAME_None);
	}

	/** Used so we can have a TMap of this struct*/
	friend uint32 GetTypeHash(const FGameplayTag& Tag)
	{
		return ::GetTypeHash(Tag.TagName);
	}

	/** Displays gameplay tag as a string for blueprint graph usage */
	FString ToString() const
	{
		return TagName.ToString();
	}

	/** Get the tag represented as a name */
	FName GetTagName() const
	{
		return TagName;
	}

	friend FArchive& operator<<(FArchive& Ar, FGameplayTag& GameplayTag)
	{
		Ar << GameplayTag.TagName;
		return Ar;
	}

	/** Returns direct parent GameplayTag of this GameplayTag */
	FGameplayTag RequestDirectParent() const;

	/** Overridden for fast serialize */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Handles fixup and errors. This is only called when not serializing a full FGameplayTagContainer */
	void PostSerialize(const FArchive& Ar);
	bool NetSerialize_Packed(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Used to upgrade a Name property to a GameplayTag struct property */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	/** Handles importing tag strings without (TagName=) in it */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

private:

	/** Intentionally private so only the tag manager can use */
	explicit FGameplayTag(FName InTagName);

	/** This Tags Name */
	UPROPERTY(VisibleAnywhere, Category = GameplayTags)
	FName TagName;

	friend class UGameplayTagsManager;
	friend struct FGameplayTagContainer;
};

template<>
struct TStructOpsTypeTraits< FGameplayTag > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true,
		WithPostSerialize = true,
		WithSerializeFromMismatchedTag = true,
		WithImportTextItem = true,
	};
};

/** Simple struct for a gameplay tag container */
USTRUCT(BlueprintType)
struct GAMEPLAYTAGS_API FGameplayTagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FGameplayTagContainer()
	{
	}
	FGameplayTagContainer(FGameplayTagContainer const& Other)
	{
		*this = Other;
	}

	FGameplayTagContainer(const FGameplayTag& Tag)
	{
		AddTag(Tag);
	}

	FGameplayTagContainer(FGameplayTagContainer&& Other)
		: GameplayTags(MoveTemp(Other.GameplayTags))
	{

	}
	~FGameplayTagContainer()
	{
	}

	/** Assignment/Equality operators */
	FGameplayTagContainer& operator=(FGameplayTagContainer const& Other);
	FGameplayTagContainer& operator=(FGameplayTagContainer&& Other);
	bool operator==(FGameplayTagContainer const& Other) const;
	bool operator!=(FGameplayTagContainer const& Other) const;

	template<class AllocatorType>
	static FGameplayTagContainer CreateFromArray(TArray<FGameplayTag, AllocatorType>& SourceTags)
	{
		FGameplayTagContainer Container;
		Container.GameplayTags.Append(SourceTags);
		return Container;
	}

	/**  Returns a new container containing all of the tags of this container, as well as all of their parent tags */
	FGameplayTagContainer GetGameplayTagParents() const;

	/**
	 * Returns a filtered version of this container, as if the container were filtered by matches from the parameter container
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 *
	 * @return A FGameplayTagContainer containing the filtered tags
	 */
	FGameplayTagContainer Filter(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType) const;

	/**
	 * Checks if this container matches ANY of the tags in the specified container. Performs matching by expanding this container out
	 * to include its parent tags.
	 *
	 * @param Other					Container we are checking against
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	 *
	 * @return True if this container has ANY the tags of the passed in container
	 */
	FORCEINLINE_DEBUGGABLE bool MatchesAny(const FGameplayTagContainer& Other, bool bCountEmptyAsMatch) const
	{
		if (Other.Num() == 0)
		{
			return bCountEmptyAsMatch;
		}
		return DoesTagContainerMatch(Other, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::Any);
	}

	/**
	* Checks if this container matches ALL of the tags in the specified container. Performs matching by expanding this container out to
	* include its parent tags.
	*
	* @param Other				Container we are checking against
	* @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	* 
	* @return True if this container has ALL the tags of the passed in container
	*/
	FORCEINLINE_DEBUGGABLE bool MatchesAll(const FGameplayTagContainer& Other, bool bCountEmptyAsMatch) const
	{
		if (Other.Num() == 0)
		{
			return bCountEmptyAsMatch;
		}
		return DoesTagContainerMatch(Other, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit, EGameplayContainerMatchType::All);
	}

	/** 
	 * Checks if this container matches the given query.
	 *
	 * @param Query		Query we are checking against
	 *
	 * @return True if this container matches the query, false otherwise.
	 */
	bool MatchesQuery(const struct FGameplayTagQuery& Query) const;


	/**
	 * Determine if the container has the specified tag. This forces an explicit match. 
	 * This function exists for convenience and brevity. We do not wish to use default values for ::HasTag match type parameters, to avoid confusion on what the default behavior is. (E.g., we want people to think and use the right match type).
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	FORCEINLINE_DEBUGGABLE bool HasTagExplicit(FGameplayTag const& TagToCheck) const
	{
		return HasTag(TagToCheck, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit);
	}

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	FORCEINLINE_DEBUGGABLE bool HasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType=EGameplayTagMatchType::Explicit, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType=EGameplayTagMatchType::Explicit) const
	{
		SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_HasTag);
		bool bResult;
		if (TagMatchType == EGameplayTagMatchType::Explicit && TagToCheckMatchType == EGameplayTagMatchType::Explicit)
		{
			bResult = GameplayTags.Contains(TagToCheck);
		}
		else
		{
			bResult = ComplexHasTag(TagToCheck, TagMatchType, TagToCheckMatchType);
		}
#if CHECK_TAG_OPTIMIZATIONS
		check(bResult == HasTagOriginal(TagToCheck, TagMatchType, TagToCheckMatchType));
#endif
		return bResult;
	}
	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	bool ComplexHasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const;
#if CHECK_TAG_OPTIMIZATIONS
	bool HasTagOriginal(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const;
#endif

	/** 
	 * Adds all the tags from one container to this container 
	 *
	 * @param Other TagContainer that has the tags you want to add to this container 
	 */
	void AppendTags(FGameplayTagContainer const& Other);

	/** 
	 * Adds all the tags that match between the two specified containers to this container 
	 *
	 * @param OtherA TagContainer that has the matching tags you want to add to this container 
	 * @param OtherB TagContainer used to check for matching tags
	 */
	void AppendMatchingTags(FGameplayTagContainer const& OtherA, FGameplayTagContainer const& OtherB);

	/**
	 * Add the specified tag to the container
	 *
	 * @param TagToAdd Tag to add to the container
	 */
	void AddTag(const FGameplayTag& TagToAdd);

	/**
	 * Add the specified tag to the container without checking for uniqueness
	 *
	 * @param TagToAdd Tag to add to the container
	 * 
	 * Useful when building container from another data struct (TMap for example)
	 */
	void AddTagFast(const FGameplayTag& TagToAdd);

	/**
	 * Tag to remove from the container
	 * 
	 * @param TagToRemove	Tag to remove from the container
	 */
	bool RemoveTag(FGameplayTag TagToRemove);

	/**
	* Removes all tags in TagsToRemove from this container
	*
	* @param TagsToRemove	Tags to remove from the container
	*/
	void RemoveTags(FGameplayTagContainer TagsToRemove);

	/** Remove all tags from the container */
	void RemoveAllTags(int32 Slack=0);

	void RemoveAllTagsKeepSlack()
	{
		RemoveAllTags(GameplayTags.Num());
	}

	void Reset()
	{
		GameplayTags.Reset();
	}

	/**
	 * Serialize the tag container
	 *
	 * @param Ar	Archive to serialize to
	 *
	 * @return True if the container was serialized
	 */
	bool Serialize(FArchive& Ar);

	/**
	 * Returns the Tag Count
	 *
	 * @return The number of tags
	 */
	int32 Num() const;

	/** Returns human readable Tag list */
	FString ToString() const;

	/** Returns abbreviated human readable Tag list without parens or property names */
	FString ToStringSimple() const;

	// Returns human readable description of what match is being looked for on the readable tag list.
	FText ToMatchingText(EGameplayContainerMatchType MatchType, bool bInvertCondition) const;

	/** Creates a const iterator for the contents of this array */
	TArray<FGameplayTag>::TConstIterator CreateConstIterator() const
	{
		return GameplayTags.CreateConstIterator();
	}

	bool IsValidIndex(int32 Index)
	{
		return GameplayTags.IsValidIndex(Index);
	}

	FGameplayTag GetByIndex(int32 Index)
	{
		if (IsValidIndex(Index))
		{
			return GameplayTags[Index];
		}
		return FGameplayTag();
	}	

	FGameplayTag First() const
	{
		return GameplayTags.Num() > 0 ? GameplayTags[0] : FGameplayTag();
	}

	FGameplayTag Last() const
	{
		return GameplayTags.Num() > 0 ? GameplayTags.Last() : FGameplayTag();
	}

	/** An empty Gameplay Tag Container */
	static const FGameplayTagContainer EmptyContainer;
		
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/**
	* Returns true if the tags in this container match the tags in OtherContainer for the specified matching types.
	*
	* @param OtherContainer		The Container to filter against
	* @param TagMatchType			Type of match to use for the tags in this container
	* @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	* @param ContainerMatchType	Type of match to use for filtering
	*
	* @return Returns true if ContainerMatchType is Any and any of the tags in OtherContainer match the tags in this or ContainerMatchType is All and all of the tags in OtherContainer match at least one tag in this. Returns false otherwise.
	*/
	FORCEINLINE_DEBUGGABLE bool DoesTagContainerMatch(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, EGameplayContainerMatchType ContainerMatchType) const
	{
		SCOPE_CYCLE_COUNTER(STAT_FGameplayTagContainer_DoesTagContainerMatch);
		bool bResult;
		if (ContainerMatchType == EGameplayContainerMatchType::Any && TagMatchType == EGameplayTagMatchType::Explicit && OtherTagMatchType == EGameplayTagMatchType::Explicit)
		{
			bResult = false;
			for (TArray<FGameplayTag>::TConstIterator OtherIt(OtherContainer.GameplayTags); OtherIt; ++OtherIt)
			{
				if (GameplayTags.Contains(*OtherIt))
				{
					bResult = true;
					break;
				}
			}			
		}
		else
		{
			bResult = DoesTagContainerMatchComplex(OtherContainer, TagMatchType, OtherTagMatchType, ContainerMatchType);
		}
#if CHECK_TAG_OPTIMIZATIONS
		// the complex version is the original
		check(bResult == DoesTagContainerMatchComplex(OtherContainer, TagMatchType, OtherTagMatchType, ContainerMatchType));
#endif
		return bResult;
	}
protected:

	/**
	* Returns true if the tags in this container match the tags in OtherContainer for the specified matching types.
	*
	* @param OtherContainer		The Container to filter against
	* @param TagMatchType			Type of match to use for the tags in this container
	* @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	* @param ContainerMatchType	Type of match to use for filtering
	*
	* @return Returns true if ContainerMatchType is Any and any of the tags in OtherContainer match the tags in this or ContainerMatchType is All and all of the tags in OtherContainer match at least one tag in this. Returns false otherwise.
	*/
	bool DoesTagContainerMatchComplex(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, EGameplayContainerMatchType ContainerMatchType) const;

	/** Array of gameplay tags */
	UPROPERTY(BlueprintReadWrite, Category=GameplayTags)
	TArray<FGameplayTag> GameplayTags;

	/**
	 * If a Tag with the specified tag name explicitly exists, it will remove that tag and return true.  Otherwise, it 
	   returns false.  It does NOT check the TagName for validity (i.e. the tag could be obsolete and so not exist in
	   the table). It also does NOT check parents (because it cannot do so for a tag that isn't in the table).
	   NOTE: This function should ONLY ever be used by GameplayTagsManager when redirecting tags.  Do NOT make this
	   function public!
	 */
	bool RemoveTagByExplicitName(const FName& TagName);

	// Allow the redirection helper class access to RemoveTagByExplicitName.  It can then (through friendship) allow
	// access to others without exposing everything the Container has privately to everyone.
	friend class FGameplayTagRedirectHelper;
	friend struct FGameplayTagQuery;
	friend struct FGameplayTagQueryExpression;

private:

	/** Array of gameplay tags */
	UPROPERTY()
	TArray<FName> Tags_DEPRECATED;

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	
	FORCEINLINE friend TArray<FGameplayTag>::TConstIterator begin(const FGameplayTagContainer& Array) { return Array.CreateConstIterator(); }
	FORCEINLINE friend TArray<FGameplayTag>::TConstIterator end(const FGameplayTagContainer& Array) { return TArray<FGameplayTag>::TConstIterator(Array.GameplayTags, Array.GameplayTags.Num()); }
};

// This helper class exists to keep FGameplayTagContainers protected and private fields appropriately private while
// exposing only necessary features to the GameplayTagsManager.
class FGameplayTagRedirectHelper
{
private:
	FORCEINLINE static bool RemoveTagByExplicitName(FGameplayTagContainer& Container, const FName& TagName) { return Container.RemoveTagByExplicitName(TagName); }

	friend class UGameplayTagsManager;
};

template<>
struct TStructOpsTypeTraits<FGameplayTagContainer> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true,
		WithNetSerializer = true,
		WithCopy = true
	};
};


/** Enumerates the list of supported query expression types. */
UENUM()
namespace EGameplayTagQueryExprType
{
	enum Type
	{
		Undefined = 0,
		AnyTagsMatch,
		AllTagsMatch,
		NoTagsMatch,
		AnyExprMatch,
		AllExprMatch,
		NoExprMatch,
	};
}

namespace EGameplayTagQueryStreamVersion
{
	enum Type
	{
		InitialVersion = 0,

		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

/**
 * An FGameplayTagQuery is a logical query that can be run against an FGameplayTagContainer.  A query that succeeds is said to "match".
 * Queries are logical expressions that can test the intersection properties of another tag container (all, any, or none), or the matching state of a set of sub-expressions
 * (all, any, or none). This allows queries to be arbitrarily recursive and very expressive.  For instance, if you wanted to test if a given tag container contained tags 
 * ((A && B) || (C)) && (!D), you would construct your query in the form ALL( ANY( ALL(A,B), ALL(C) ), NONE(D) )
 * 
 * You can expose the query structs to Blueprints and edit them with a custom editor, or you can construct them natively in code. 
 * 
 * Example of how to build a query via code:
 *	FGameplayTagQuery Q;
 *	Q.BuildQuery(
 *		FGameplayTagQueryExpression()
 * 		.AllTagsMatch()
 *		.AddTag(IGameplayTagsModule::RequestGameplayTag(FName(TEXT("Animal.Mammal.Dog.Corgi"))))
 *		.AddTag(IGameplayTagsModule::RequestGameplayTag(FName(TEXT("Plant.Tree.Spruce"))))
 *		);
 * 
 * Queries are internally represented as a byte stream that is memory-efficient and can be evaluated quickly at runtime.
 * Note: these have an extensive details and graph pin customization for editing, so there is no need to expose the internals to Blueprints.
 */
USTRUCT(BlueprintType, meta=(HasNativeMake="GameplayTags.UBlueprintGameplayTagLibrary.MakeGameplayTagQuery"))
struct GAMEPLAYTAGS_API FGameplayTagQuery
{
	GENERATED_BODY();

public:
	FGameplayTagQuery();

	FGameplayTagQuery(FGameplayTagQuery const& Other);
	FGameplayTagQuery(FGameplayTagQuery&& Other);

	/** Assignment/Equality operators */
	FGameplayTagQuery& operator=(FGameplayTagQuery const& Other);
	FGameplayTagQuery& operator=(FGameplayTagQuery&& Other);

private:
	/** Versioning for future token stream protocol changes. See EGameplayTagQueryStreamVersion. */
	UPROPERTY()
	int32 TokenStreamVersion;

	/** List of tags referenced by this entire query. Token stream stored indices into this list. */
	UPROPERTY()
	TArray<FGameplayTag> TagDictionary;

	/** Stream representation of the actual hierarchical query */
	UPROPERTY()
	TArray<uint8> QueryTokenStream;

	/** User-provided string describing the query */
	UPROPERTY()
	FString UserDescription;

	/** Auto-generated string describing the query */
	UPROPERTY()
	FString AutoDescription;

	/** Returns a gameplay tag from the tag dictionary */
	FGameplayTag GetTagFromIndex(int32 TagIdx) const
	{
		ensure(TagDictionary.IsValidIndex(TagIdx));
		return TagDictionary[TagIdx];
	}

public:

	/** Replaces existing tags with passed in tags. Does not modify the tag query expression logic. Useful when you need to cache off and update often used query. Must use same sized tag container! */
	void ReplaceTagsFast(FGameplayTagContainer const& Tags)
	{
		ensure(Tags.Num() == TagDictionary.Num());
		TagDictionary.Reset();
		TagDictionary.Append(Tags.GameplayTags);
	}

	/** Replaces existing tags with passed in tag. Does not modify the tag query expression logic. Useful when you need to cache off and update often used query. */		 
	void ReplaceTagFast(FGameplayTag const& Tag)
	{
		ensure(1 == TagDictionary.Num());
		TagDictionary.Reset();
		TagDictionary.Add(Tag);
	}

	/** Returns true if the given tags match this query, or false otherwise. */
	bool Matches(FGameplayTagContainer const& Tags) const;

	/** Returns true if this query is empty, false otherwise. */
	bool IsEmpty() const;

	/** Resets this query to its default empty state. */
	void Clear();

	/** Creates this query with the given root expression. */
	void Build(struct FGameplayTagQueryExpression& RootQueryExpr, FString InUserDescription = FString());

	/** Static function to assemble and return a query. */
	static FGameplayTagQuery BuildQuery(struct FGameplayTagQueryExpression& RootQueryExpr, FString InDescription = FString());

	/** Builds a FGameplayTagQueryExpression from this query. */
	void GetQueryExpr(struct FGameplayTagQueryExpression& OutExpr) const;

	/** Returns description string. */
	FString GetDescription() const { return UserDescription.IsEmpty() ? AutoDescription : UserDescription; };

#if WITH_EDITOR
	/** Creates this query based on the given EditableQuery object */
	void BuildFromEditableQuery(class UEditableGameplayTagQuery& EditableQuery); 

	/** Creates editable query object tree based on this query */
	UEditableGameplayTagQuery* CreateEditableQuery();
#endif // WITH_EDITOR

	static const FGameplayTagQuery EmptyQuery;

	/**
	* Shortcuts for easily creating common query types
	* @todo: add more as dictated by use cases
	*/

	/** Creates a tag query that will match if there are any common tags between the given tags and the tags being queries against. */
	static FGameplayTagQuery MakeQuery_MatchAnyTags(FGameplayTagContainer const& InTags);
	static FGameplayTagQuery MakeQuery_MatchAllTags(FGameplayTagContainer const& InTags);
	static FGameplayTagQuery MakeQuery_MatchNoTags(FGameplayTagContainer const& InTags);

	friend class FQueryEvaluator;
};

struct GAMEPLAYTAGS_API FGameplayTagQueryExpression
{
	/** 
	 * Fluid syntax approach for setting the type of this expression. 
	 */

	FGameplayTagQueryExpression& AnyTagsMatch()
	{
		ExprType = EGameplayTagQueryExprType::AnyTagsMatch;
		return *this;
	}

	FGameplayTagQueryExpression& AllTagsMatch()
	{
		ExprType = EGameplayTagQueryExprType::AllTagsMatch;
		return *this;
	}

	FGameplayTagQueryExpression& NoTagsMatch()
	{
		ExprType = EGameplayTagQueryExprType::NoTagsMatch;
		return *this;
	}

	FGameplayTagQueryExpression& AnyExprMatch()
	{
		ExprType = EGameplayTagQueryExprType::AnyExprMatch;
		return *this;
	}

	FGameplayTagQueryExpression& AllExprMatch()
	{
		ExprType = EGameplayTagQueryExprType::AllExprMatch;
		return *this;
	}

	FGameplayTagQueryExpression& NoExprMatch()
	{
		ExprType = EGameplayTagQueryExprType::NoExprMatch;
		return *this;
	}

	FGameplayTagQueryExpression& AddTag(TCHAR const* TagString)
	{
		return AddTag(FName(TagString));
	}
	FGameplayTagQueryExpression& AddTag(FName TagName);
	FGameplayTagQueryExpression& AddTag(FGameplayTag Tag)
	{
		ensure(UsesTagSet());
		TagSet.Add(Tag);
		return *this;
	}
	
	FGameplayTagQueryExpression& AddTags(FGameplayTagContainer const& Tags)
	{
		ensure(UsesTagSet());
		TagSet.Append(Tags.GameplayTags);
		return *this;
	}

	FGameplayTagQueryExpression& AddExpr(FGameplayTagQueryExpression& Expr)
	{
		ensure(UsesExprSet());
		ExprSet.Add(Expr);
		return *this;
	}
	
	/** Writes this expression to the given token stream. */
	void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary) const;

	/** Which type of expression this is. */
	EGameplayTagQueryExprType::Type ExprType;
	/** Expression list, for expression types that need it */
	TArray<struct FGameplayTagQueryExpression> ExprSet;
	/** Tag list, for expression types that need it */
	TArray<FGameplayTag> TagSet;

	/** Returns true if this expression uses the tag data. */
	FORCEINLINE bool UsesTagSet() const
	{
		return (ExprType == EGameplayTagQueryExprType::AllTagsMatch) || (ExprType == EGameplayTagQueryExprType::AnyTagsMatch) || (ExprType == EGameplayTagQueryExprType::NoTagsMatch);
	}
	/** Returns true if this expression uses the expression list data. */
	FORCEINLINE bool UsesExprSet() const
	{
		return (ExprType == EGameplayTagQueryExprType::AllExprMatch) || (ExprType == EGameplayTagQueryExprType::AnyExprMatch) || (ExprType == EGameplayTagQueryExprType::NoExprMatch);
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayTagQuery> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true
	};
};



/** 
 * This is an editor-only representation of a query, designed to be editable with a typical property window. 
 * To edit a query in the editor, an FGameplayTagQuery is converted to a set of UObjects and edited,  When finished,
 * the query struct is rewritten and these UObjects are discarded.
 * This query representation is not intended for runtime use.
 */
UCLASS(editinlinenew, collapseCategories, Transient) 
class GAMEPLAYTAGS_API UEditableGameplayTagQuery : public UObject
{
	GENERATED_BODY()

public:
	/** User-supplied description, shown in property details. Auto-generated description is shown if not supplied. */
	UPROPERTY(EditDefaultsOnly, Category = Query)
	FString UserDescription;

	/** Automatically-generated description */
	FString AutoDescription;

	/** The base expression of this query. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Query)
	class UEditableGameplayTagQueryExpression* RootExpression;

#if WITH_EDITOR
	/** Converts this editor query construct into the runtime-usable token stream. */
	void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString=nullptr) const;

	/** Generates and returns the export text for this query. */
	FString GetTagQueryExportText(FGameplayTagQuery const& TagQuery);
#endif  // WITH_EDITOR

private:
	/** Property to hold a gameplay tag query so we can use the exporttext path to get a string representation. */
	UPROPERTY()
	FGameplayTagQuery TagQueryExportText_Helper;
};

UCLASS(abstract, editinlinenew, collapseCategories, Transient)
class GAMEPLAYTAGS_API UEditableGameplayTagQueryExpression : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	/** Converts this editor query construct into the runtime-usable token stream. */
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString=nullptr) const {};

protected:
	void EmitTagTokens(FGameplayTagContainer const& TagsToEmit, TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const;
	void EmitExprListTokens(TArray<UEditableGameplayTagQueryExpression*> const& ExprList, TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString) const;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta=(DisplayName="Any Tags Match"))
class UEditableGameplayTagQueryExpression_AnyTagsMatch : public UEditableGameplayTagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FGameplayTagContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "All Tags Match"))
class UEditableGameplayTagQueryExpression_AllTagsMatch : public UEditableGameplayTagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FGameplayTagContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "No Tags Match"))
class UEditableGameplayTagQueryExpression_NoTagsMatch : public UEditableGameplayTagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FGameplayTagContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "Any Expressions Match"))
class UEditableGameplayTagQueryExpression_AnyExprMatch : public UEditableGameplayTagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableGameplayTagQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "All Expressions Match"))
class UEditableGameplayTagQueryExpression_AllExprMatch : public UEditableGameplayTagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableGameplayTagQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "No Expressions Match"))
class UEditableGameplayTagQueryExpression_NoExprMatch : public UEditableGameplayTagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableGameplayTagQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FGameplayTag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};


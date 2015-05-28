// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.generated.h"

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
	FGameplayTag();

	/** Operators */
	bool operator==(FGameplayTag const& Other) const
	{
		return TagName == Other.TagName;
	}

	bool operator!=(FGameplayTag const& Other) const
	{
		return TagName != Other.TagName;
	}

	bool operator<(FGameplayTag const& Other) const
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
	bool Matches(TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeOne, const FGameplayTag& Other, TEnumAsByte<EGameplayTagMatchType::Type> MatchTypeTwo) const;
	
	/** Returns whether the tag is valid or not; Invalid tags are set to NAME_None and do not exist in the game-specific global dictionary */
	bool IsValid() const;

	/** Used so we can have a TMap of this struct*/
	friend uint32 GetTypeHash(const FGameplayTag& Tag)
	{
		return ::GetTypeHash(Tag.TagName);
	}

	/** Displays container as a string for blueprint graph usage */
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

private:

	/** Intentionally private so only the tag manager can use */
	explicit FGameplayTag(FName InTagName);

	/** This Tags Name */
	UPROPERTY(VisibleAnywhere, Category = GameplayTags)
	FName TagName;

	friend class UGameplayTagsManager;
};

template<>
struct TStructOpsTypeTraits< FGameplayTag > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true,
	};
};

/** Simple struct for a gameplay tag container */
USTRUCT(BlueprintType)
struct GAMEPLAYTAGS_API FGameplayTagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FGameplayTagContainer();
	FGameplayTagContainer(FGameplayTagContainer const& Other);
	FGameplayTagContainer(const FGameplayTag& Tag);
	FGameplayTagContainer(FGameplayTagContainer&& Other);
	virtual ~FGameplayTagContainer() {}

	/** Assignment/Equality operators */
	FGameplayTagContainer& operator=(FGameplayTagContainer const& Other);
	FGameplayTagContainer & operator=(FGameplayTagContainer&& Other);
	bool operator==(FGameplayTagContainer const& Other) const;
	bool operator!=(FGameplayTagContainer const& Other) const;

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
	bool MatchesAny(const FGameplayTagContainer& Other, bool bCountEmptyAsMatch) const;

	/**
	* Checks if this container matches ALL of the tags in the specified container. Performs matching by expanding this container out to
	* include its parent tags.
	*
	* @param Other				Container we are checking against
	* @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	* 
	* @return True if this container has ALL the tags of the passed in container
	*/
	bool MatchesAll(const FGameplayTagContainer& Other, bool bCountEmptyAsMatch) const;

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	virtual bool HasTag(FGameplayTag const& TagToCheck, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> TagToCheckMatchType) const;

	/** 
	 * Adds all the tags from one container to this container 
	 *
	 * @param Other TagContainer that has the tags you want to add to this container 
	 */
	virtual void AppendTags(FGameplayTagContainer const& Other);

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
	 * @param RemoveChildlessParents Also Remove any parents that would be left with no children
	 */
	virtual void RemoveTag(FGameplayTag TagToRemove);

	/** Remove all tags from the container */
	virtual void RemoveAllTags(int32 Slack=0);

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
	bool DoesTagContainerMatch(const FGameplayTagContainer& OtherContainer, TEnumAsByte<EGameplayTagMatchType::Type> TagMatchType, TEnumAsByte<EGameplayTagMatchType::Type> OtherTagMatchType, EGameplayContainerMatchType ContainerMatchType) const;

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
		WithCopy = true
	};
};

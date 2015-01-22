// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h" // for ENodeTitleType

/*******************************************************************************
 * FNodeTextCache
 ******************************************************************************/

/** 
 * Constructing FText strings every frame can be costly, so this struct provides 
 * a way to easily cache those strings (node title, tooptip, etc.) for reuse.
 */
struct FNodeTextCache
{
public:
	/** */
	FORCEINLINE bool IsOutOfDate() const
	{
		return CachedText.IsEmpty();
	}

	/** */
	FORCEINLINE void SetCachedText(FText const& Text) const
	{
		CachedText = Text;
	}

	/** */
	FORCEINLINE FText& GetCachedText() const
	{
		return CachedText;
	}

	/** */
	FORCEINLINE void MarkDirty() const
	{
		Clear();
	}

	/** */
	FORCEINLINE void Clear() const
	{
		CachedText = FText::GetEmpty();
	}

	/** */
	FORCEINLINE FText& operator=(FText const& Text) const
	{
		SetCachedText(Text);
		return CachedText;
	}

	/** */
	FORCEINLINE operator FText&() const
	{
		return GetCachedText();
	}

private:
	/** Mutable so that SetCachedText() can remain const (callable by the node's GetNodeTitle() method)*/
	mutable FText CachedText;
};

/*******************************************************************************
 * FNodeTitleTextTable
 ******************************************************************************/

struct FNodeTitleTextTable
{
public:
	/** */
	FORCEINLINE bool IsTitleCached(ENodeTitleType::Type TitleType) const
	{
		return !CachedNodeTitles[TitleType].IsOutOfDate();
	}

	/** */
	FORCEINLINE void SetCachedTitle(ENodeTitleType::Type TitleType, FText const& Text) const
	{
		CachedNodeTitles[TitleType].SetCachedText(Text);
	}

	/** */
	FORCEINLINE FText& GetCachedTitle(ENodeTitleType::Type TitleType) const
	{
		return CachedNodeTitles[TitleType].GetCachedText();
	}

	/** */
	FORCEINLINE void MarkDirty() const
	{
		for (uint32 TitleIndex = 0; TitleIndex < ENodeTitleType::MAX_TitleTypes; ++TitleIndex)
		{
			CachedNodeTitles[TitleIndex].MarkDirty();
		}
	}

	/** */
	FORCEINLINE FText& operator[](ENodeTitleType::Type TitleIndex) const
	{
		return GetCachedTitle(TitleIndex);
	}

private:
	/** */
	FNodeTextCache CachedNodeTitles[ENodeTitleType::MAX_TitleTypes];
};

/*******************************************************************************
* FNodeTextTable
******************************************************************************/

struct FNodeTextTable : FNodeTitleTextTable
{
public:
	/** */
	FORCEINLINE bool IsTooltipCached() const
	{
		return !CachedTooltip.IsOutOfDate();
	}

	/** */
	FORCEINLINE void SetCachedTooltip(FText const& Text) const
	{
		CachedTooltip.SetCachedText(Text);
	}

	/** */
	FORCEINLINE FText GetCachedTooltip() const
	{
		return CachedTooltip.GetCachedText();
	}

	/** */
	FORCEINLINE void MarkDirty() const
	{
		FNodeTitleTextTable::MarkDirty();
		CachedTooltip.MarkDirty();
	}

private:
	/** */
	FNodeTextCache CachedTooltip;
};

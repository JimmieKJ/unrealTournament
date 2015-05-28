// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "AssetViewTypes.h"

struct FCompareFAssetItemBase
{
public:
	/** Default constructor */
	FCompareFAssetItemBase(bool bInAscending, const FName& InTag) : bAscending(bInAscending), Tag(InTag) {}

	virtual ~FCompareFAssetItemBase() {}

	/** Sort function */
	FORCEINLINE bool operator()(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const
	{
		bool bFolderComparisonResult = false;
		if (CompareFolderFirst(A, B, bFolderComparisonResult))
		{
			return bFolderComparisonResult;
		}
		return Compare(A, B);
	}

	/** Get the next comparisons array */
	FORCEINLINE TArray<TUniquePtr<FCompareFAssetItemBase>>& GetNextComparisons() const
	{
		return NextComparisons;
	}

	/** Set the next comparisons array */
	FORCEINLINE void SetNextComparisons(TArray<TUniquePtr<FCompareFAssetItemBase>>& InNextComparisons) const
	{
		check(NextComparisons.Num() == 0);
		NextComparisons = MoveTemp(InNextComparisons);
	}

protected:
	/** Comparison function, needs to be implemented by inheriting structs - by default just forwards to the next comparitor */
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const { return CompareNext(A, B); }

private:
	/** Compare the folders of both assets */
	FORCEINLINE bool CompareFolderFirst(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B, bool& bComparisonResult) const
	{
		if (A->GetType() == EAssetItemType::Folder)
		{
			if (B->GetType() == EAssetItemType::Folder)
			{
				const FText& ValueA = StaticCastSharedPtr<FAssetViewFolder>(A)->FolderName;
				const FText& ValueB = StaticCastSharedPtr<FAssetViewFolder>(B)->FolderName;
				const int32 Result = ValueA.CompareTo(ValueB);
				bComparisonResult = bAscending ? Result < 0 : Result > 0;
			}
			else
			{
				bComparisonResult = bAscending;
			}
			return true;
		}
		else if (B->GetType() == EAssetItemType::Folder)
		{
			bComparisonResult = !bAscending;
			return true;
		}
		return false;
	}

	/** Comparison function in the event of a tie*/
	FORCEINLINE bool CompareNext(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const
	{
		if (NextComparisons.Num() > 0)
		{
			TUniquePtr<FCompareFAssetItemBase> CompareNext = MoveTemp(NextComparisons[0]);
			check(CompareNext);
			NextComparisons.RemoveAt(0);

			// Move all the remaining comparisons to the next comparitor
			CompareNext->SetNextComparisons(GetNextComparisons());
			const bool Result = CompareNext->Compare(A, B);

			// Move the comparisons back for further ties
			SetNextComparisons(CompareNext->GetNextComparisons());
			NextComparisons.Insert(MoveTemp(CompareNext), 0);

			return Result;
		}
		return true;	// default to true if a tie
	}

protected:
	/** Whether to sort ascending or descending */
	bool bAscending;

	/** The tag type we need to compare next */
	FName Tag;

private:
	/** The follow up sorting methods in the event of a tie */
	mutable TArray<TUniquePtr<FCompareFAssetItemBase>> NextComparisons;
};

struct FCompareFAssetItemByName : public FCompareFAssetItemBase
{
public:
	FCompareFAssetItemByName(bool bInAscending, const FName& InTag) : FCompareFAssetItemBase(bInAscending, InTag) {}
	virtual ~FCompareFAssetItemByName() {}

protected:
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const override
	{
		const FName& ValueA = StaticCastSharedPtr<FAssetViewAsset>(A)->Data.AssetName;
		const FName& ValueB = StaticCastSharedPtr<FAssetViewAsset>(B)->Data.AssetName;
		const int32 Result = ValueA.Compare(ValueB);
		if (Result < 0)
		{
			return bAscending;
		}
		else if (Result > 0)
		{
			return !bAscending;
		}
		return FCompareFAssetItemBase::Compare(A, B);
	}
};

struct FCompareFAssetItemByClass : public FCompareFAssetItemBase
{
public:
	FCompareFAssetItemByClass(bool bInAscending, const FName& InTag) : FCompareFAssetItemBase(bInAscending, InTag) {}
	virtual ~FCompareFAssetItemByClass() {}

protected:
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const override
	{
		const FName& ValueA = StaticCastSharedPtr<FAssetViewAsset>(A)->Data.AssetClass;
		const FName& ValueB = StaticCastSharedPtr<FAssetViewAsset>(B)->Data.AssetClass;
		const int32 Result = ValueA.Compare(ValueB);
		if (Result < 0)
		{
			return bAscending;
		}
		else if (Result > 0)
		{
			return !bAscending;
		}
		return FCompareFAssetItemBase::Compare(A, B);
	}
};

struct FCompareFAssetItemByPath : public FCompareFAssetItemBase
{
public:
	FCompareFAssetItemByPath(bool bInAscending, const FName& InTag) : FCompareFAssetItemBase(bInAscending, InTag) {}
	virtual ~FCompareFAssetItemByPath() {}

protected:
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const override
	{
		const FName& ValueA = StaticCastSharedPtr<FAssetViewAsset>(A)->Data.PackagePath;
		const FName& ValueB = StaticCastSharedPtr<FAssetViewAsset>(B)->Data.PackagePath;
		const int32 Result = ValueA.Compare(ValueB);
		if (Result < 0)
		{
			return bAscending;
		}
		else if (Result > 0)
		{
			return !bAscending;
		}
		return FCompareFAssetItemBase::Compare(A, B);
	}
};

struct FCompareFAssetItemByTag : public FCompareFAssetItemBase
{
public:
	FCompareFAssetItemByTag(bool bInAscending, const FName& InTag) : FCompareFAssetItemBase(bInAscending, InTag) {}
	virtual ~FCompareFAssetItemByTag() {}

protected:
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const override
	{
		// Depending if we're sorting ascending or descending it's quicker to flip the compares incase tags are missing
		const FString* Value1 = bAscending ? StaticCastSharedPtr<FAssetViewAsset>(A)->Data.TagsAndValues.Find(Tag) : StaticCastSharedPtr<FAssetViewAsset>(B)->Data.TagsAndValues.Find(Tag);
		if (!Value1)
		{
			return true;
		}

		const FString* Value2 = bAscending ? StaticCastSharedPtr<FAssetViewAsset>(B)->Data.TagsAndValues.Find(Tag) : StaticCastSharedPtr<FAssetViewAsset>(A)->Data.TagsAndValues.Find(Tag);
		if (!Value2)
		{
			return false;
		}

		const int32 Result = Value1->Compare(*Value2, ESearchCase::IgnoreCase);
		if (Result < 0)
		{
			return true;
		}
		else if (Result > 0)
		{
			return false;
		}
		return FCompareFAssetItemBase::Compare(A, B);
	}
};

struct FCompareFAssetItemByTagNumerical : public FCompareFAssetItemBase
{
public:
	FCompareFAssetItemByTagNumerical(bool bInAscending, const FName& InTag) : FCompareFAssetItemBase(bInAscending, InTag) {}
	virtual ~FCompareFAssetItemByTagNumerical() {}

protected:
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const override
	{
		// Depending if we're sorting ascending or descending it's quicker to flip the compares incase tags are missing
		const FString* Value1 = bAscending ? StaticCastSharedPtr<FAssetViewAsset>(A)->Data.TagsAndValues.Find(Tag) : StaticCastSharedPtr<FAssetViewAsset>(B)->Data.TagsAndValues.Find(Tag);
		if (!Value1)
		{
			return true;
		}

		const FString* Value2 = bAscending ? StaticCastSharedPtr<FAssetViewAsset>(B)->Data.TagsAndValues.Find(Tag) : StaticCastSharedPtr<FAssetViewAsset>(A)->Data.TagsAndValues.Find(Tag);
		if (!Value2)
		{
			return false;
		}

		const float Num1 = FCString::Atof(**Value1);
		const float Num2 = FCString::Atof(**Value2);
		if (Num1 < Num2)
		{
			return true;
		}
		else if (Num1 > Num2)
		{
			return false;
		}
		return FCompareFAssetItemBase::Compare(A, B);
	}
};

struct FCompareFAssetItemByTagDimensional : public FCompareFAssetItemBase
{
public:
	FCompareFAssetItemByTagDimensional(bool bInAscending, const FName& InTag) : FCompareFAssetItemBase(bInAscending, InTag) {}
	virtual ~FCompareFAssetItemByTagDimensional() {}

protected:
	FORCEINLINE virtual bool Compare(const TSharedPtr<FAssetViewItem>& A, const TSharedPtr<FAssetViewItem>& B) const override
	{
		// Depending if we're sorting ascending or descending it's quicker to flip the compares incase tags are missing
		const FString* Value1 = bAscending ? StaticCastSharedPtr<FAssetViewAsset>(A)->Data.TagsAndValues.Find(Tag) : StaticCastSharedPtr<FAssetViewAsset>(B)->Data.TagsAndValues.Find(Tag);
		if (!Value1)
		{
			return true;
		}

		const FString* Value2 = bAscending ? StaticCastSharedPtr<FAssetViewAsset>(B)->Data.TagsAndValues.Find(Tag) : StaticCastSharedPtr<FAssetViewAsset>(A)->Data.TagsAndValues.Find(Tag);
		if (!Value2)
		{
			return false;
		}

		float Num1 = 1.f;
		{
			TArray<FString> Tokens1;
			Value1->ParseIntoArray(Tokens1, TEXT("x"), true);
			for (auto TokenIt1 = Tokens1.CreateConstIterator(); TokenIt1; ++TokenIt1)
			{
				Num1 *= FCString::Atof(**TokenIt1);
			}
		}
		float Num2 = 1.f;
		{
			TArray<FString> Tokens2;
			Value2->ParseIntoArray(Tokens2, TEXT("x"), true);
			for (auto TokenIt2 = Tokens2.CreateConstIterator(); TokenIt2; ++TokenIt2)
			{
				Num2 *= FCString::Atof(**TokenIt2);
			}
		}
		if (Num1 < Num2)
		{
			return true;
		}
		else if (Num1 > Num2)
		{
			return false;
		}
		return FCompareFAssetItemBase::Compare(A, B);
	}
};

const FName FAssetViewSortManager::NameColumnId = "Name";
const FName FAssetViewSortManager::ClassColumnId = "Class";
const FName FAssetViewSortManager::PathColumnId = "Path";

FAssetViewSortManager::FAssetViewSortManager()
{
	ResetSort();
}

void FAssetViewSortManager::ResetSort()
{
	SortColumnId[EColumnSortPriority::Primary] = NameColumnId;
	SortMode[EColumnSortPriority::Primary] = EColumnSortMode::Ascending;
	for (int32 PriorityIdx = 1; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
	{
		SortColumnId[PriorityIdx] = NAME_None;
		SortMode[PriorityIdx] = EColumnSortMode::None;
	}
}

void FAssetViewSortManager::SortList(TArray<TSharedPtr<FAssetViewItem>>& AssetItems, const FName& MajorityAssetType) const
{
	//double SortListStartTime = FPlatformTime::Seconds();

	TArray<TUniquePtr<FCompareFAssetItemBase>> SortMethod;
	for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
	{
		const bool bAscending(SortMode[PriorityIdx] == EColumnSortMode::Ascending);
		const FName& Tag(SortColumnId[PriorityIdx]);

		if (Tag == NAME_None)
		{
			break;
		}

		if (Tag == NameColumnId)
		{
			SortMethod.Add(MakeUnique<FCompareFAssetItemByName>(bAscending, Tag));
		}
		else if (Tag == ClassColumnId)
		{
			SortMethod.Add(MakeUnique<FCompareFAssetItemByClass>(bAscending, Tag));
		}
		else if (Tag == PathColumnId)
		{
			SortMethod.Add(MakeUnique<FCompareFAssetItemByPath>(bAscending, Tag));
		}
		else
		{
			// Since this SortData.Tag is not one of preset columns, sort by asset registry tag
			UObject::FAssetRegistryTag::ETagType TagType = UObject::FAssetRegistryTag::TT_Alphabetical;
			if (MajorityAssetType != NAME_None)
			{
				UClass* Class = FindObject<UClass>(ANY_PACKAGE, *MajorityAssetType.ToString());
				if (Class)
				{
					UObject* CDO = Class->GetDefaultObject();
					if (CDO)
					{
						TArray<UObject::FAssetRegistryTag> TagList;
						CDO->GetAssetRegistryTags(TagList);

						for (auto TagIt = TagList.CreateConstIterator(); TagIt; ++TagIt)
						{
							if (TagIt->Name == Tag)
							{
								TagType = TagIt->Type;
								break;
							}
						}
					}
				}
			}

			if (TagType == UObject::FAssetRegistryTag::TT_Numerical)
			{
				// The property is a Number, compare using atof
				SortMethod.Add(MakeUnique<FCompareFAssetItemByTagNumerical>(bAscending, Tag));
			}
			else if (TagType == UObject::FAssetRegistryTag::TT_Dimensional)
			{
				// The property is a series of Numbers representing dimensions, compare by using atof for each Number, delimited by an "x"
				SortMethod.Add(MakeUnique<FCompareFAssetItemByTagDimensional>(bAscending, Tag));
			}
			else
			{
				// Unknown or alphabetical, sort alphabetically either way
				SortMethod.Add(MakeUnique<FCompareFAssetItemByTag>(bAscending, Tag));
			}
		}
	}

	// Sort the list...
	if (SortMethod.Num() > 0)
	{
		TUniquePtr<FCompareFAssetItemBase> PrimarySortMethod = MoveTemp(SortMethod[EColumnSortPriority::Primary]);
		check(PrimarySortMethod);
		SortMethod.RemoveAt(0);

		// Move all the comparisons to the primary sort method
		PrimarySortMethod->SetNextComparisons(SortMethod);
		AssetItems.Sort(*(PrimarySortMethod.Get()));

		// Move the comparisons back for ease of cleanup
		SortMethod = MoveTemp(PrimarySortMethod->GetNextComparisons());
		SortMethod.Insert(MoveTemp(PrimarySortMethod), 0);
	}

	// Cleanup the methods we no longer need.
	for (int32 PriorityIdx = 0; PriorityIdx < SortMethod.Num(); PriorityIdx++)
	{
		SortMethod[PriorityIdx].Reset();
	}
	SortMethod.Empty();

	//UE_LOG(LogContentBrowser, Warning/*VeryVerbose*/, TEXT("FAssetViewSortManager Sort Time: %0.4f seconds."), FPlatformTime::Seconds() - SortListStartTime);
}

void FAssetViewSortManager::SetSortColumnId(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId)
{
	check(InSortPriority < EColumnSortPriority::Max);
	SortColumnId[InSortPriority] = InColumnId;

	// Prevent the same ColumnId being assigned to multiple columns
	bool bOrderChanged = false;
	for (int32 PriorityIdxA = 0; PriorityIdxA < EColumnSortPriority::Max; PriorityIdxA++)
	{
		for (int32 PriorityIdxB = 0; PriorityIdxB < EColumnSortPriority::Max; PriorityIdxB++)
		{
			if (PriorityIdxA != PriorityIdxB)
			{
				if (SortColumnId[PriorityIdxA] == SortColumnId[PriorityIdxB] && SortColumnId[PriorityIdxB] != NAME_None)
				{
					SortColumnId[PriorityIdxB] = NAME_None;
					bOrderChanged = true;
				}
			}
		}
	}
	if (bOrderChanged)
	{
		// If the order has changed, we need to remove any unneeded sorts by bumping the priority of the remaining valid ones
		for (int32 PriorityIdxA = 0, PriorityNum = 0; PriorityNum < EColumnSortPriority::Max - 1; PriorityNum++, PriorityIdxA++)
		{
			if (SortColumnId[PriorityIdxA] == NAME_None)
			{
				for (int32 PriorityIdxB = PriorityIdxA; PriorityIdxB < EColumnSortPriority::Max - 1; PriorityIdxB++)
				{
					SortColumnId[PriorityIdxB] = SortColumnId[PriorityIdxB + 1];
					SortMode[PriorityIdxB] = SortMode[PriorityIdxB + 1];
				}
				SortColumnId[EColumnSortPriority::Max - 1] = NAME_None;
				PriorityIdxA--;
			}
		}
	}
}

void FAssetViewSortManager::SetSortMode(const EColumnSortPriority::Type InSortPriority, const EColumnSortMode::Type InSortMode)
{
	check(InSortPriority < EColumnSortPriority::Max);
	SortMode[InSortPriority] = InSortMode;
}

bool FAssetViewSortManager::SetOrToggleSortColumn(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId)
{
	check(InSortPriority < EColumnSortPriority::Max);
	if (SortColumnId[InSortPriority] != InColumnId)
	{
		// Clicked a new column, default to ascending
		SortColumnId[InSortPriority] = InColumnId;
		SortMode[InSortPriority] = EColumnSortMode::Ascending;
		return true;
	}
	else
	{
		// Clicked the current column, toggle sort mode
		if (SortMode[InSortPriority] == EColumnSortMode::Ascending)
		{
			SortMode[InSortPriority] = EColumnSortMode::Descending;
		}
		else
		{
			SortMode[InSortPriority] = EColumnSortMode::Ascending;
		}
		return false;
	}
}
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "GroupedKeyArea.h"
#include "MovieSceneSection.h"

FIndexKey::FIndexKey(FString InNodePath, UMovieSceneSection* InSection)
	: NodePath(MoveTemp(InNodePath))
	, Section(InSection)
	, CachedHash(HashCombine(GetTypeHash(NodePath), GetTypeHash(InSection)))
{}

namespace 
{
	/** Structure that defines the index for a particular section */
	struct FIndexEntry
	{
		TMap<FKeyHandle, int32> HandleToGroup;
		TArray<FKeyHandle> GroupHandles;
		TArray<float> RepresentativeTimes;
	};

	/** A persistent index is required to ensure that generated key handles are maintained for the lifetime of specific display nodes */
	TMap<FIndexKey, FIndexEntry> GlobalIndex;
}

FGroupedKeyCollection::FGroupedKeyCollection()
	: IndexKey(FString(), nullptr)
{
	GroupingThreshold = SMALL_NUMBER;
}

void FGroupedKeyCollection::InitializeExplicit(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold)
{
	KeyAreas.Reset();
	Groups.Reset();

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	for (const FSequencerDisplayNode* Node : InNodes)
	{
		const FSequencerSectionKeyAreaNode* KeyAreaNode = nullptr;

		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			KeyAreaNode = static_cast<const FSequencerSectionKeyAreaNode*>(Node);
		}
		else if (Node->GetType() == ESequencerNode::Track)
		{
			KeyAreaNode = static_cast<const FSequencerTrackNode*>(Node)->GetTopLevelKeyNode().Get();
			if (!KeyAreaNode)
			{
				continue;
			}
		}
		else
		{
			continue;
		}

		const TArray<TSharedRef<IKeyArea>>& AllKeyAreas = KeyAreaNode->GetAllKeyAreas();
		KeyAreas.Reserve(KeyAreas.Num() + AllKeyAreas.Num());

		for (const TSharedRef<IKeyArea>& KeyArea : AllKeyAreas)
		{
			AddKeyArea(KeyArea);
		}
	}

	RemoveDuplicateKeys(DuplicateThreshold);
}

void FGroupedKeyCollection::InitializeRecursive(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold)
{
	KeyAreas.Reset();
	Groups.Reset();

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	for (FSequencerDisplayNode* Node : InNodes)
	{
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			AllKeyAreaNodes.Add(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node->AsShared()));
		}

		Node->GetChildKeyAreaNodesRecursively(AllKeyAreaNodes);
	}

	for (const auto& Node : AllKeyAreaNodes)
	{
		const TArray<TSharedRef<IKeyArea>>& AllKeyAreas = Node->GetAllKeyAreas();
		KeyAreas.Reserve(KeyAreas.Num() + AllKeyAreas.Num());

		for (const TSharedRef<IKeyArea>& KeyArea : AllKeyAreas)
		{
			AddKeyArea(KeyArea);
		}
	}

	RemoveDuplicateKeys(DuplicateThreshold);
}

void FGroupedKeyCollection::InitializeRecursive(FSequencerDisplayNode& InNode, int32 InSectionIndex, float DuplicateThreshold)
{
	KeyAreas.Reset();
	Groups.Reset();

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	InNode.GetChildKeyAreaNodesRecursively(AllKeyAreaNodes);

	for (const auto& Node : AllKeyAreaNodes)
	{
		TSharedPtr<IKeyArea> KeyArea = Node->GetKeyArea(InSectionIndex);
		if (KeyArea.IsValid())
		{
			AddKeyArea(KeyArea.ToSharedRef());
		}
	}

	RemoveDuplicateKeys(DuplicateThreshold);
}

void FGroupedKeyCollection::AddKeyArea(const TSharedRef<IKeyArea>& InKeyArea)
{
	const int32 KeyAreaIndex = KeyAreas.Num();
	KeyAreas.Add(InKeyArea);

	TArray<FKeyHandle> AllKeyHandles = InKeyArea->GetUnsortedKeyHandles();
	Groups.Reserve(Groups.Num() + AllKeyHandles.Num());

	for (const FKeyHandle& KeyHandle : AllKeyHandles)
	{
		Groups.Emplace(InKeyArea->GetKeyTime(KeyHandle), KeyAreaIndex, KeyHandle);
	}
}

void FGroupedKeyCollection::RemoveDuplicateKeys(float DuplicateThreshold)
{
	GroupingThreshold = DuplicateThreshold;

	Groups.Sort([](const FKeyGrouping& A, const FKeyGrouping& B){
		return A.RepresentativeTime < B.RepresentativeTime;
	});

	// Remove duplicates
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		const float CurrentTime = Groups[Index].RepresentativeTime;

		int32 NumToMerge = 0;
		for (int32 DuplIndex = Index + 1; DuplIndex < Groups.Num(); ++DuplIndex)
		{
			if (!FMath::IsNearlyEqual(CurrentTime, Groups[DuplIndex].RepresentativeTime, DuplicateThreshold))
			{
				break;
			}
			++NumToMerge;
		}

		if (NumToMerge)
		{
			const int32 Start = Index + 1, End = Start + NumToMerge;
			for (int32 MergeIndex = Start; MergeIndex < End; ++MergeIndex)
			{
				Groups[Index].Keys.Append(MoveTemp(Groups[MergeIndex].Keys));
			}

			Groups.RemoveAt(Start, NumToMerge, false);
		}
	}
}

void FGroupedKeyCollection::UpdateIndex() const
{
	auto& IndexEntry = GlobalIndex.FindOrAdd(IndexKey);

	TArray<FKeyHandle> NewKeyHandles;
	TArray<float> NewRepresentativeTimes;

	IndexEntry.HandleToGroup.Reset();

	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		float RepresentativeTime = Groups[GroupIndex].RepresentativeTime;

		// Find a key handle we can recycle
		int32 RecycledIndex = IndexEntry.RepresentativeTimes.IndexOfByPredicate([&](float Time){
			// Must be an *exact* match to recycle
			return Time == RepresentativeTime;
		});
		
		if (RecycledIndex != INDEX_NONE)
		{
			NewKeyHandles.Add(IndexEntry.GroupHandles[RecycledIndex]);
			NewRepresentativeTimes.Add(IndexEntry.RepresentativeTimes[RecycledIndex]);

			IndexEntry.GroupHandles.RemoveAt(RecycledIndex, 1, false);
			IndexEntry.RepresentativeTimes.RemoveAt(RecycledIndex, 1, false);
		}
		else
		{
			NewKeyHandles.Add(FKeyHandle());
			NewRepresentativeTimes.Add(RepresentativeTime);
		}

		IndexEntry.HandleToGroup.Add(NewKeyHandles.Last(), GroupIndex);
	}

	IndexEntry.GroupHandles = MoveTemp(NewKeyHandles);
	IndexEntry.RepresentativeTimes = MoveTemp(NewRepresentativeTimes);
}

void FGroupedKeyCollection::IterateKeys(const TFunctionRef<bool(float)>& Iter)
{
	for (const FKeyGrouping& Grouping : Groups)
	{
		if (!Iter(Grouping.RepresentativeTime))
		{
			return;
		}
	}
}

float FGroupedKeyCollection::GetKeyGroupingThreshold() const
{
	return GroupingThreshold;
}

TOptional<float> FGroupedKeyCollection::FindFirstKeyInRange(const TRange<float>& InRange, EFindKeyDirection Direction) const
{
	// @todo: linear search may be slow where there are lots of keys

	bool bWithinRange = false;
	if (Direction == EFindKeyDirection::Backwards)
	{
		for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
		{
			if (Groups[Index].RepresentativeTime < InRange.GetUpperBoundValue())
			{
				// Just entered the range
				return Groups[Index].RepresentativeTime;
			}
			else if (InRange.HasLowerBound() && Groups[Index].RepresentativeTime < InRange.GetLowerBoundValue())
			{
				// No longer inside the range
				return TOptional<float>();
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < Groups.Num(); ++Index)
		{
			if (Groups[Index].RepresentativeTime > InRange.GetLowerBoundValue())
			{
				// Just entered the range
				return Groups[Index].RepresentativeTime;
			}
			else if (InRange.HasUpperBound() && Groups[Index].RepresentativeTime > InRange.GetUpperBoundValue())
			{
				// No longer inside the range
				return TOptional<float>();
			}
		}
	}

	return TOptional<float>();
}

const FKeyGrouping* FGroupedKeyCollection::FindGroup(FKeyHandle InHandle) const
{
	if (auto* IndexEntry = GlobalIndex.Find(IndexKey))
	{
		if (int32* GroupIndex = IndexEntry->HandleToGroup.Find(InHandle))
		{
			return Groups.IsValidIndex(*GroupIndex) ? &Groups[*GroupIndex] : nullptr;
		}
	}
	return nullptr;
}

FKeyGrouping* FGroupedKeyCollection::FindGroup(FKeyHandle InHandle)
{
	if (auto* IndexEntry = GlobalIndex.Find(IndexKey))
	{
		if (int32* GroupIndex = IndexEntry->HandleToGroup.Find(InHandle))
		{
			return Groups.IsValidIndex(*GroupIndex) ? &Groups[*GroupIndex] : nullptr;
		}
	}
	return nullptr;
}

FLinearColor FGroupedKeyCollection::GetKeyTint(FKeyHandle InHandle) const
{
	// Everything is untinted for now
	return FLinearColor::White;
}

const FSlateBrush* FGroupedKeyCollection::GetBrush(FKeyHandle InHandle) const
{
	static const FSlateBrush* PartialKeyBrush = FEditorStyle::GetBrush("Sequencer.PartialKey");
	const auto* Group = FindGroup(InHandle);

	// Ensure that each key area is represented at least once for it to be considered a 'complete key'
	for (int32 AreaIndex = 0; Group && AreaIndex < KeyAreas.Num(); ++AreaIndex)
	{
		if (!Group->Keys.ContainsByPredicate([&](const FKeyGrouping::FKeyIndex& Key){ return Key.AreaIndex == AreaIndex; }))
		{
			return PartialKeyBrush;
		}
	}

	return nullptr;
}

FGroupedKeyArea::FGroupedKeyArea(FSequencerDisplayNode& InNode, int32 InSectionIndex)
	: Section(nullptr)
{
	// Calling this virtual function is safe here, as it's just called through standard name-lookup, not runtime dispatch
	FGroupedKeyCollection::InitializeRecursive(InNode, InSectionIndex);

	for (const TSharedPtr<IKeyArea>& KeyArea : KeyAreas)
	{
		auto* OwningSection = KeyArea->GetOwningSection();
		// Ensure they all belong to the same section
		ensure(!Section || Section == OwningSection);
		Section = OwningSection;
	}

	if (Section)
	{
		IndexKey = FIndexKey(InNode.GetPathName(), Section);
		UpdateIndex();
	}
}

TArray<FKeyHandle> FGroupedKeyArea::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> Array;
	if (auto* IndexEntry = GlobalIndex.Find(IndexKey))
	{
		IndexEntry->HandleToGroup.GenerateKeyArray(Array);
	}
	return Array;
}

void FGroupedKeyArea::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) const
{
	auto* Group = FindGroup(KeyHandle);

	for (auto& Key : Group->Keys)
	{
		KeyAreas[Key.AreaIndex]->SetKeyTime(Key.KeyHandle, NewKeyTime);
	}
}

float FGroupedKeyArea::GetKeyTime(FKeyHandle KeyHandle) const
{
	auto* Group = FindGroup(KeyHandle);
	return Group ? Group->RepresentativeTime : 0.f;
}

FKeyHandle FGroupedKeyArea::MoveKey(FKeyHandle KeyHandle, float DeltaPosition)
{
	int32* GroupIndex = nullptr;

	auto* IndexEntry = GlobalIndex.Find(IndexKey);
	if (IndexEntry)
	{
		GroupIndex = IndexEntry->HandleToGroup.Find(KeyHandle);
	}

	if (!GroupIndex || !Groups.IsValidIndex(*GroupIndex))
	{
		return KeyHandle;
	}

	bool bIsTimeUpdated = false;

	FKeyGrouping& Group = Groups[*GroupIndex];

	// Move all the keys
	for (auto& Key : Group.Keys)
	{
		Key.KeyHandle = KeyAreas[Key.AreaIndex]->MoveKey(Key.KeyHandle, DeltaPosition);

		const float KeyTime = KeyAreas[Key.AreaIndex]->GetKeyTime(Key.KeyHandle);
		Group.RepresentativeTime = bIsTimeUpdated ? FMath::Min(Group.RepresentativeTime, KeyTime) : KeyTime;
		bIsTimeUpdated = true;
	}

	// Update the representative time to the smallest of all the keys (so it will deterministically get the same key handle on regeneration)
	IndexEntry->RepresentativeTimes[*GroupIndex] = Group.RepresentativeTime;

	return KeyHandle;
}

void FGroupedKeyArea::DeleteKey(FKeyHandle KeyHandle)
{
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			KeyAreas[Key.AreaIndex]->DeleteKey(Key.KeyHandle);
		}
	}
}

void                    
FGroupedKeyArea::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			KeyAreas[Key.AreaIndex]->SetKeyInterpMode(Key.KeyHandle, InterpMode);
		}
	}
}

ERichCurveInterpMode
FGroupedKeyArea::GetKeyInterpMode(FKeyHandle KeyHandle) const
{
	// Return RCIM_None if the keys don't all have the same interp mode
	ERichCurveInterpMode InterpMode = RCIM_None;
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			if (InterpMode == RCIM_None)
			{
				InterpMode = KeyAreas[Key.AreaIndex]->GetKeyInterpMode(Key.KeyHandle);
			}
			else if (InterpMode != KeyAreas[Key.AreaIndex]->GetKeyInterpMode(Key.KeyHandle))
			{
				return RCIM_None;
			}
		}
	}
	return InterpMode;
}

void                    
FGroupedKeyArea::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			KeyAreas[Key.AreaIndex]->SetKeyTangentMode(Key.KeyHandle, TangentMode);
		}
	}
}

ERichCurveTangentMode
FGroupedKeyArea::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	// Return RCTM_None if the keys don't all have the same tangent mode
	ERichCurveTangentMode TangentMode = RCTM_None;
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			if (TangentMode == RCTM_None)
			{
				TangentMode = KeyAreas[Key.AreaIndex]->GetKeyTangentMode(Key.KeyHandle);
			}
			else if (TangentMode != KeyAreas[Key.AreaIndex]->GetKeyTangentMode(Key.KeyHandle))
			{
				return RCTM_None;
			}
		}
	}
	return TangentMode;
}

void FGroupedKeyArea::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	for (auto& Area : KeyAreas)
	{
		Area->SetExtrapolationMode(ExtrapMode, bPreInfinity);
	}
}

ERichCurveExtrapolation FGroupedKeyArea::GetExtrapolationMode(bool bPreInfinity) const
{
	ERichCurveExtrapolation ExtrapMode = RCCE_None;
	for (auto& Area : KeyAreas)
	{
		if (ExtrapMode == RCCE_None)
		{
			ExtrapMode = Area->GetExtrapolationMode(bPreInfinity);
		}
		else if (Area->GetExtrapolationMode(bPreInfinity) != ExtrapMode)
		{
			return RCCE_None;
		}
	}
	return ExtrapMode;
}

bool FGroupedKeyArea::CanSetExtrapolationMode() const
{
	for (auto& Area : KeyAreas)
	{
		if (Area->CanSetExtrapolationMode())
		{
			return true;
		}
	}
	return false;
}

TArray<FKeyHandle> FGroupedKeyArea::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
{
	TArray<FKeyHandle> AddedKeyHandles;

	for (auto& Area : KeyAreas)
	{
		// If TimeToCopyFrom is valid, add a key only if there is a key to copy from
		if (TimeToCopyFrom != FLT_MAX)
		{
			if (FRichCurve* Curve = Area->GetRichCurve())
			{
				if (!Curve->IsKeyHandleValid(Curve->FindKey(TimeToCopyFrom)))
				{
					continue;
				}
			}
		}

		TArray<FKeyHandle> AddedGroupKeyHandles = Area->AddKeyUnique(Time, InKeyInterpolation, TimeToCopyFrom);
		AddedKeyHandles.Append(AddedGroupKeyHandles);
	}

	return AddedKeyHandles;
}

TOptional<FKeyHandle> FGroupedKeyArea::DuplicateKey(FKeyHandle KeyToDuplicate)
{
	FKeyGrouping* Group = FindGroup(KeyToDuplicate);
	if (!Group)
	{
		return TOptional<FKeyHandle>();
	}

	const float Time = Group->RepresentativeTime;

	const int32 NewGroupIndex = Groups.Num();
	Groups.Emplace(Time);
	
	for (const FKeyGrouping::FKeyIndex& Key : Group->Keys)
	{
		TOptional<FKeyHandle> NewKeyHandle = KeyAreas[Key.AreaIndex]->DuplicateKey(Key.KeyHandle);
		if (NewKeyHandle.IsSet())
		{
			Groups[NewGroupIndex].Keys.Emplace(Key.AreaIndex, NewKeyHandle.GetValue());
		}
	}

	// Update the global index with our new key
	FIndexEntry* IndexEntry = GlobalIndex.Find(IndexKey);
	if (IndexEntry)
	{
		FKeyHandle ThisGroupKeyHandle;

		IndexEntry->GroupHandles.Add(ThisGroupKeyHandle);
		IndexEntry->HandleToGroup.Add(ThisGroupKeyHandle, NewGroupIndex);
		IndexEntry->RepresentativeTimes.Add(Time);

		return ThisGroupKeyHandle;
	}

	return TOptional<FKeyHandle>();
}

FRichCurve* FGroupedKeyArea::GetRichCurve()
{
	return nullptr;
}

UMovieSceneSection* FGroupedKeyArea::GetOwningSection()
{
	return Section;
}

bool FGroupedKeyArea::CanCreateKeyEditor()
{
	return false;
}


void FGroupedKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const
{
	const FIndexEntry* IndexEntry = GlobalIndex.Find(IndexKey);
	if (!IndexEntry)
	{
		return;
	}

	// Since we are a group of nested key areas, we test the key mask for our key handles, and forward on the results to each key area

	// Using ptr as map key is fine here as we know they will not change
	TMap<const IKeyArea*, TSet<FKeyHandle>> AllValidHandles;

	for (auto& Pair : IndexEntry->HandleToGroup)
	{
		if (!KeyMask(Pair.Key, *this) || !Groups.IsValidIndex(Pair.Value))
		{
			continue;
		}

		const FKeyGrouping& Group = Groups[Pair.Value];
		for (const FKeyGrouping::FKeyIndex& KeyIndex : Group.Keys)
		{
			const IKeyArea& KeyArea = KeyAreas[KeyIndex.AreaIndex].Get();
			AllValidHandles.FindOrAdd(&KeyArea).Add(KeyIndex.KeyHandle);
		}
	}

	for (auto& Pair : AllValidHandles)
	{
		Pair.Key->CopyKeys(ClipboardBuilder, [&](FKeyHandle Handle, const IKeyArea&){
			return Pair.Value.Contains(Handle);
		});
	}
}


TSharedRef<SWidget> FGroupedKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNullWidget::NullWidget;
}


TOptional<FLinearColor> FGroupedKeyArea::GetColor()
{
	return TOptional<FLinearColor>();
}


TSharedPtr<FStructOnScope> FGroupedKeyArea::GetKeyStruct(FKeyHandle KeyHandle)
{
	FKeyGrouping* Group = FindGroup(KeyHandle);
	
	if (Group == nullptr)
	{
		return nullptr;
	}

	TArray<FKeyHandle> KeyHandles;

	for (auto& Key : Group->Keys)
	{
		KeyHandles.Add(Key.KeyHandle);
	}

	return Section->GetKeyStruct(KeyHandles);
}


void FGroupedKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
{
	checkf(false, TEXT("Pasting into grouped key areas is not supported, and should not be used. Iterate child tracks instead."));
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SectionLayoutBuilder.h"
#include "ISequencerSection.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "SSequencer.h"
#include "SSequencerSectionAreaView.h"
#include "MovieSceneSection.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "CommonMovieSceneTools.h"
#include "IKeyArea.h"
#include "ISequencerObjectBindingManager.h"


class SSequencerObjectTrack : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSequencerObjectTrack) {}
		/** The view range of the section area */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
	SLATE_END_ARGS()

	/** SLeafWidget Interface */
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	
	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> InRootNode )
	{
		RootNode = InRootNode;
		
		ViewRange = InArgs._ViewRange;

		check(RootNode->GetType() == ESequencerNode::Object);
	}

private:
	/** Collects all key times from the root node */
	void CollectAllKeyTimes(TArray<float>& OutKeyTimes) const;

	/** Adds a key time uniquely to an array of key times */
	void AddKeyTime(float NewTime, TArray<float>& OutKeyTimes) const;

private:
	/** Root node of this track view panel */
	TSharedPtr<FSequencerDisplayNode> RootNode;
	/** The current view range */
	TAttribute< TRange<float> > ViewRange;
};

int32 SSequencerObjectTrack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Draw a region around the entire section area
	FSlateDrawElement::MakeBox( 
		OutDrawElements, 
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FEditorStyle::GetBrush("Sequencer.SectionArea.Background"),
		MyClippingRect,
		ESlateDrawEffect::None,
		FLinearColor( .1f, .1f, .1f, 0.5f )
		);

	TArray<float> OutKeyTimes;
	CollectAllKeyTimes(OutKeyTimes);
	
	FTimeToPixel TimeToPixelConverter(AllottedGeometry, ViewRange.Get());

	for (int32 i = 0; i < OutKeyTimes.Num(); ++i)
	{
		float KeyPosition = TimeToPixelConverter.TimeToPixel(OutKeyTimes[i]);

		static const FVector2D KeyMarkSize = FVector2D(3.f, 21.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry(FVector2D(KeyPosition - FMath::CeilToFloat(KeyMarkSize.X/2.f), FMath::CeilToFloat(AllottedGeometry.Size.Y/2.f - KeyMarkSize.Y/2.f)), KeyMarkSize),
			FEditorStyle::GetBrush("Sequencer.KeyMark"),
			MyClippingRect,
			ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 1.f)
		);
	}

	return LayerId+1;
}

FVector2D SSequencerObjectTrack::ComputeDesiredSize( float ) const
{
	// Note: X Size is not used
	return FVector2D( 100.0f, RootNode->GetNodeHeight() );
}

void SSequencerObjectTrack::CollectAllKeyTimes(TArray<float>& OutKeyTimes) const
{
	TArray< TSharedRef<FSectionKeyAreaNode> > OutNodes;
	RootNode->GetChildKeyAreaNodesRecursively(OutNodes);

	for (int32 i = 0; i < OutNodes.Num(); ++i)
	{
		TArray< TSharedRef<IKeyArea> > KeyAreas = OutNodes[i]->GetAllKeyAreas();
		for (int32 j = 0; j < KeyAreas.Num(); ++j)
		{
			TArray<FKeyHandle> KeyHandles = KeyAreas[j]->GetUnsortedKeyHandles();
			for (int32 k = 0; k < KeyHandles.Num(); ++k)
			{
				AddKeyTime(KeyAreas[j]->GetKeyTime(KeyHandles[k]), OutKeyTimes);
			}
		}
	}
}

void SSequencerObjectTrack::AddKeyTime(float NewTime, TArray<float>& OutKeyTimes) const
{
	// @todo Sequencer It might be more efficient to add each key and do the pruning at the end
	for (int32 i = 0; i < OutKeyTimes.Num(); ++i)
	{
		if (FMath::IsNearlyEqual(OutKeyTimes[i], NewTime))
		{
			return;
		}
	}
	OutKeyTimes.Add(NewTime);
}



FSequencerDisplayNode::FSequencerDisplayNode( FName InNodeName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: ParentNode( InParentNode )
	, ParentTree( InParentTree )
	, NodeName( InNodeName )
	, TreeLevel( InParentNode.IsValid() ? InParentNode->GetTreeLevel() + 1 : 0 )
	, bExpanded( false )
	, bCachedShotFilteredVisibility( true )
	, bNodeIsPinned( false )
{
	bExpanded = ParentTree.GetSavedExpansionState( *this );
}

TSharedRef<FSectionCategoryNode> FSequencerDisplayNode::AddCategoryNode( FName CategoryName, const FText& DisplayLabel )
{
	TSharedPtr<FSectionCategoryNode> CategoryNode;

	// See if there is an already existing category node to use
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FSequencerDisplayNode>& ChildNode = ChildNodes[ChildIndex];
		if( ChildNode->GetNodeName() == CategoryName && ChildNode->GetType() == ESequencerNode::Category )
		{
			CategoryNode = StaticCastSharedRef<FSectionCategoryNode>( ChildNode );
		}
	}

	if( !CategoryNode.IsValid() )
	{
		// No existing category found, make a new one
		CategoryNode = MakeShareable( new FSectionCategoryNode( CategoryName, DisplayLabel, SharedThis( this ), ParentTree ) );
		ChildNodes.Add( CategoryNode.ToSharedRef() );
	}

	return CategoryNode.ToSharedRef();
}

TSharedRef<FTrackNode> FSequencerDisplayNode::AddSectionAreaNode( FName SectionName, UMovieSceneTrack& AssociatedTrack )
{
	TSharedPtr<FTrackNode> SectionNode;

	// See if there is an already existing section node to use
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FSequencerDisplayNode>& ChildNode = ChildNodes[ChildIndex];
		if( ChildNode->GetNodeName() == SectionName && ChildNode->GetType() == ESequencerNode::Track )
		{
			SectionNode = StaticCastSharedRef<FTrackNode>( ChildNode );
		}
	}

	if( !SectionNode.IsValid() )
	{
		// No existing node found make a new one
		SectionNode = MakeShareable( new FTrackNode( SectionName, AssociatedTrack, SharedThis( this ), ParentTree ) );
		ChildNodes.Add( SectionNode.ToSharedRef() );
	}

	// The section node type has to match
	check( SectionNode->GetTrack() == &AssociatedTrack );

	return SectionNode.ToSharedRef();
}

void FSequencerDisplayNode::AddKeyAreaNode( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea )
{
	TSharedPtr<FSectionKeyAreaNode> KeyAreaNode;

	// See if there is an already existing key area node to use
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		TSharedRef<FSequencerDisplayNode>& ChildNode = ChildNodes[ChildIndex];
		if( ChildNode->GetNodeName() == KeyAreaName && ChildNode->GetType() == ESequencerNode::KeyArea )
		{
			KeyAreaNode = StaticCastSharedRef<FSectionKeyAreaNode>( ChildNode );
		}
	}

	if( !KeyAreaNode.IsValid() )
	{
		// No existing node found make a new one
		KeyAreaNode = MakeShareable( new FSectionKeyAreaNode( KeyAreaName, DisplayName, SharedThis( this ), ParentTree ) );
		ChildNodes.Add( KeyAreaNode.ToSharedRef() );
	}

	KeyAreaNode->AddKeyArea( KeyArea );
}

TSharedRef<SWidget> FSequencerDisplayNode::GenerateWidgetForOutliner( TSharedRef<class FSequencer> Sequencer )
{
	return SNew( SAnimationOutlinerView, SharedThis( this ), Sequencer );
}

TSharedRef<SWidget> FSequencerDisplayNode::GenerateWidgetForSectionArea( const TAttribute< TRange<float> >& ViewRange )
{
	if( GetType() == ESequencerNode::Track )
	{
		return 
			SNew( SSequencerSectionAreaView, SharedThis( this ) )
			.ViewRange( ViewRange );
	}
	else if (GetType() == ESequencerNode::Object)
	{
		return SNew(SSequencerObjectTrack, SharedThis(this))
			.ViewRange( ViewRange );
	}
	else
	{
		// Currently only section areas display widgets
		return SNullWidget::NullWidget;
	}
}

FString FSequencerDisplayNode::GetPathName() const
{
	// First get our parent's path
	FString PathName;
	if( ParentNode.IsValid() )
	{
		PathName = ParentNode.Pin()->GetPathName() + TEXT(".");
	}

	//then append our path
	PathName += GetNodeName().ToString();

	return PathName;
}

void FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(TArray< TSharedRef<FSectionKeyAreaNode> >& OutNodes) const
{
	for (int32 i = 0; i < ChildNodes.Num(); ++i)
	{
		if (ChildNodes[i]->GetType() == ESequencerNode::KeyArea)
		{
			OutNodes.Add(StaticCastSharedRef<FSectionKeyAreaNode>(ChildNodes[i]));
		}
		ChildNodes[i]->GetChildKeyAreaNodesRecursively(OutNodes);
	}
}

void FSequencerDisplayNode::ToggleExpansion()
{
	bExpanded = !bExpanded;

	// Expansion state has changed, save it to the movie scene now
	ParentTree.SaveExpansionState( *this, bExpanded );

	UpdateCachedShotFilteredVisibility();
}

bool FSequencerDisplayNode::IsExpanded() const
{
	return ParentTree.HasActiveFilter() ? ParentTree.IsNodeFiltered( AsShared() ) : bExpanded;
}

bool FSequencerDisplayNode::IsVisible() const
{
	// Must be visible after shot filtering AND
	// If there is a search filter, must be filtered, otherwise, it's parent must be expanded AND
	// If shot filtering is off on clean view is on, node must be pinned
	return bCachedShotFilteredVisibility &&
		(ParentTree.HasActiveFilter() ? ParentTree.IsNodeFiltered(AsShared()) : IsParentExpandedOrIsARootNode()) &&
		(GetSequencer().IsShotFilteringOn() || !GetDefault<USequencerSettings>()->GetIsUsingCleanView() || bNodeIsPinned);
}

bool FSequencerDisplayNode::HasVisibleChildren() const
{
	for (int32 i = 0; i < ChildNodes.Num(); ++i)
	{
		if (ChildNodes[i]->bCachedShotFilteredVisibility) {return true;}
	}
	return false;
}

bool FSequencerDisplayNode::IsParentExpandedOrIsARootNode() const
{
	return !ParentNode.IsValid() || ParentNode.Pin()->bExpanded;
}

void FSequencerDisplayNode::UpdateCachedShotFilteredVisibility()
{
	// Tell our children to update their visibility first
	for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
	{
		ChildNodes[ChildIndex]->UpdateCachedShotFilteredVisibility();
	}
	
	// then cache our visibility
	// this must be done after the children, because it relies on child cached visibility
	bCachedShotFilteredVisibility = GetShotFilteredVisibilityToCache();
}

void FSequencerDisplayNode::PinNode()
{
	bNodeIsPinned = true;
}



float FSectionKeyAreaNode::GetNodeHeight() const
{
	//@todo Sequencer - Should be defined by the key area probably
	return SequencerLayoutConstants::KeyAreaHeight;
}

bool FSectionKeyAreaNode::GetShotFilteredVisibilityToCache() const
{
	return true;
}

void FSectionKeyAreaNode::AddKeyArea( TSharedRef< IKeyArea> KeyArea )
{
	KeyAreas.Add( KeyArea );
}

FTrackNode::FTrackNode( FName NodeName, UMovieSceneTrack& InAssociatedType, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: FSequencerDisplayNode( NodeName, InParentNode, InParentTree )
	, AssociatedType( &InAssociatedType )
{
}

float FTrackNode::GetNodeHeight() const
{
	return Sections.Num() > 0 ? Sections[0]->GetSectionHeight() * (GetMaxRowIndex()+1) : SequencerLayoutConstants::SectionAreaDefaultHeight;
}

FText FTrackNode::GetDisplayName() const
{
	// @todo Sequencer - IS there a better way to get the section interface name for the animation outliner?
	return Sections.Num() > 0 ? Sections[0]->GetDisplayName() : FText::FromName(NodeName);
}

void FTrackNode::SetSectionAsKeyArea( TSharedRef<IKeyArea>& KeyArea )
{
	if( !TopLevelKeyNode.IsValid() )
	{
		bool bTopLevel = true;
		TopLevelKeyNode = MakeShareable( new FSectionKeyAreaNode( GetNodeName(), FText::GetEmpty(), NULL, ParentTree, bTopLevel ) );
	}

	TopLevelKeyNode->AddKeyArea( KeyArea );
}

bool FTrackNode::GetShotFilteredVisibilityToCache() const
{
	// if no child sections are visible, neither is the entire section
	bool bAnySectionsVisible = false;
	for (int32 i = 0; i < Sections.Num() && !bAnySectionsVisible; ++i)
	{
		bAnySectionsVisible = GetSequencer().IsSectionVisible(Sections[i]->GetSectionObject());
	}
	return AssociatedType->HasShowableData() && bAnySectionsVisible;
}

void FTrackNode::GetChildKeyAreaNodesRecursively(TArray< TSharedRef<class FSectionKeyAreaNode> >& OutNodes) const
{
	FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(OutNodes);
	if (TopLevelKeyNode.IsValid())
	{
		OutNodes.Add(TopLevelKeyNode.ToSharedRef());
	}
}

int32 FTrackNode::GetMaxRowIndex() const
{
	int32 MaxRowIndex = 0;
	for (int32 i = 0; i < Sections.Num(); ++i)
	{
		MaxRowIndex = FMath::Max(MaxRowIndex, Sections[i]->GetSectionObject()->GetRowIndex());
	}
	return MaxRowIndex;
}

void FTrackNode::FixRowIndices()
{
	if (AssociatedType->SupportsMultipleRows())
	{
		// remove any empty track rows by waterfalling down sections to be as compact as possible
		TArray< TArray< TSharedRef<ISequencerSection> > > TrackIndices;
		TrackIndices.AddZeroed(GetMaxRowIndex() + 1);
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			TrackIndices[Sections[i]->GetSectionObject()->GetRowIndex()].Add(Sections[i]);
		}

		int32 NewIndex = 0;
		for (int32 i = 0; i < TrackIndices.Num(); ++i)
		{
			const TArray< TSharedRef<ISequencerSection> >& SectionsForThisIndex = TrackIndices[i];
			if (SectionsForThisIndex.Num() > 0)
			{
				for (int32 j = 0; j < SectionsForThisIndex.Num(); ++j)
				{
					SectionsForThisIndex[j]->GetSectionObject()->SetRowIndex(NewIndex);
				}
				++NewIndex;
			}
		}
	}
	else
	{
		// non master tracks can only have a single row
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			Sections[i]->GetSectionObject()->SetRowIndex(0);
		}
	}
}

FText FObjectBindingNode::GetDisplayName() const
{
	FText DisplayName;
	return GetSequencer().GetObjectBindingManager()->TryGetObjectBindingDisplayName(ObjectBinding, DisplayName) ?
		DisplayName : DefaultDisplayName;
}

float FObjectBindingNode::GetNodeHeight() const
{ 
	return SequencerLayoutConstants::ObjectNodeHeight;
}

bool FObjectBindingNode::GetShotFilteredVisibilityToCache() const
{
	// if shot filtering is off and we are not unfilterable
	// always show the object nodes (clean view is handled elsewhere)
	return !GetSequencer().IsShotFilteringOn() || GetSequencer().IsObjectUnfilterable(ObjectBinding) || HasVisibleChildren();
}

TSharedPtr<SWidget> FObjectBindingNode::OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FSequencer& ParentSequencer = GetSequencer();

	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieScene();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding);
	
	// should exist, but also shouldn't be both a spawnable and a possessable
	check((Spawnable != NULL) ^ (Possessable != NULL));
	check((NULL == Spawnable) || (NULL != Spawnable->GetClass())  );
	const UClass* ObjectClass = Spawnable ? Spawnable->GetClass()->GetSuperClass() : Possessable->GetPossessedObjectClass();

	// @todo sequencer replace with UI Commands instead of faking it
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, NULL);

	GetSequencer().BuildObjectBindingContextMenu(MenuBuilder, ObjectBinding, ObjectClass);

	return MenuBuilder.MakeWidget();
}

float FSectionCategoryNode::GetNodeHeight() const 
{
	return SequencerLayoutConstants::CategoryNodeHeight;
}

bool FSectionCategoryNode::GetShotFilteredVisibilityToCache() const
{
	// this node is only visible if at least one child node is visible
	return HasVisibleChildren();
}

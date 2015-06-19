// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerCurveOwner.h"
#include "IKeyArea.h"
#include "MovieSceneSection.h"
#include "Sequencer.h"

void GetAllKeyAreaNodes( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree, TArray<TSharedRef<FSectionKeyAreaNode>>& KeyAreaNodes )
{
	TDoubleLinkedList<TSharedRef<FSequencerDisplayNode>> NodesToProcess;
	for ( auto RootNode : InSequencerNodeTree->GetRootNodes() )
	{
		NodesToProcess.AddTail( RootNode );
	}

	while ( NodesToProcess.Num() > 0 )
	{
		auto Node = NodesToProcess.GetHead();
		if ( Node->GetValue()->GetType() == ESequencerNode::KeyArea )
		{
			KeyAreaNodes.Add( StaticCastSharedRef<FSectionKeyAreaNode>( Node->GetValue() ) );
		}
		for ( auto ChildNode : Node->GetValue()->GetChildNodes() )
		{
			NodesToProcess.AddTail( ChildNode );
		}
		NodesToProcess.RemoveNode( Node );
	}
}

FName BuildCurveName( TSharedPtr<FSectionKeyAreaNode> KeyAreaNode )
{
	FString CurveName;
	TSharedPtr<FSequencerDisplayNode> CurrentNameNode = KeyAreaNode;
	TArray<FString> NameParts;
	while ( CurrentNameNode.IsValid() )
	{
		NameParts.Insert( CurrentNameNode->GetDisplayName().ToString(), 0);
		CurrentNameNode = CurrentNameNode->GetParent();
	}
	return FName(*FString::Join(NameParts, TEXT(" - ")));
}

FSequencerCurveOwner::FSequencerCurveOwner( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree )
{
	SequencerNodeTree = InSequencerNodeTree;

	TArray<TSharedRef<FSectionKeyAreaNode>> KeyAreaNodes;
	GetAllKeyAreaNodes( SequencerNodeTree, KeyAreaNodes );
	for ( TSharedRef<FSectionKeyAreaNode> KeyAreaNode : KeyAreaNodes )
	{
		for ( TSharedRef<IKeyArea> KeyArea : KeyAreaNode->GetAllKeyAreas() )
		{
			IKeyArea::FCurveInfo* CurveInfo = KeyArea->GetCurveInfo();
			if ( CurveInfo != nullptr )
			{
				bool bAddCurve = false;
				switch ( GetDefault<USequencerSettings>()->GetCurveVisibility() )
				{
				case ESequencerCurveVisibility::AllCurves:
					bAddCurve = true;
					break;
				case ESequencerCurveVisibility::SelectedCurves:
					bAddCurve = KeyAreaNode->GetSequencer().GetSelection()->IsSelected(KeyAreaNode);
					break;
				case ESequencerCurveVisibility::AnimatedCurves:
					bAddCurve = CurveInfo->Curve->GetNumKeys() > 0;
					break;
				}
				if ( bAddCurve )
				{
					FName CurveName = BuildCurveName(KeyAreaNode);
					Curves.Add( FRichCurveEditInfo( CurveInfo->Curve, CurveName ) );
					ConstCurves.Add( FRichCurveEditInfoConst( CurveInfo->Curve, CurveName ) );
					EditInfoToSectionMap.Add( FRichCurveEditInfo( CurveInfo->Curve, CurveName ), CurveInfo->OwningSection );
				}
			}
		}
	}
}

TArray<FRichCurveEditInfoConst> FSequencerCurveOwner::GetCurves() const
{
	return ConstCurves;
}

TArray<FRichCurveEditInfo> FSequencerCurveOwner::GetCurves()
{
	return Curves;
};

void FSequencerCurveOwner::ModifyOwner()
{
	TArray<UMovieSceneSection*> Owners;
	EditInfoToSectionMap.GenerateValueArray( Owners );
	for ( auto Owner : Owners )
	{
		Owner->Modify();
	}
}

void FSequencerCurveOwner::MakeTransactional()
{
	TArray<UMovieSceneSection*> Owners;
	EditInfoToSectionMap.GenerateValueArray( Owners );
	for ( auto Owner : Owners )
	{
		Owner->SetFlags( Owner->GetFlags() | RF_Transactional );
	}
}

void FSequencerCurveOwner::OnCurveChanged( const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos )
{
	// Whenever a curve changes make sure to resize it's section so that the curve fits.
	for ( auto& ChangedCurveEditInfo : ChangedCurveEditInfos )
	{
		UMovieSceneSection** OwningSection = EditInfoToSectionMap.Find(ChangedCurveEditInfo);
		if ( OwningSection != nullptr )
		{
			float CurveStart;
			float CurveEnd;
			ChangedCurveEditInfo.CurveToEdit->GetTimeRange(CurveStart, CurveEnd);
			if ( (*OwningSection)->GetStartTime() > CurveStart )
			{
				(*OwningSection)->SetStartTime(CurveStart);
			}			
			if ( (*OwningSection)->GetEndTime() < CurveEnd )
			{
				(*OwningSection)->SetEndTime( CurveEnd );
			}
		}
	}
}

bool FSequencerCurveOwner::IsValidCurve( FRichCurveEditInfo CurveInfo )
{
	return EditInfoToSectionMap.Contains(CurveInfo);
}
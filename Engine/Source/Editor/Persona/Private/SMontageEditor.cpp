// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SMontageEditor.h"
#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "Editor/Kismet/Public/SKismetInspector.h"
#include "SAnimMontageScrubPanel.h"
#include "SAnimMontagePanel.h"
#include "SAnimMontageSectionsPanel.h"
#include "ScopedTransaction.h"
#include "SAnimNotifyPanel.h"
#include "SAnimCurvePanel.h"
#include "AnimPreviewInstance.h"
#include "SAnimTimingPanel.h"

#define LOCTEXT_NAMESPACE "AnimSequenceEditor"

//////////////////////////////////////////////////////////////////////////
// SMontageEditor

TSharedRef<SWidget> SMontageEditor::CreateDocumentAnchor()
{
	return IDocumentation::Get()->CreateAnchor(TEXT("Engine/Animation/AnimMontage"));
}

void SMontageEditor::Construct(const FArguments& InArgs)
{
	MontageObj = InArgs._Montage;
	check(MontageObj);

	WeakPersona = InArgs._Persona;

	bDragging = false;
	bIsActiveTimerRegistered = false;

	SAnimEditorBase::Construct( SAnimEditorBase::FArguments()
		.Persona(InArgs._Persona)
		);

	TSharedPtr<FPersona> SharedPersona = PersonaPtr.Pin();
	if(SharedPersona.IsValid())
	{
		SharedPersona->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SMontageEditor::PostUndo ) );
		SharedPersona->RegisterOnPersonaRefresh(FPersona::FOnPersonaRefresh::CreateSP(this, &SMontageEditor::RebuildMontagePanel));
	}

	SAssignNew(AnimTimingPanel, SAnimTimingPanel)
		.InWeakPersona(PersonaPtr)
		.InSequence(MontageObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange);

	TAttribute<EVisibility> SectionVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(AnimTimingPanel.ToSharedRef(), &SAnimTimingPanel::IsElementDisplayVisible, ETimingElementType::Section));
	TAttribute<EVisibility> NotifyVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(AnimTimingPanel.ToSharedRef(), &SAnimTimingPanel::IsElementDisplayVisible, ETimingElementType::QueuedNotify));
	FOnGetTimingNodeVisibility TimingNodeVisibilityDelegate = FOnGetTimingNodeVisibility::CreateSP(AnimTimingPanel.ToSharedRef(), &SAnimTimingPanel::IsElementDisplayVisible);
	
	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimMontagePanel, SAnimMontagePanel )
		.Persona(PersonaPtr)
		.Montage(MontageObj)
		.MontageEditor(SharedThis(this))
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.SectionTimingNodeVisibility(SectionVisibility)
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimMontageSectionsPanel, SAnimMontageSectionsPanel )
		.Montage(MontageObj)
		.MontageEditor(SharedThis(this))
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		AnimTimingPanel.ToSharedRef()
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimNotifyPanel, SAnimNotifyPanel )
		.Persona(InArgs._Persona)
		.Sequence(MontageObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
		.OnSelectionChanged(this, &SAnimEditorBase::OnSelectionChanged)
		.MarkerBars(this, &SMontageEditor::GetMarkerBarInformation)
		.OnRequestRefreshOffsets(this, &SMontageEditor::RefreshNotifyTriggerOffsets)
		.OnGetTimingNodeVisibility(TimingNodeVisibilityDelegate)
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimCurvePanel, SAnimCurvePanel )
		.Persona(InArgs._Persona)
		.Sequence(MontageObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
	];

	if (MontageObj)
	{
		EnsureStartingSection();
		EnsureSlotNode();
	}

	CollapseMontage();
}

SMontageEditor::~SMontageEditor()
{
	TSharedPtr<FPersona> SharedPersona = PersonaPtr.Pin();
	if (SharedPersona.IsValid())
	{
		SharedPersona->UnregisterOnPostUndo(this);
		SharedPersona->UnregisterOnPersonaRefresh(this);
	}
}

TSharedRef<class SAnimationScrubPanel> SMontageEditor::ConstructAnimScrubPanel()
{
	return SAssignNew(AnimMontageScrubPanel, SAnimMontageScrubPanel)
		.Persona(PersonaPtr)
		.LockedSequence(MontageObj)
		.ViewInputMin(this, &SMontageEditor::GetViewMinInput)
		.ViewInputMax(this, &SMontageEditor::GetViewMaxInput)
		.OnSetInputViewRange(this, &SMontageEditor::SetInputViewRange)
		.bAllowZoom(true)
		.MontageEditor(SharedThis(this));
}

void SMontageEditor::SetMontageObj(UAnimMontage * NewMontage)
{
	MontageObj = NewMontage;

	if (MontageObj)
	{
		SetInputViewRange(0, MontageObj->SequenceLength); // FIXME
	}

	AnimMontagePanel->SetMontage(NewMontage);
	AnimNotifyPanel->SetSequence(NewMontage);
	AnimCurvePanel->SetSequence(NewMontage);
	// sequence editor locks the sequence, so it doesn't get replaced by clicking 
	AnimMontageScrubPanel->ReplaceLockedSequence(NewMontage);
}

bool SMontageEditor::ValidIndexes(int32 AnimSlotIndex, int32 AnimSegmentIndex) const
{
	return (MontageObj != NULL && MontageObj->SlotAnimTracks.IsValidIndex(AnimSlotIndex) && MontageObj->SlotAnimTracks[AnimSlotIndex].AnimTrack.AnimSegments.IsValidIndex(AnimSegmentIndex) );
}

bool SMontageEditor::ValidSection(int32 SectionIndex) const
{
	return (MontageObj != NULL && MontageObj->CompositeSections.IsValidIndex(SectionIndex));
}

void SMontageEditor::RefreshNotifyTriggerOffsets()
{
	for(auto Iter = MontageObj->Notifies.CreateIterator(); Iter; ++Iter)
	{
		FAnimNotifyEvent& Notify = (*Iter);

		// Offset for the beginning of a notify
		EAnimEventTriggerOffsets::Type PredictedOffset = MontageObj->CalculateOffsetForNotify(Notify.GetTime());
		Notify.RefreshTriggerOffset(PredictedOffset);

		// Offset for the end of a notify state if necessary
		if(Notify.GetDuration() > 0.0f)
		{
			PredictedOffset = MontageObj->CalculateOffsetForNotify(Notify.GetTime() + Notify.GetDuration());
			Notify.RefreshEndTriggerOffset(PredictedOffset);
		}
		else
		{
			Notify.EndTriggerTimeOffset = 0.0f;
		}
	}
}

bool SMontageEditor::GetSectionTime( int32 SectionIndex, float &OutTime ) const
{
	if (MontageObj != NULL && MontageObj->CompositeSections.IsValidIndex(SectionIndex))
	{
		OutTime = MontageObj->CompositeSections[SectionIndex].GetTime();
		return true;
	}

	return false;
}

TArray<FString>	SMontageEditor::GetSectionNames() const
{
	TArray<FString> Names;
	if (MontageObj != NULL)
	{
		for( int32 I=0; I < MontageObj->CompositeSections.Num(); I++)
		{
			Names.Add(MontageObj->CompositeSections[I].SectionName.ToString());
		}
	}
	return Names;
}

TArray<float> SMontageEditor::GetSectionStartTimes() const
{
	TArray<float>	Times;
	if (MontageObj != NULL)
	{
		for( int32 I=0; I < MontageObj->CompositeSections.Num(); I++)
		{
			Times.Add(MontageObj->CompositeSections[I].GetTime());
		}
	}
	return Times;
}

TArray<FTrackMarkerBar> SMontageEditor::GetMarkerBarInformation() const
{
	TArray<FTrackMarkerBar> MarkerBars;
	if (MontageObj != NULL)
	{
		for( int32 I=0; I < MontageObj->CompositeSections.Num(); I++)
		{
			FTrackMarkerBar Bar;
			Bar.Time = MontageObj->CompositeSections[I].GetTime();
			Bar.DrawColour = FLinearColor(0.f,1.f,0.f);
			MarkerBars.Add(Bar);
		}
	}
	return MarkerBars;
}

TArray<float> SMontageEditor::GetAnimSegmentStartTimes() const
{
	TArray<float>	Times;
	if (MontageObj != NULL)
	{
		for ( int32 i=0; i < MontageObj->SlotAnimTracks.Num(); i++)
		{
			for (int32 j=0; j < MontageObj->SlotAnimTracks[i].AnimTrack.AnimSegments.Num(); j++)
			{
				Times.Add( MontageObj->SlotAnimTracks[i].AnimTrack.AnimSegments[j].StartPos );
			}
		}
	}
	return Times;
}

void SMontageEditor::OnEditSectionTime( int32 SectionIndex, float NewTime)
{
	if (MontageObj != NULL && MontageObj->CompositeSections.IsValidIndex(SectionIndex))
	{
		if(!bDragging)
		{
			//If this is the first drag event
			const FScopedTransaction Transaction( LOCTEXT("EditSection", "Edit Section Start Time") );
			MontageObj->Modify();
		}
		bDragging = true;

		MontageObj->CompositeSections[SectionIndex].SetTime(NewTime);
		MontageObj->CompositeSections[SectionIndex].LinkMontage(MontageObj, NewTime);
	}

	AnimMontagePanel->RefreshTimingNodes();
}
void SMontageEditor::OnEditSectionTimeFinish( int32 SectionIndex )
{
	bDragging = false;

	if(MontageObj!=NULL)
	{
		SortSections();
		RefreshNotifyTriggerOffsets();
		MontageObj->MarkPackageDirty();
		AnimMontageSectionsPanel->Update();
	}

	TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
	if(SharedPersona.IsValid())
	{
		SharedPersona->OnSectionsChanged.Broadcast();
	}
}

void SMontageEditor::PreAnimUpdate()
{
	MontageObj->Modify();
}

void SMontageEditor::PostAnimUpdate()
{
	MontageObj->MarkPackageDirty();
	SortAndUpdateMontage();
}

void SMontageEditor::RebuildMontagePanel()
{
	SortAndUpdateMontage();
	AnimMontageSectionsPanel->Update();
}

EActiveTimerReturnType SMontageEditor::TriggerRebuildMontagePanel(double InCurrentTime, float InDeltaTime)
{
	RebuildMontagePanel();

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SMontageEditor::OnMontageChange(class UObject *EditorAnimBaseObj, bool Rebuild)
{
	bDragging = false;

	if ( MontageObj != nullptr )
	{
		float PreviouewSeqLength = GetSequenceLength();

		if(Rebuild && !bIsActiveTimerRegistered)
		{
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SMontageEditor::TriggerRebuildMontagePanel));
		} 
		else
		{
			CollapseMontage();
		}
		
		MontageObj->MarkPackageDirty();

		// if animation length changed, we might be out of range, let's restart
		if (GetSequenceLength() != PreviouewSeqLength)
		{
			// this might not be safe
			RestartPreview();
		}
	}
}

void SMontageEditor::InitDetailsViewEditorObject(UEditorAnimBaseObj* EdObj)
{
	EdObj->InitFromAnim(MontageObj, FOnAnimObjectChange::CreateSP( SharedThis(this), &SMontageEditor::OnMontageChange ));
}

/** This will remove empty spaces in the montage's anim segment but not resort. e.g. - all cached indexes remains valid. UI IS NOT REBUILT after this */
void SMontageEditor::CollapseMontage()
{
	if (MontageObj==NULL)
	{
		return;
	}

	for (int32 i=0; i < MontageObj->SlotAnimTracks.Num(); i++)
	{
		MontageObj->SlotAnimTracks[i].AnimTrack.CollapseAnimSegments();
	}

	MontageObj->UpdateLinkableElements();

	RecalculateSequenceLength();
}

/** This will sort all components of the montage and update (recreate) the UI */
void SMontageEditor::SortAndUpdateMontage()
{
	if (MontageObj==NULL)
	{
		return;
	}
	
	SortAnimSegments();

	MontageObj->UpdateLinkableElements();

	RecalculateSequenceLength();

	SortSections();

	RefreshNotifyTriggerOffsets();

	// Update view (this will recreate everything)
	AnimMontagePanel->Update();
	AnimMontageSectionsPanel->Update();
	AnimTimingPanel->Update();

	// Restart the preview instance of the montage
	RestartPreview();
}

float SMontageEditor::CalculateSequenceLengthOfEditorObject() const
{
	return MontageObj->CalculateSequenceLength();
}

/** Sort Segments by starting time */
void SMontageEditor::SortAnimSegments()
{
	for (int32 I=0; I < MontageObj->SlotAnimTracks.Num(); I++)
	{
		MontageObj->SlotAnimTracks[I].AnimTrack.SortAnimSegments();
	}
}

/** Sort Composite Sections by Start TIme */
void SMontageEditor::SortSections()
{
	struct FCompareSections
	{
		bool operator()( const FCompositeSection &A, const FCompositeSection &B ) const
		{
			return A.GetTime() < B.GetTime();
		}
	};
	if (MontageObj != NULL)
	{
		MontageObj->CompositeSections.Sort(FCompareSections());
	}

	EnsureStartingSection();
}

/** Ensure there is at least one section in the montage and that the first section starts at T=0.f */
void SMontageEditor::EnsureStartingSection()
{
	if(MontageObj->CompositeSections.Num() <= 0)
	{
		FCompositeSection NewSection;
		NewSection.SetTime(0.0f);
		NewSection.SectionName = FName(TEXT("Default"));
		MontageObj->CompositeSections.Add(NewSection);
		MontageObj->MarkPackageDirty();
	}

	check(MontageObj->CompositeSections.Num() > 0);
	if(MontageObj->CompositeSections[0].GetTime() > 0.0f)
	{
		MontageObj->CompositeSections[0].SetTime(0.0f);
		MontageObj->MarkPackageDirty();
	}
}

/** Esnure there is at least one slot node track */
void SMontageEditor::EnsureSlotNode()
{
	if (MontageObj && MontageObj->SlotAnimTracks.Num()==0)
	{
		AddNewMontageSlot(FAnimSlotGroup::DefaultSlotName.ToString());
		MontageObj->MarkPackageDirty();
	}
}

/** Make sure all Sections and Notifies are clamped to NewEndTime (called before NewEndTime is set to SequenceLength) */
bool SMontageEditor::ClampToEndTime(float NewEndTime)
{
	bool bClampingNeeded = SAnimEditorBase::ClampToEndTime(NewEndTime);
	if(bClampingNeeded)
	{
		float ratio = NewEndTime / MontageObj->SequenceLength;

		for(int32 i=0; i < MontageObj->CompositeSections.Num(); i++)
		{
			if(MontageObj->CompositeSections[i].GetTime() > NewEndTime)
			{
				float CurrentTime = MontageObj->CompositeSections[i].GetTime();
				MontageObj->CompositeSections[i].SetTime(CurrentTime * ratio);
			}
		}

		for(int32 i=0; i < MontageObj->Notifies.Num(); i++)
		{
			FAnimNotifyEvent& Notify = MontageObj->Notifies[i];
			float NotifyTime = Notify.GetTime();

			if(NotifyTime >= NewEndTime)
			{
				Notify.SetTime(NotifyTime * ratio);
				Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(MontageObj->CalculateOffsetForNotify(Notify.GetTime()));
			}
		}
	}

	return bClampingNeeded;
}

void SMontageEditor::AddNewSection(float StartTime, FString SectionName)
{
	if ( MontageObj != nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT("AddNewSection", "Add New Section") );
		MontageObj->Modify();

		if (MontageObj->AddAnimCompositeSection(FName(*SectionName), StartTime) != INDEX_NONE)
		{
			RebuildMontagePanel();
		}
	}
}

void SMontageEditor::RemoveSection(int32 SectionIndex)
{
	if(ValidSection(SectionIndex))
	{
		const FScopedTransaction Transaction( LOCTEXT("DeleteSection", "Delete Section") );
		MontageObj->Modify();

		MontageObj->CompositeSections.RemoveAt(SectionIndex);
		EnsureStartingSection();
		MontageObj->MarkPackageDirty();
		AnimMontageSectionsPanel->Update();
		AnimTimingPanel->Update();
		RestartPreview();
	}
}

FString	SMontageEditor::GetSectionName(int32 SectionIndex) const
{
	if (ValidSection(SectionIndex))
	{
		return MontageObj->GetSectionName(SectionIndex).ToString();
	}
	return FString();
}

void SMontageEditor::RenameSlotNode(int32 SlotIndex, FString NewSlotName)
{
	if(MontageObj->SlotAnimTracks.IsValidIndex(SlotIndex))
	{
		FName NewName(*NewSlotName);
		if(MontageObj->SlotAnimTracks[SlotIndex].SlotName != NewName)
		{
			const FScopedTransaction Transaction( LOCTEXT("RenameSlot", "Rename Slot") );
			MontageObj->Modify();

			MontageObj->SlotAnimTracks[SlotIndex].SlotName = NewName;
			MontageObj->MarkPackageDirty();
		}
	}
}

void SMontageEditor::AddNewMontageSlot( FString NewSlotName )
{
	if ( MontageObj != nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT("AddSlot", "Add Slot") );
		MontageObj->Modify();

		FSlotAnimationTrack NewTrack;
		NewTrack.SlotName = FName(*NewSlotName);
		MontageObj->SlotAnimTracks.Add( NewTrack );
		MontageObj->MarkPackageDirty();

		AnimMontagePanel->Update();
	}
}

FText SMontageEditor::GetMontageSlotName(int32 SlotIndex) const
{
	if(MontageObj->SlotAnimTracks.IsValidIndex(SlotIndex) && MontageObj->SlotAnimTracks[SlotIndex].SlotName != NAME_None)
	{
		return FText::FromName( MontageObj->SlotAnimTracks[SlotIndex].SlotName );
	}	
	return FText::GetEmpty();
}

void SMontageEditor::RemoveMontageSlot(int32 AnimSlotIndex)
{
	if ( MontageObj != nullptr && MontageObj->SlotAnimTracks.IsValidIndex( AnimSlotIndex ) )
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveSlot", "Remove Slot") );
		MontageObj->Modify();

		MontageObj->SlotAnimTracks.RemoveAt(AnimSlotIndex);
		MontageObj->MarkPackageDirty();
		AnimMontagePanel->Update();

		// Iterate the notifies and relink anything that is now invalid
		for(FAnimNotifyEvent& Event : MontageObj->Notifies)
		{
			Event.ConditionalRelink();
		}

		// Do the same for sections
		for(FCompositeSection& Section : MontageObj->CompositeSections)
		{
			Section.ConditionalRelink();
		}
	}
}

void SMontageEditor::DuplicateMontageSlot(int32 AnimSlotIndex)
{
	if (MontageObj != nullptr && MontageObj->SlotAnimTracks.IsValidIndex(AnimSlotIndex))
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicateSlot", "Duplicate Slot"));
		MontageObj->Modify();

		FSlotAnimationTrack NewTrack; 
		NewTrack.SlotName = FAnimSlotGroup::DefaultSlotName;

		NewTrack.AnimTrack = MontageObj->SlotAnimTracks[AnimSlotIndex].AnimTrack;

		MontageObj->SlotAnimTracks.Add(NewTrack);
		MontageObj->MarkPackageDirty();

		AnimMontagePanel->Update();
	}
}

void SMontageEditor::ShowSectionInDetailsView(int32 SectionIndex)
{
	UEditorCompositeSection *Obj = Cast<UEditorCompositeSection>(ShowInDetailsView(UEditorCompositeSection::StaticClass()));
	if ( Obj != nullptr )
	{
		Obj->InitSection(SectionIndex);
	}
	RestartPreviewFromSection(SectionIndex);
}

void SMontageEditor::RestartPreview()
{
	UAnimPreviewInstance * Preview = Cast<UAnimPreviewInstance>(PersonaPtr.Pin()->GetPreviewMeshComponent() ? PersonaPtr.Pin()->GetPreviewMeshComponent()->PreviewInstance:NULL);
	if (Preview)
	{
		Preview->MontagePreview_PreviewNormal(INDEX_NONE, Preview->IsPlaying());
	}
}

void SMontageEditor::RestartPreviewFromSection(int32 FromSectionIdx)
{
	UAnimPreviewInstance * Preview = Cast<UAnimPreviewInstance>(PersonaPtr.Pin()->GetPreviewMeshComponent() ? PersonaPtr.Pin()->GetPreviewMeshComponent()->PreviewInstance:NULL);
	if(Preview)
	{
		Preview->MontagePreview_PreviewNormal(FromSectionIdx, Preview->IsPlaying());
	}
}

void SMontageEditor::RestartPreviewPlayAllSections()
{
	UAnimPreviewInstance * Preview = Cast<UAnimPreviewInstance>(PersonaPtr.Pin()->GetPreviewMeshComponent() ? PersonaPtr.Pin()->GetPreviewMeshComponent()->PreviewInstance:NULL);
	if(Preview)
	{
		Preview->MontagePreview_PreviewAllSections(Preview->IsPlaying());
	}
}

void SMontageEditor::MakeDefaultSequentialSections()
{
	check( MontageObj != nullptr );
	SortSections();
	for(int32 SectionIdx=0; SectionIdx < MontageObj->CompositeSections.Num(); SectionIdx++)
	{
		MontageObj->CompositeSections[SectionIdx].NextSectionName = MontageObj->CompositeSections.IsValidIndex(SectionIdx+1) ? MontageObj->CompositeSections[SectionIdx+1].SectionName : NAME_None;
	}
	RestartPreview();
}

void SMontageEditor::ClearSquenceOrdering()
{
	check( MontageObj != nullptr );
	SortSections();
	for(int32 SectionIdx=0; SectionIdx < MontageObj->CompositeSections.Num(); SectionIdx++)
	{
		MontageObj->CompositeSections[SectionIdx].NextSectionName = NAME_None;
	}
	RestartPreview();
}

void SMontageEditor::PostUndo()
{
	// when undo or redo happens, we still have to recalculate length, so we can't rely on sequence length changes or not
	if (MontageObj->SequenceLength)
	{
		MontageObj->SequenceLength = 0.f;
	}

	RebuildMontagePanel(); //Rebuild here, undoing adds can cause slate to crash later on if we don't (using dummy args since they aren't used by the method
}
#undef LOCTEXT_NAMESPACE


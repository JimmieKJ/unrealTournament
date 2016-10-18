// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SAnimEditorBase.h"

class SAnimMontagePanel;
class SAnimNotifyPanel;
class SAnimCurvePanel;
class SAnimMontageSectionsPanel;
class SAnimMontageScrubPanel;
class SAnimTimingPanel;

//////////////////////////////////////////////////////////////////////////
// SMontageEditor

/** Overall animation montage editing widget. This mostly contains functions for editing the UAnimMontage.

	SMontageEditor will create the SAnimMontagePanel which is mostly responsible for setting up the UI 
	portion of the Montage tool and registering callbacks to the SMontageEditor to do the actual editing.
	
*/
class SMontageEditor : public SAnimEditorBase 
{
public:
	SLATE_BEGIN_ARGS( SMontageEditor )
		: _Persona()
		, _Montage(NULL)
		{}

		SLATE_ARGUMENT( TSharedPtr<FPersona>, Persona )
		SLATE_ARGUMENT( UAnimMontage*, Montage )
	SLATE_END_ARGS()

	~SMontageEditor();

private:
	/** Persona reference **/
	TSharedPtr<SAnimMontagePanel> AnimMontagePanel;
	TSharedPtr<SAnimNotifyPanel> AnimNotifyPanel;
	TSharedPtr<SAnimCurvePanel>	AnimCurvePanel;
	TSharedPtr<SAnimMontageSectionsPanel> AnimMontageSectionsPanel;
	TSharedPtr<SAnimMontageScrubPanel> AnimMontageScrubPanel;
	TSharedPtr<SAnimTimingPanel> AnimTimingPanel;

	TWeakPtr<FPersona> WeakPersona;
protected:
	//~ Begin SAnimEditorBase Interface
	virtual TSharedRef<class SAnimationScrubPanel> ConstructAnimScrubPanel() override;
	//~ End SAnimEditorBase Interface

public:
	void Construct(const FArguments& InArgs);
	void SetMontageObj(UAnimMontage * NewMontage);
	UAnimMontage * GetMontageObj() const { return MontageObj; }

	virtual UAnimationAsset* GetEditorObject() const override { return GetMontageObj(); }

	void RestartPreview();
	void RestartPreviewFromSection(int32 FromSectionIdx = INDEX_NONE);
	void RestartPreviewPlayAllSections();

private:
	/** Pointer to the animation sequence being edited */
	UAnimMontage* MontageObj;

	/** If previewing section, it is section used to restart previewing when play button is pushed */
	int32 PreviewingStartSectionIdx;

	/** If currently previewing all or selected section */
	bool bPreviewingAllSections : 1;

	/** If currently previewing tracks instead of sections */
	bool bPreviewingTracks : 1;

	/** If user is currently dragging an item */
	bool bDragging : 1;

	/** If the active timer to trigger a montage panel rebuild is currently registered */
	bool bIsActiveTimerRegistered : 1;

	virtual float CalculateSequenceLengthOfEditorObject() const override;
	void SortAndUpdateMontage();
	void CollapseMontage();
	void SortAnimSegments();
	void SortSections();
	void EnsureStartingSection();
	void EnsureSlotNode();
	virtual bool ClampToEndTime(float NewEndTime) override;
	void PostUndo();
	
	bool GetSectionTime( int32 SectionIndex, float &OutTime ) const;

	bool ValidIndexes(int32 AnimSlotIndex, int32 AnimSegmentIndex) const;
	bool ValidSection(int32 SectionIndex) const;

	/** Updates Notify trigger offsets to take into account current montage state */
	void RefreshNotifyTriggerOffsets();

	/** One-off active timer to trigger a montage panel rebuild */
	EActiveTimerReturnType TriggerRebuildMontagePanel(double InCurrentTime, float InDeltaTime);

	/** Rebuilds the montage panel */
	void RebuildMontagePanel();

protected:
	virtual void InitDetailsViewEditorObject(class UEditorAnimBaseObj* EdObj) override;

public:

	/** These are meant to be callbacks used by the montage editing UI widgets */

	void					OnMontageChange(class UObject *EditorAnimBaseObj, bool Rebuild);
	void					ShowSectionInDetailsView(int32 SectionIdx);

	TArray<float>			GetSectionStartTimes() const;
	TArray<FTrackMarkerBar>	GetMarkerBarInformation() const;
	TArray<FString>			GetSectionNames() const;
	TArray<float>			GetAnimSegmentStartTimes() const;
	void					OnEditSectionTime( int32 SectionIndex, float NewTime);
	void					OnEditSectionTimeFinish( int32 SectionIndex);

	void					AddNewSection(float StartTime, FString SectionName);
	void					RemoveSection(int32 SectionIndex);

	FString					GetSectionName(int32 SectionIndex) const;
	void					RenameSlotNode(int32 SlotIndex, FString NewSlotName);

	void					AddNewMontageSlot(FString NewSlotName);
	void					RemoveMontageSlot(int32 AnimSlotIndex);
	void					DuplicateMontageSlot(int32 AnimSlotIndex);
	FText					GetMontageSlotName(int32 SlotIndex) const;

	void					MakeDefaultSequentialSections();
	void					ClearSquenceOrdering();

	/** Delegete handlers for when the editor UI is changing the montage */
	void			PreAnimUpdate();
	void			PostAnimUpdate();

	//~ Begin SAnimEditorBase Interface
	virtual TSharedRef<SWidget> CreateDocumentAnchor() override;
	//~ End SAnimEditorBase Interface
};

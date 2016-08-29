// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SAnimEditorBase.h"
#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "Editor/Kismet/Public/SKismetInspector.h"
#include "SAnimationScrubPanel.h"
#include "SAnimNotifyPanel.h"
#include "SAnimCurvePanel.h"
#include "AnimPreviewInstance.h"

#define LOCTEXT_NAMESPACE "AnimEditorBase"

//////////////////////////////////////////////////////////////////////////
// SAnimEditorBase

TSharedRef<SWidget> SAnimEditorBase::CreateDocumentAnchor()
{
	return IDocumentation::Get()->CreateAnchor(TEXT("Engine/Animation/Sequences"));
}

void SAnimEditorBase::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;

	SetInputViewRange(0, GetSequenceLength());

	TSharedPtr<SVerticalBox> AnimVerticalBox;

	this->ChildSlot
	[
		SAssignNew(AnimVerticalBox, SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Header, shows name of timeline we are editing
			SNew(SBorder)
			. BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
			. HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				. VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14 ) )
					.ColorAndOpacity( FLinearColor(1,1,1,0.5) )
					.Text( this, &SAnimEditorBase::GetEditorObjectName )
				]
				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						CreateDocumentAnchor()
					]
			]
		]

		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				SNew( SScrollBox )
				+SScrollBox::Slot() 
				[
					SAssignNew (EditorPanels, SVerticalBox)
				]
			]
		]
	];

	// If we want to create anim info bar, display that now
	if (InArgs._DisplayAnimInfoBar)
	{
		AnimVerticalBox->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			// this is *temporary* information to display stuff
			SNew( SBorder )
			.Padding(FMargin(4))
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("Animation", "Animation : "))
					]

					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetEditorObjectName)
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("Percentage", "Percentage: "))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetCurrentPercentage)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("CurrentTime", "CurrentTime: "))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetCurrentSequenceTime)
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(LOCTEXT("CurrentFrame", "Current Frame: "))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(FMargin(4,4,0,0))
					[
						SNew( STextBlock )
						.Text(this, &SAnimEditorBase::GetCurrentFrame)
					]
				]
			]
		];

	}

	// If we are an anim sequence, add scrub panel as well
	UAnimSequenceBase* SeqBase = Cast<UAnimSequenceBase>(GetEditorObject());
	if (SeqBase)
	{
		AnimVerticalBox->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Bottom)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				ConstructAnimScrubPanel()
			]
		];
	}

}

TSharedRef<class SAnimationScrubPanel> SAnimEditorBase::ConstructAnimScrubPanel()
{
	UAnimSequenceBase* AnimSeqBase = Cast<UAnimSequenceBase>(GetEditorObject());
	check(AnimSeqBase);

	return SAssignNew(AnimScrubPanel, SAnimationScrubPanel)
		.Persona(PersonaPtr)
		.LockedSequence(AnimSeqBase)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.bAllowZoom(true);
}

void SAnimEditorBase::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

UObject* SAnimEditorBase::ShowInDetailsView( UClass* EdClass )
{
	check(GetEditorObject()!=NULL);

	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		if(Obj->IsA(UEditorAnimBaseObj::StaticClass()))
		{
			UEditorAnimBaseObj *EdObj = Cast<UEditorAnimBaseObj>(Obj);
			InitDetailsViewEditorObject(EdObj);
			PersonaPtr.Pin()->SetDetailObject(EdObj);
		}
	}
	return Obj;
}

void SAnimEditorBase::ClearDetailsView()
{
	PersonaPtr.Pin()->SetDetailObject(NULL);
}

FText SAnimEditorBase::GetEditorObjectName() const
{
	if (GetEditorObject() != NULL)
	{
		return FText::FromString(GetEditorObject()->GetName());
	}
	else
	{
		return LOCTEXT("NoEditorObject", "No Editor Object");
	}
}

void SAnimEditorBase::RecalculateSequenceLength()
{
	// Remove Gaps and update Montage Sequence Length
	if(UAnimCompositeBase* Composite = Cast<UAnimCompositeBase>(GetEditorObject()))
	{
		Composite->InvalidateRecursiveAsset();

		float NewSequenceLength = CalculateSequenceLengthOfEditorObject();
		if (NewSequenceLength != GetSequenceLength())
		{
			ClampToEndTime(NewSequenceLength);

			Composite->SetSequenceLength(NewSequenceLength);

			// Reset view if we changed length (note: has to be done after ->SetSequenceLength)!
			SetInputViewRange(0.f, NewSequenceLength);

			UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
			if (PreviewInstance)
			{
				// Re-set the position, so instance is clamped properly
				PreviewInstance->SetPosition(PreviewInstance->GetCurrentTime(), false); 
			}
		}
	}

	if(UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(GetEditorObject()))
	{
		Sequence->ClampNotifiesAtEndOfSequence();
	}
}

bool SAnimEditorBase::ClampToEndTime(float NewEndTime)
{
	float SequenceLength = GetSequenceLength();

	//if we had a valid sequence length before and our new end time is shorter
	//then we need to clamp.
	return (SequenceLength > 0.f && NewEndTime < SequenceLength);
}

void SAnimEditorBase::OnSelectionChanged(const FGraphPanelSelectionSet& SelectedItems)
{
	if (SelectedItems.Num() == 0)
	{
		// If nothing is selected, unselect it instead of selecting current editor object. 
		// That means, it shows same information as Anim Detail Panel
		PersonaPtr.Pin()->UpdateSelectionDetails(NULL, LOCTEXT("Edit Sequence", "Edit Sequence"));
	}
	else
	{
		// Edit selected notifications
		PersonaPtr.Pin()->FocusInspectorOnGraphSelection(SelectedItems);
	}
}

class UAnimSingleNodeInstance* SAnimEditorBase::GetPreviewInstance() const
{
	return (PersonaPtr.Pin()->GetPreviewMeshComponent())? PersonaPtr.Pin()->GetPreviewMeshComponent()->PreviewInstance:NULL;
}

float SAnimEditorBase::GetScrubValue() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		float CurTime = PreviewInstance->GetCurrentTime();
		return (CurTime); 
	}
	else
	{
		return 0.f;
	}
}

void SAnimEditorBase::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMaxInput = FMath::Min<float>(InViewMaxInput, GetSequenceLength());
	ViewMinInput = FMath::Max<float>(InViewMinInput, 0.f);
}

FText SAnimEditorBase::GetCurrentSequenceTime() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	float CurTime = 0.f;
	float TotalTime = GetSequenceLength();

	if (PreviewInstance)
	{
		CurTime = PreviewInstance->GetCurrentTime();
	}

	static const FNumberFormattingOptions FractionNumberFormat = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(3)
		.SetMaximumFractionalDigits(3);
	return FText::Format(LOCTEXT("FractionSecondsFmt", "{0} / {1} (second(s))"), FText::AsNumber(CurTime, &FractionNumberFormat), FText::AsNumber(TotalTime, &FractionNumberFormat));
}

float SAnimEditorBase::GetPercentageInternal() const
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	float Percentage = 0.f;
	if (PreviewInstance)
	{
		float SequenceLength = GetSequenceLength();
		if (SequenceLength > 0.f)
		{
			Percentage = PreviewInstance->GetCurrentTime() / SequenceLength;
		}
		else
		{
			Percentage = 0.f;
		}
	}

	return Percentage;
}

FText SAnimEditorBase::GetCurrentPercentage() const
{
	float Percentage = GetPercentageInternal();

	static const FNumberFormattingOptions PercentNumberFormat = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::AsPercent(Percentage, &PercentNumberFormat);
}

FText SAnimEditorBase::GetCurrentFrame() const
{
	float Percentage = GetPercentageInternal();
	float NumFrames = 0;
	
	if (UAnimSequenceBase* AnimSeqBase = Cast<UAnimSequenceBase>(GetEditorObject()))
	{
		NumFrames = AnimSeqBase->GetNumberOfFrames();
	}

	static const FNumberFormattingOptions FractionNumberFormat = FNumberFormattingOptions()
		.SetMinimumFractionalDigits(2)
		.SetMaximumFractionalDigits(2);
	return FText::Format(LOCTEXT("FractionKeysFmt", "{0} / {1} (key(s))"), FText::AsNumber(NumFrames * Percentage, &FractionNumberFormat), FText::AsNumber((int32)NumFrames));
}

float SAnimEditorBase::GetSequenceLength() const
{
	UAnimSequenceBase* AnimSeqBase = Cast<UAnimSequenceBase>(GetEditorObject());
	
	if (AnimSeqBase)
	{
		return AnimSeqBase->SequenceLength;
	}
	
	return 0.f;
}
#undef LOCTEXT_NAMESPACE

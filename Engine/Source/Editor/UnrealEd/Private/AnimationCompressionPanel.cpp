// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Animation/AnimCompress.h"
#include "AnimationCompressionPanel.h"

#define LOCTEXT_NAMESPACE "AnimationCompression"

//////////////////////////////////////////////
//	FDlgAnimCompression

FDlgAnimCompression::FDlgAnimCompression( TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences )
{
	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title( LOCTEXT("AnimCompression", "Animation Compression") )
			.SupportsMinimize(false) .SupportsMaximize(false)
			.SizingRule( ESizingRule::Autosized );

		TSharedPtr<SBorder> DialogWrapper = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SAnimationCompressionPanel)
				.AnimSequences(AnimSequences)
				.ParentWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

void FDlgAnimCompression::ShowModal()
{
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
}

void SAnimationCompressionPanel::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow.Get();

	AnimSequences = InArgs._AnimSequences;
	CurrentCompressionChoice = 0;

	for ( TObjectIterator<UClass> It ; It ; ++It )
	{
		UClass* Class = *It;
		if( !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) )
		{
			if ( Class->IsChildOf(UAnimCompress::StaticClass()) )
			{
				UAnimCompress* NewAlgorithm = NewObject<UAnimCompress>(GetTransientPackage(), Class);
				AnimationCompressionAlgorithms.Add( NewAlgorithm );
			}
		}
	}

	TSharedRef<SVerticalBox> Box =
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew( STextBlock )
			.Text( LOCTEXT("AnimCompressionLabel", "Compression Algorithms") )
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SNew(SSeparator)
		];

	for( int32 i = 0 ; i < AnimationCompressionAlgorithms.Num() ; ++i )
	{
		UAnimCompress* Alg = AnimationCompressionAlgorithms[i];
		(*Box).AddSlot()
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			CreateRadioButton( FText::FromString( Alg->Description ), i)
		];
	}

	(*Box)	.AddSlot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 4.0f, 8.0f)
			[
				SNew(SSeparator)
			];
	(*Box)	.AddSlot()
			.Padding(4.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.AutoHeight()
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("AnimCompressionApply", "Apply"))
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SAnimationCompressionPanel::ApplyClicked)
				]
			];
	
	this->ChildSlot[Box];
}

SAnimationCompressionPanel::~SAnimationCompressionPanel()
{

}

FReply SAnimationCompressionPanel::ApplyClicked()
{
	UAnimCompress* CompressionAlgorithm = AnimationCompressionAlgorithms[CurrentCompressionChoice];
	ApplyAlgorithm( CompressionAlgorithm );
	return FReply::Handled();
}

void SAnimationCompressionPanel::ApplyAlgorithm(class UAnimCompress* Algorithm)
{
	if ( Algorithm )
	{
		const bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo,
			FText::Format( NSLOCTEXT("UnrealEd", "AboutToCompressAnimations_F", "About to compress {0} animations.  Proceed?"), FText::AsNumber(AnimSequences.Num()) ) );
		if ( bProceed )
		{
			TArray<UAnimSequence*> AnimSequencePtrs;
			AnimSequencePtrs.Reserve(AnimSequences.Num());

			for(int32 Index = 0; Index < AnimSequences.Num(); ++Index)
			{
				AnimSequencePtrs.Add(AnimSequences[Index].Get());
			}

			GWarn->BeginSlowTask( LOCTEXT("AnimCompressing", "Compressing"), true);
			
			Algorithm->Reduce(AnimSequencePtrs, true);

			GWarn->EndSlowTask( );

			ParentWindow->RequestDestroyWindow();
		}
	}
}
#undef LOCTEXT_NAMESPACE
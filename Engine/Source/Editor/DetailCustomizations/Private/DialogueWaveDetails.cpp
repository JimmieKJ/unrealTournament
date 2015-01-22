// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SoundDefinitions.h"
#include "DialogueWaveDetails.h"
#include "DialogueWaveWidgets.h"
#include "Sound/DialogueWave.h"

#define LOCTEXT_NAMESPACE "DialogueWaveDetails"

FDialogueContextMappingNodeBuilder::FDialogueContextMappingNodeBuilder( IDetailLayoutBuilder* InDetailLayoutBuilder, const TSharedPtr<IPropertyHandle>& InPropertyHandle )
	: DetailLayoutBuilder( InDetailLayoutBuilder ), ContextMappingPropertyHandle( InPropertyHandle ), DisplayedTargetCount(0)
{

}

void FDialogueContextMappingNodeBuilder::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	if( ContextMappingPropertyHandle->IsValidHandle() )
	{
		const TSharedPtr<IPropertyHandle> ContextPropertyHandle = ContextMappingPropertyHandle->GetChildHandle("Context");
		if( ContextPropertyHandle->IsValidHandle() )
		{
			const TSharedPtr<IPropertyHandle> SpeakerPropertyHandle = ContextPropertyHandle->GetChildHandle("Speaker");
			const TSharedPtr<IPropertyHandle> TargetsPropertyHandle = ContextPropertyHandle->GetChildHandle("Targets");

			const TSharedPtr<IPropertyHandle> ParentHandle = ContextMappingPropertyHandle->GetParentHandle();
			const TSharedPtr<IPropertyHandleArray> ParentArrayHandle = ParentHandle->AsArray();

			uint32 ContextCount;
			ParentArrayHandle->GetNumElements(ContextCount);

			TSharedRef<SWidget> ClearButton = PropertyCustomizationHelpers::MakeDeleteButton( FSimpleDelegate::CreateSP( this, &FDialogueContextMappingNodeBuilder::RemoveContextButton_OnClick), 
				ContextCount > 1 ? LOCTEXT("RemoveContextToolTip", "Remove context.") : LOCTEXT("RemoveContextDisabledToolTip", "Cannot remove context - a dialogue wave must have at least one context."),
				ContextCount > 1 );

			NodeRow
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SBorder )
					.BorderImage( FEditorStyle::GetBrush("DialogueWaveDetails.HeaderBorder") )
					[
						SNew( SDialogueContextHeaderWidget, ContextPropertyHandle.ToSharedRef(), DetailLayoutBuilder->GetThumbnailPool().ToSharedRef() )
					]
				]
				+SHorizontalBox::Slot()
				.Padding(2.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					ClearButton
				]
			];
		}
	}
}

void FDialogueContextMappingNodeBuilder::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	if( ContextMappingPropertyHandle->IsValidHandle() )
	{
		const TSharedPtr<IPropertyHandle> SoundwavePropertyHandle = ContextMappingPropertyHandle->GetChildHandle("Soundwave");
		ChildrenBuilder.AddChildProperty(SoundwavePropertyHandle.ToSharedRef());
	}
}

void FDialogueContextMappingNodeBuilder::RemoveContextButton_OnClick()
{
	if( ContextMappingPropertyHandle->IsValidHandle() )
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = ContextMappingPropertyHandle->GetParentHandle();
		const TSharedPtr<IPropertyHandleArray> ParentArrayHandle = ParentHandle->AsArray();

		uint32 ContextCount;
		ParentArrayHandle->GetNumElements(ContextCount);
		if( ContextCount != 1 ) // Mustn't remove the only context.
		{
			ParentArrayHandle->DeleteItem( ContextMappingPropertyHandle->GetIndexInArray() );
			DetailLayoutBuilder->ForceRefreshDetails();
		}
	}
}

TSharedRef<IDetailCustomization> FDialogueWaveDetails::MakeInstance()
{
	return MakeShareable( new FDialogueWaveDetails );
}

void FDialogueWaveDetails::CustomizeDetails( class IDetailLayoutBuilder& DetailBuilder )
{
	DetailLayoutBuilder = &DetailBuilder;

	IDetailCategoryBuilder& ContextMappingsDetailCategoryBuilder = DetailBuilder.EditCategory("DialogueContexts");

	// Add Context Button
	ContextMappingsDetailCategoryBuilder.AddCustomRow( LOCTEXT("AddDialogueContext", "Add Dialogue Context") )
	[
		SNew( SBox )
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew( SButton )
			.Text( LOCTEXT("AddDialogueContext", "Add Dialogue Context") )
			.ToolTipText( LOCTEXT("AddDialogueContextToolTip", "Adds a new context for dialogue based on speakers, those spoken to, and the associated soundwave.") )
			.OnClicked( FOnClicked::CreateSP( this, &FDialogueWaveDetails::AddDialogueContextMapping_OnClicked ) )
		]
	];

	// Individual Context Mappings
	const TSharedPtr<IPropertyHandle> ContextMappingsPropertyHandle = DetailLayoutBuilder->GetProperty("ContextMappings", UDialogueWave::StaticClass());
	ContextMappingsPropertyHandle->MarkHiddenByCustomization();

	const TSharedPtr<IPropertyHandleArray> ContextMappingsPropertyArrayHandle = ContextMappingsPropertyHandle->AsArray();

	uint32 DialogueContextMappingCount;
	ContextMappingsPropertyArrayHandle->GetNumElements( DialogueContextMappingCount );
	for(uint32 j = 0; j < DialogueContextMappingCount; ++j)
	{
		const TSharedPtr<IPropertyHandle> ChildContextMappingPropertyHandle = ContextMappingsPropertyArrayHandle->GetElement(j);

		const TSharedRef<FDialogueContextMappingNodeBuilder> DialogueContextMapping = MakeShareable( new FDialogueContextMappingNodeBuilder( DetailLayoutBuilder, ChildContextMappingPropertyHandle ) );
		ContextMappingsDetailCategoryBuilder.AddCustomBuilder(DialogueContextMapping);
	}
}

FReply FDialogueWaveDetails::AddDialogueContextMapping_OnClicked()
{
	const TSharedPtr<IPropertyHandle> ContextMappingsPropertyHandle = DetailLayoutBuilder->GetProperty("ContextMappings", UDialogueWave::StaticClass());
	const TSharedPtr<IPropertyHandleArray> ContextMappingsPropertyArrayHandle = ContextMappingsPropertyHandle->AsArray();
	ContextMappingsPropertyArrayHandle->AddItem();

	DetailLayoutBuilder->ForceRefreshDetails();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
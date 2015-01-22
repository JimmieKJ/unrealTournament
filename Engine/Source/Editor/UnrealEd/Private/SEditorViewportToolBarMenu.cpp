// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "SEditorViewportToolBarMenu.h"
#include "SViewportToolBar.h"

void SEditorViewportToolbarMenu::Construct( const FArguments& Declaration )
{
	const TAttribute<FText>& Label = Declaration._Label;

	const FName ImageName = Declaration._Image;
	const FSlateBrush* ImageBrush = FEditorStyle::GetBrush( ImageName );

	LabelIconBrush = Declaration._LabelIcon;

	ParentToolBar = Declaration._ParentToolBar;
	checkf(ParentToolBar.IsValid(), TEXT("The parent toolbar must be specified") );

	TSharedPtr<SWidget> ButtonContent;
	
	// Create the content for the button.  We always use an image over a label if it is valid
	if( ImageName != NAME_None )
	{
		ButtonContent = 
			SNew( SImage )	
				.Image( ImageBrush )
				.ColorAndOpacity( FSlateColor::UseForeground() );
	}
	else
	{
		if( LabelIconBrush.IsBound() || LabelIconBrush.Get() != NULL )
		{
			// Label with an icon to the left
			const float MenuIconSize = 16.0f;
			ButtonContent = 
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0.0f, 0.0f, 3.0f, 0.0f )
				[
					SNew( SBox )
					.Visibility( this, &SEditorViewportToolbarMenu::GetLabelIconVisibility )
					.WidthOverride( MenuIconSize )
					.HeightOverride( MenuIconSize )
					[
						SNew( SImage )
							.Image( LabelIconBrush )
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth( 1.0f )
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
						.Font( FEditorStyle::GetFontStyle("EditorViewportToolBar.Font") )
						.Text( Label )
				];
		}
		else
		{
			// Just the label text, no icon
			ButtonContent = 
				SNew( STextBlock )
					.Font( FEditorStyle::GetFontStyle("EditorViewportToolBar.Font") )
					.Text( Label );
		}
	}

	ChildSlot
	[
		SAssignNew( MenuAnchor, SMenuAnchor )
		.Placement( MenuPlacement_BelowAnchor )
		[
			SNew( SButton )
			// Allows users to drag with the mouse to select options after opening the menu */
			.ClickMethod( EButtonClickMethod::MouseDown )
			.ContentPadding( FMargin( 5.0f, 2.0f ) )
			.VAlign( VAlign_Center )
			.ButtonStyle( FEditorStyle::Get(), "EditorViewportToolBar.MenuButton" )
			.OnClicked( this, &SEditorViewportToolbarMenu::OnMenuClicked )
			[
				ButtonContent.ToSharedRef()
			]
		]
		.OnGetMenuContent( Declaration._OnGetMenuContent )
	];
}

FReply SEditorViewportToolbarMenu::OnMenuClicked()
{
	// If the menu button is clicked toggle the state of the menu anchor which will open or close the menu
	if (MenuAnchor->ShouldOpenDueToClick())
	{
		MenuAnchor->SetIsOpen( true );
		ParentToolBar.Pin()->SetOpenMenu( MenuAnchor );
	}
	else
	{
		MenuAnchor->SetIsOpen( false );
		TSharedPtr<SMenuAnchor> NullAnchor;
		ParentToolBar.Pin()->SetOpenMenu( NullAnchor );
	}

	return FReply::Handled();
}

void SEditorViewportToolbarMenu::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// See if there is another menu on the same tool bar already open
	TWeakPtr<SMenuAnchor> OpenedMenu = ParentToolBar.Pin()->GetOpenMenu();
	if( OpenedMenu.IsValid() && OpenedMenu.Pin()->IsOpen() && OpenedMenu.Pin() != MenuAnchor )
	{
		// There is another menu open so we open this menu and close the other
		ParentToolBar.Pin()->SetOpenMenu( MenuAnchor ); 
		MenuAnchor->SetIsOpen( true );
	}

}

EVisibility SEditorViewportToolbarMenu::GetLabelIconVisibility() const
{
	return LabelIconBrush.Get() != NULL ? EVisibility::Visible : EVisibility::Collapsed;
}


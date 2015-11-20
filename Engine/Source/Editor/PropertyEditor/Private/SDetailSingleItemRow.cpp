// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SDetailSingleItemRow.h"
#include "DetailItemNode.h"
#include "PropertyEditorHelpers.h"
#include "IDetailKeyframeHandler.h"
#include "IDetailPropertyExtensionHandler.h"
#include "DetailPropertyRow.h"

namespace DetailWidgetConstants
{
	const FMargin LeftRowPadding( 0.0f, 2.5f, 2.0f, 2.5f );
	const FMargin RightRowPadding( 3.0f, 2.5f, 2.0f, 2.5f );
}

class SConstrainedBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SConstrainedBox )
		: _MinWidth()
		, _MaxWidth()
	{}
		SLATE_DEFAULT_SLOT( FArguments, Content )
		SLATE_ATTRIBUTE( TOptional<float>, MinWidth )
		SLATE_ATTRIBUTE( TOptional<float>, MaxWidth )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		MinWidth = InArgs._MinWidth;
		MaxWidth = InArgs._MaxWidth;

		ChildSlot
		[
			InArgs._Content.Widget
		];
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		const float MinWidthVal = MinWidth.Get().Get(0.0f);
		const float MaxWidthVal = MaxWidth.Get().Get(0.0f);

		if ( MinWidthVal == 0.0f && MaxWidthVal == 0.0f )
		{
			return SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
		}
		else
		{
			FVector2D ChildSize = ChildSlot.GetWidget()->GetDesiredSize();

			float XVal = FMath::Max( MinWidthVal, ChildSize.X );
			if( MaxWidthVal >= MinWidthVal )
			{
				XVal = FMath::Min( MaxWidthVal, XVal );
			}

			return FVector2D( XVal, ChildSize.Y );
		}
	}
private:
	TAttribute< TOptional<float> > MinWidth;
	TAttribute< TOptional<float> > MaxWidth;
};


void SDetailSingleItemRow::Construct( const FArguments& InArgs, FDetailLayoutCustomization* InCustomization, bool bHasMultipleColumns, TSharedRef<IDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	OwnerTreeNode = InOwnerTreeNode;

	ColumnSizeData = InArgs._ColumnSizeData;

	TSharedRef<SWidget> Widget = SNullWidget::NullWidget;
	Customization = InCustomization;

	EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
	EVerticalAlignment VerticalAlignment = VAlign_Fill;

	if( InCustomization->IsValidCustomization() )
	{
		FDetailWidgetRow Row = InCustomization->GetWidgetRow();
		TSharedPtr<FPropertyEditor> PropertyEditor;
		auto bRequiresEditConfigHierarchy = false;
		auto PropertyNode = InCustomization->GetPropertyNode();
		if (PropertyNode.IsValid() && PropertyNode->GetProperty())
		{
			auto Prop = PropertyNode->GetProperty();
			if (Prop->HasAnyPropertyFlags(CPF_GlobalConfig | CPF_Config))
			{
				if (Prop->HasMetaData(TEXT("ConfigHierarchyEditable")))
				{
					PropertyEditor = InCustomization->PropertyRow->GetPropertyEditor();
					bRequiresEditConfigHierarchy = true;
				}
			}
		}

		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
	
		NameWidget = Row.NameWidget.Widget;
		if( Row.IsEnabledAttr.IsBound() )
		{
			NameWidget->SetEnabled( Row.IsEnabledAttr );
		}
		
		ValueWidget =
			SNew( SConstrainedBox )
			.MinWidth( Row.ValueWidget.MinWidth )
			.MaxWidth( Row.ValueWidget.MaxWidth )
			[
				Row.ValueWidget.Widget
			];

		ValueWidget = CreateExtensionWidget(ValueWidget.ToSharedRef(), *Customization, InOwnerTreeNode );

		if( Row.IsEnabledAttr.IsBound() )
		{
			ValueWidget->SetEnabled( Row.IsEnabledAttr );
		}

		TSharedRef<SWidget> KeyFrameButton = CreateKeyframeButton( *Customization, InOwnerTreeNode );
		TAttribute<bool> IsPropertyEditingEnabled = InOwnerTreeNode->IsPropertyEditingEnabled();

		if( bHasMultipleColumns )
		{
			NameWidget->SetEnabled(IsPropertyEditingEnabled);
			TSharedPtr<SHorizontalBox> HBox;
			TSharedRef<SSplitter> Splitter = 
				SNew( SSplitter )
				.Style( FEditorStyle::Get(), "DetailsView.Splitter" )
				.PhysicalSplitterHandleSize( 1.0f )
				.HitDetectionSplitterHandleSize( 5.0f )
				+ SSplitter::Slot()
				.Value( ColumnSizeData.LeftColumnWidth )
				.OnSlotResized( SSplitter::FOnSlotResized::CreateSP( this, &SDetailSingleItemRow::OnLeftColumnResized ) )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.Padding( 3.0f, 0.0f )
					.HAlign( HAlign_Left )
					.VAlign( VAlign_Center )
					.AutoWidth()
					[
						SNew( SExpanderArrow, SharedThis(this) )
						.BaseIndentLevel(1)
					]
					+ SHorizontalBox::Slot()
					.HAlign( Row.NameWidget.HorizontalAlignment )
					.VAlign( Row.NameWidget.VerticalAlignment )
					.Padding( DetailWidgetConstants::LeftRowPadding )
					[
						NameWidget.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.Padding(3.0f, 0.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						KeyFrameButton
					]
				]
				+ SSplitter::Slot()
				.Value( ColumnSizeData.RightColumnWidth )
				.OnSlotResized( ColumnSizeData.OnWidthChanged )
				[
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					[
						SAssignNew(HBox, SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(DetailWidgetConstants::RightRowPadding)
						.HAlign(Row.ValueWidget.HorizontalAlignment)
						.VAlign(Row.ValueWidget.VerticalAlignment)
						[
							SNew(SBox)
							.IsEnabled(IsPropertyEditingEnabled)
							[
								ValueWidget.ToSharedRef()
							]
						]
					]
				];
				if (bRequiresEditConfigHierarchy && PropertyEditor.IsValid())
				{
					HBox->AddSlot()
					.AutoWidth()
					.Padding(0)
					//.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						PropertyCustomizationHelpers::MakeEditConfigHierarchyButton(FSimpleDelegate::CreateSP(PropertyEditor.ToSharedRef(), &FPropertyEditor::EditConfigHierarchy))
					];
				}
				Widget = Splitter;
		}
		else
		{
			Row.WholeRowWidget.Widget->SetEnabled(IsPropertyEditingEnabled);
			Widget =
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.Padding( 3.0f, 0.0f )
				.HAlign( HAlign_Left )
				.VAlign( VAlign_Center )
				.AutoWidth()
				[
					SNew( SExpanderArrow, SharedThis(this) )
					.BaseIndentLevel(1)
				]
				+ SHorizontalBox::Slot()
				.HAlign( Row.WholeRowWidget.HorizontalAlignment )
				.VAlign( Row.WholeRowWidget.VerticalAlignment )
				.Padding( DetailWidgetConstants::LeftRowPadding )
				[
					Row.WholeRowWidget.Widget
				]
				+ SHorizontalBox::Slot()
				.Padding(3.0f, 0.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					KeyFrameButton
				];
		}
	}


	this->ChildSlot
	[	
		SNew( SBorder )
		.BorderImage( this, &SDetailSingleItemRow::GetBorderImage )
		.Padding( FMargin( 0.0f, 0.0f, SDetailTableRowBase::ScrollbarPaddingSize, 0.0f ) )
		[
			Widget
		]
	];

	STableRow< TSharedPtr< IDetailTreeNode > >::ConstructInternal(
		STableRow::FArguments()
			.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
		InOwnerTableView
	);
}

bool SDetailSingleItemRow::OnContextMenuOpening( FMenuBuilder& MenuBuilder )
{
	if( Customization->HasPropertyNode() || Customization->GetWidgetRow().IsCopyPasteBound() )
	{
		FUIAction CopyAction  = Customization->GetWidgetRow().CopyMenuAction;
		FUIAction PasteAction = Customization->GetWidgetRow().PasteMenuAction;

		if( !CopyAction.ExecuteAction.IsBound() && Customization->HasPropertyNode() )
		{
			CopyAction.ExecuteAction = FExecuteAction::CreateSP( this, &SDetailSingleItemRow::OnCopyProperty );
		}

		if( !PasteAction.ExecuteAction.IsBound() && Customization->HasPropertyNode() )
		{
			PasteAction.ExecuteAction = FExecuteAction::CreateSP( this, &SDetailSingleItemRow::OnPasteProperty );
			PasteAction.CanExecuteAction = FCanExecuteAction::CreateSP( this, &SDetailSingleItemRow::CanPasteProperty );
		}

		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddMenuEntry(	
			NSLOCTEXT("PropertyView", "CopyProperty", "Copy"),
			NSLOCTEXT("PropertyView", "CopyProperty_ToolTip", "Copy this property value"),
			FSlateIcon(),
			CopyAction );

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("PropertyView", "PasteProperty", "Paste"),
			NSLOCTEXT("PropertyView", "PasteProperty_ToolTip", "Paste the copied value here"),
			FSlateIcon(),
			PasteAction);

		return true;
	}

	return false;
}

void SDetailSingleItemRow::OnLeftColumnResized( float InNewWidth )
{
	// This has to be bound or the splitter will take it upon itself to determine the size
	// We do nothing here because it is handled by the column size data
}

void SDetailSingleItemRow::OnCopyProperty()
{
	if( Customization->HasPropertyNode() && OwnerTreeNode.IsValid() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle( Customization->GetPropertyNode().ToSharedRef(), OwnerTreeNode.Pin()->GetDetailsView().GetNotifyHook(),  OwnerTreeNode.Pin()->GetDetailsView().GetPropertyUtilities() );

		FString Value;
		if( Handle->GetValueAsFormattedString(Value) == FPropertyAccess::Success )
		{
			FPlatformMisc::ClipboardCopy(*Value);
		}
	}
}

void SDetailSingleItemRow::OnPasteProperty()
{
	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);

	if( !ClipboardContent.IsEmpty() && Customization->HasPropertyNode() && OwnerTreeNode.IsValid() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(Customization->GetPropertyNode().ToSharedRef(), OwnerTreeNode.Pin()->GetDetailsView().GetNotifyHook(), OwnerTreeNode.Pin()->GetDetailsView().GetPropertyUtilities());

		Handle->SetValueFromFormattedString( ClipboardContent );
	}

}

bool SDetailSingleItemRow::CanPasteProperty() const
{
	FString ClipboardContent;
	if( OwnerTreeNode.IsValid() )
	{
		FPlatformMisc::ClipboardPaste(ClipboardContent);
	}

	return !ClipboardContent.IsEmpty();
}

const FSlateBrush* SDetailSingleItemRow::GetBorderImage() const
{
	if( IsHighlighted() )
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle_Highlighted");
	}
	else if (IsHovered())
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle_Hovered");
	}
	else
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle");
	}
}

TSharedRef<SWidget> SDetailSingleItemRow::CreateExtensionWidget(TSharedRef<SWidget> ValueWidget, FDetailLayoutCustomization& InCustomization, TSharedRef<IDetailTreeNode> InTreeNode)
{
	IDetailsViewPrivate& DetailsView = InTreeNode->GetDetailsView();
	TSharedPtr<IDetailPropertyExtensionHandler> ExtensionHandler = DetailsView.GetExtensionHandler();

	if ( ExtensionHandler.IsValid() && InCustomization.HasPropertyNode() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(InCustomization.GetPropertyNode().ToSharedRef(), nullptr, nullptr);

		UClass* ObjectClass = InCustomization.GetPropertyNode()->FindObjectItemParent()->GetObjectBaseClass();
		if (Handle->IsValidHandle() && ExtensionHandler->IsPropertyExtendable(ObjectClass, *Handle))
		{
			ValueWidget = SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					ValueWidget
				]
					
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ExtensionHandler->GenerateExtensionWidget(ObjectClass, Handle)
				];
		}
	}

	return ValueWidget;
}

TSharedRef<SWidget> SDetailSingleItemRow::CreateKeyframeButton( FDetailLayoutCustomization& InCustomization, TSharedRef<IDetailTreeNode> InTreeNode )
{
	IDetailsViewPrivate& DetailsView = InTreeNode->GetDetailsView();

	KeyframeHandler = DetailsView.GetKeyframeHandler();

	EVisibility SetKeyVisibility = EVisibility::Collapsed;

	if (InCustomization.HasPropertyNode() && KeyframeHandler.IsValid() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(InCustomization.GetPropertyNode().ToSharedRef(), nullptr, nullptr);

		UClass* ObjectClass = InCustomization.GetPropertyNode()->FindObjectItemParent()->GetObjectBaseClass();
		SetKeyVisibility = KeyframeHandler.Pin()->IsPropertyKeyable(ObjectClass, *Handle) ? EVisibility::Visible : EVisibility::Hidden;
		
	}

	return
		SNew(SButton)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.ContentPadding(0.0f)
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.AddKey.Details")
		.Visibility(SetKeyVisibility)
		.IsEnabled( this, &SDetailSingleItemRow::IsKeyframeButtonEnabled, InTreeNode )
		.ToolTipText( NSLOCTEXT("PropertyView", "AddKeyframeButton_ToolTip", "Adds a keyframe for this property to the current animation") )
		.OnClicked( this, &SDetailSingleItemRow::OnAddKeyframeClicked );
}

bool SDetailSingleItemRow::IsKeyframeButtonEnabled(TSharedRef<IDetailTreeNode> InTreeNode) const
{
	return
		InTreeNode->IsPropertyEditingEnabled().Get() &&
		KeyframeHandler.IsValid() &&
		KeyframeHandler.Pin()->IsPropertyKeyingEnabled();
}

FReply SDetailSingleItemRow::OnAddKeyframeClicked()
{	
	if( KeyframeHandler.IsValid() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(Customization->GetPropertyNode().ToSharedRef(), nullptr, nullptr);

		KeyframeHandler.Pin()->OnKeyPropertyClicked(*Handle);
	}

	return FReply::Handled();
}

bool SDetailSingleItemRow::IsHighlighted() const
{
	return OwnerTreeNode.Pin()->IsHighlighted();
}

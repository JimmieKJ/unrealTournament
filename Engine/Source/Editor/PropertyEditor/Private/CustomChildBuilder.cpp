// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "CustomChildBuilder.h"
#include "DetailGroup.h"
#include "PropertyHandleImpl.h"
#include "DetailPropertyRow.h"

IDetailChildrenBuilder& FCustomChildrenBuilder::AddChildCustomBuilder( TSharedRef<class IDetailCustomNodeBuilder> InCustomBuilder )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.CustomBuilderRow = MakeShareable( new FDetailCustomBuilderRow( InCustomBuilder ) );

	ChildCustomizations.Add( NewCustomization );
	return *this;
}

IDetailGroup& FCustomChildrenBuilder::AddChildGroup( FName GroupName, const FText& LocalizedDisplayName )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.DetailGroup = MakeShareable( new FDetailGroup( GroupName, ParentCategory.Pin().ToSharedRef(), LocalizedDisplayName ) );

	ChildCustomizations.Add( NewCustomization );

	return *NewCustomization.DetailGroup;
}

FDetailWidgetRow& FCustomChildrenBuilder::AddChildContent( const FText& SearchString )
{
	TSharedRef<FDetailWidgetRow> NewRow = MakeShareable( new FDetailWidgetRow );
	FDetailLayoutCustomization NewCustomization;

	NewRow->FilterString( SearchString );

	NewCustomization.WidgetDecl = NewRow;

	ChildCustomizations.Add( NewCustomization );
	return *NewRow;
}

IDetailPropertyRow& FCustomChildrenBuilder::AddChildProperty( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() )

	FDetailLayoutCustomization NewCustomization;
	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	if (CustomResetChildToDefault.IsSet())
	{
		NewCustomization.PropertyRow->OverrideResetToDefault(CustomResetChildToDefault.GetValue());
	}

	ChildCustomizations.Add( NewCustomization );

	return *NewCustomization.PropertyRow;
}

class SStandaloneCustomStructValue : public SCompoundWidget, public IPropertyTypeCustomizationUtils
{
public:
	SLATE_BEGIN_ARGS( SStandaloneCustomStructValue )
	{}
	SLATE_END_ARGS()
	
	void Construct( const FArguments& InArgs, TSharedPtr<IPropertyTypeCustomization> InCustomizationInterface, TSharedRef<IPropertyHandle> InStructPropertyHandle, TSharedRef<FDetailCategoryImpl> InParentCategory )
	{
		CustomizationInterface = InCustomizationInterface;
		StructPropertyHandle = InStructPropertyHandle;
		ParentCategory = InParentCategory;
		CustomPropertyWidget = MakeShareable(new FDetailWidgetRow);

		CustomizationInterface->CustomizeHeader(InStructPropertyHandle, *CustomPropertyWidget, *this);

		ChildSlot
		[
			CustomPropertyWidget->ValueWidget.Widget
		];
	}

	virtual TSharedPtr<FAssetThumbnailPool> GetThumbnailPool() const override
	{
		TSharedPtr<FDetailCategoryImpl> ParentCategoryPinned = ParentCategory.Pin();
		return ParentCategoryPinned.IsValid() ? ParentCategoryPinned->GetParentLayout().GetThumbnailPool() : NULL;
	}

private:
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
	TSharedPtr<IPropertyTypeCustomization> CustomizationInterface;
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<FDetailWidgetRow> CustomPropertyWidget;
};


TSharedRef<SWidget> FCustomChildrenBuilder::GenerateStructValueWidget( TSharedRef<IPropertyHandle> StructPropertyHandle )
{
	UStructProperty* StructProperty = CastChecked<UStructProperty>( StructPropertyHandle->GetProperty() );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	IDetailsViewPrivate& DetailsView = ParentCategory.Pin()->GetDetailsView();
	
	TSharedRef<IDetailsView> DetailsViewPtr = StaticCastSharedRef<IDetailsView>( DetailsView.AsShared() );
	
	FPropertyTypeLayoutCallback LayoutCallback = PropertyEditorModule.GetPropertyTypeCustomization(StructProperty, *StructPropertyHandle, DetailsViewPtr );
	if (LayoutCallback.IsValid())
	{
		TSharedRef<IPropertyTypeCustomization> CustomStructInterface = LayoutCallback.GetCustomizationInstance();

		return SNew( SStandaloneCustomStructValue, CustomStructInterface, StructPropertyHandle, ParentCategory.Pin().ToSharedRef() );
	}
	else
	{
		// Uncustomized structs have nothing for their value content
		return SNullWidget::NullWidget;
	}
}

FCustomChildrenBuilder& FCustomChildrenBuilder::OverrideResetChildrenToDefault(const FResetToDefaultOverride& ResetToDefault)
{
	CustomResetChildToDefault = ResetToDefault;
	return *this;
}

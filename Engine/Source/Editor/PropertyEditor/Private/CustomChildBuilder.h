// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FCustomChildrenBuilder : public IDetailChildrenBuilder
{
public:
	FCustomChildrenBuilder( TSharedRef<FDetailCategoryImpl> InParentCategory )
		: ParentCategory( InParentCategory )
	{}
	
	virtual IDetailChildrenBuilder& AddChildCustomBuilder( TSharedRef<class IDetailCustomNodeBuilder> InCustomBuilder ) override;
	virtual IDetailGroup& AddChildGroup( FName GroupName, const FText& LocalizedDisplayName ) override;
	virtual FDetailWidgetRow& AddChildContent( const FText& SearchString ) override;
	virtual IDetailPropertyRow& AddChildProperty( TSharedRef<IPropertyHandle> PropertyHandle ) override;
	virtual TSharedRef<SWidget> GenerateStructValueWidget( TSharedRef<IPropertyHandle> StructPropertyHandle ) override;

	const TArray< FDetailLayoutCustomization >& GetChildCustomizations() const { return ChildCustomizations; }
private:
	TArray< FDetailLayoutCustomization > ChildCustomizations;
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
};

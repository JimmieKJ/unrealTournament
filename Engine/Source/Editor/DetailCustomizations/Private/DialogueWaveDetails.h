// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDialogueContextMappingNodeBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FDialogueContextMappingNodeBuilder>
{
public:
	FDialogueContextMappingNodeBuilder( IDetailLayoutBuilder* InDetailLayoutBuilder, const TSharedPtr<IPropertyHandle>& InPropertyHandle );

	/** IDetailCustomNodeBuilder interface */
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRebuildChildren  ) override { OnRebuildChildren = InOnRebuildChildren; } 
	virtual bool RequiresTick() const override { return false; }
	virtual void Tick( float DeltaTime ) override {};
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual bool InitiallyCollapsed() const override { return true; };
	virtual FName GetName() const override { return NAME_None; }

private:
	void RemoveContextButton_OnClick();

private:
	/** Called to rebuild the children of the detail tree */
	FSimpleDelegate OnRebuildChildren;

	/** Associated detail layout builder */
	IDetailLayoutBuilder* DetailLayoutBuilder;

	/** Property handle to associated context mapping */
	TSharedPtr<IPropertyHandle> ContextMappingPropertyHandle;

	/** Count of the currently displayed targets */
	uint32 DisplayedTargetCount;
};

class FDialogueWaveDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( class IDetailLayoutBuilder& DetailBuilder ) override;

private:
	FReply AddDialogueContextMapping_OnClicked();

private:
	/** Associated detail layout builder */
	IDetailLayoutBuilder* DetailLayoutBuilder;
};
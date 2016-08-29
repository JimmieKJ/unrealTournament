// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SImage.h"

class SGraphNode;

#define NAME_DefaultPinLabelStyle TEXT("Graph.Node.PinName")

/**
 * Represents a pin on a node in the GraphEditor
 */
class GRAPHEDITOR_API SGraphPin : public SBorder
{
public:

	SLATE_BEGIN_ARGS(SGraphPin)
		: _PinLabelStyle(NAME_DefaultPinLabelStyle)
		, _UsePinColorForText(false)
		, _SideToSideMargin(5.0f)
		{}
		SLATE_ARGUMENT(FName, PinLabelStyle)
		SLATE_ARGUMENT(bool, UsePinColorForText)
		SLATE_ARGUMENT(float, SideToSideMargin)
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InPin	The pin to create a widget for
	 */
	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

public:
	SGraphPin();

	/** Set attribute for determining if pin is editable */
	void SetIsEditable(TAttribute<bool> InIsEditable);

	/** Retrieves the full horizontal box that contains the pin's row content */
	TWeakPtr<SHorizontalBox> GetFullPinHorizontalRowWidget()
	{
		return FullPinHorizontalRowWidget;
	}

	/** Handle clicking on the pin */
	virtual FReply OnPinMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent);

	/** Handle clicking on the pin name */
	FReply OnPinNameMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent);

	// SWidget interface
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

public:
	UEdGraphPin* GetPinObj() const;

	/** @param OwnerNode  The SGraphNode that this pin belongs to */
	void SetOwner( const TSharedRef<SGraphNode> OwnerNode );

	/** @return whether this pin is incoming or outgoing */
	EEdGraphPinDirection GetDirection() const;

	/** @return whether this pin is an array value */
	bool IsArray() const;

	/** @return whether this pin is passed by ref */
	bool IsByRef() const;

	/** @return whether this pin is passed by mutable ref */
	bool IsByMutableRef() const;

	/** @return whether this pin is passed by mutable ref */
	bool IsDelegate() const;

	/** @return whether this pin is passed by const ref */
	bool IsByConstRef() const;

	/** @return whether this pin is connected to another pin */
	bool IsConnected() const;

	/** Tries to handle making a connection to another pin, depending on the schema and the pins it may do:  
		 - Nothing
		 - Break existing links on either side while making the new one
		 - Just make the new one
		 @return Whether a connection was actually made or not
     */
	virtual bool TryHandlePinConnection(SGraphPin& OtherSPin);

	/** @return Visibility based on whether or not we should show the inline editable text field for this pin */
	virtual EVisibility GetDefaultValueVisibility() const;

	/** Control whether we draw the label on this pin */
	void SetShowLabel(bool bNewDrawLabel);

	/** Allows the connection drawing policy to control the pin color */
	void SetPinColorModifier(FLinearColor InColor)
	{
		PinColorModifier = InColor;
	}

	/** If pin in node is visible at all */
	EVisibility IsPinVisibleAsAdvanced() const;

	/** Gets Node Offset */
	FVector2D GetNodeOffset() const;

	/** Returns whether or not this pin is currently connectable */
	bool GetIsConnectable() const;

protected:
	FText GetPinLabel() const;

	/** If we should show the label on this pin */
	EVisibility GetPinLabelVisibility() const
	{
		return bShowLabel ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/** Get the widget we should put into the 'default value' space, shown when nothing connected */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget();

	/** @return The brush with which to pain this graph pin's incoming/outgoing bullet point */
	virtual const FSlateBrush* GetPinIcon() const;

	const FSlateBrush* GetPinBorder() const;
	
	/** @return The status brush to show after/before the name (on an input/output) */
	virtual const FSlateBrush* GetPinStatusIcon() const;

	/** @return Should we show a status brush after/before the name (on an input/output) */
	virtual EVisibility GetPinStatusIconVisibility() const;

	/** Toggle the watch pinning */
	virtual FReply ClickedOnPinStatusIcon();

	/** @return The color that we should use to draw this pin */
	virtual FSlateColor GetPinColor() const;

	/** @return The color that we should use to draw this pin's text */
	virtual FSlateColor GetPinTextColor() const;

	/** @return The tooltip to display for this pin */
	FText GetTooltipText() const;

	TOptional<EMouseCursor::Type> GetPinCursor() const;

	/** Spawns a FDragConnection or similar class for the pin drag event */
	virtual TSharedRef<FDragDropOperation> SpawnPinDragEvent(const TSharedRef<class SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bShiftOperation);
	
	/** True if pin can be edited */
	bool IsEditingEnabled() const;

	// Should we use low-detail pin names?
	virtual bool UseLowDetailPinNames() const;

	/** Determines the pin's visibility based on the LOD factor, when it is low LOD, no hit test will occur */
	EVisibility GetPinVisiblity() const;

protected:
	/** The GraphNode that owns this pin */
	TWeakPtr<SGraphNode> OwnerNodePtr;

	/** Image of pin */
	TSharedPtr<SImage> PinImage;

	/** Horizontal box that holds the full detail pin widget, useful for outsiders to inject widgets into the pin */
	TWeakPtr<SHorizontalBox> FullPinHorizontalRowWidget;

	/** The GraphPin that this widget represents. */
	class UEdGraphPin* GraphPinObj;

	/** Is this pin editable */
	TAttribute<bool> IsEditable;

	/** If we should draw the label on this pin */
	bool bShowLabel;

	/** true when we're moving links between pins. */
	bool bIsMovingLinks;

	/** Color modifier for use by the connection drawing policy */
	FLinearColor PinColorModifier;

	/** Cached offset from owning node to approximate position of culled pins */
	FVector2D CachedNodeOffset;

	TSet< FEdGraphPinReference > HoverPinSet;

	/** TRUE if the pin should use the Pin's color for the text */
	bool bUsePinColorForText;

	//@TODO: Want to cache these once for all SGraphPins, but still handle slate style updates
	mutable const FSlateBrush* CachedImg_ArrayPin_ConnectedHovered;
	mutable const FSlateBrush* CachedImg_ArrayPin_Connected;
	mutable const FSlateBrush* CachedImg_ArrayPin_DisconnectedHovered;
	mutable const FSlateBrush* CachedImg_ArrayPin_Disconnected;
	mutable const FSlateBrush* CachedImg_RefPin_ConnectedHovered;
	mutable const FSlateBrush* CachedImg_RefPin_Connected;
	mutable const FSlateBrush* CachedImg_RefPin_DisconnectedHovered;
	mutable const FSlateBrush* CachedImg_RefPin_Disconnected;
	mutable const FSlateBrush* CachedImg_Pin_ConnectedHovered;
	mutable const FSlateBrush* CachedImg_Pin_Connected;
	mutable const FSlateBrush* CachedImg_Pin_DisconnectedHovered;
	mutable const FSlateBrush* CachedImg_Pin_Disconnected;
	mutable const FSlateBrush* CachedImg_DelegatePin_ConnectedHovered;
	mutable const FSlateBrush* CachedImg_DelegatePin_Connected;
	mutable const FSlateBrush* CachedImg_DelegatePin_DisconnectedHovered;
	mutable const FSlateBrush* CachedImg_DelegatePin_Disconnected;

	mutable const FSlateBrush* CachedImg_Pin_Background;
	mutable const FSlateBrush* CachedImg_Pin_BackgroundHovered;
};

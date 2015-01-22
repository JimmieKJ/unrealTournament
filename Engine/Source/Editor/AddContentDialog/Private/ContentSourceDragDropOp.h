// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecoratedDragDropOp.h"

class FContentSourceViewModel;

/** A drag drop operation for dragging and dropping FContentSourceViewModels. */
class FContentSourceDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FContentSourceDragDropOp, FDecoratedDragDropOp)

	/** Creates and constructs a shared references to a FContentSourceDragDrop.
		@param InContentSource - The view model for the content source being dragged and dropped */
	static TSharedRef<FContentSourceDragDropOp> CreateShared(TSharedPtr<FContentSourceViewModel> InContentSource);

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	/** Gets the view model for the content source being dragged and dropped. */
	TSharedPtr<FContentSourceViewModel> GetContentSource();

private:
	FContentSourceDragDropOp(TSharedPtr<FContentSourceViewModel> InContentSource);

private:
	/** The view model for the content source being dragged and dropped. */
	TSharedPtr<FContentSourceViewModel> ContentSource;
};
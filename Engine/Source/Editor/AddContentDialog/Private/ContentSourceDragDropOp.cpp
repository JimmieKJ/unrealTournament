// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogPCH.h"

TSharedRef<FContentSourceDragDropOp> FContentSourceDragDropOp::CreateShared(TSharedPtr<FContentSourceViewModel> InContentSource)
{
	TSharedPtr<FContentSourceDragDropOp> DragDropOp = MakeShareable(new FContentSourceDragDropOp(InContentSource));
	DragDropOp->MouseCursor = EMouseCursor::GrabHandClosed;
	DragDropOp->Construct();
	return DragDropOp.ToSharedRef();
}

FContentSourceDragDropOp::FContentSourceDragDropOp(TSharedPtr<FContentSourceViewModel> InContentSource)
{
	ContentSource = InContentSource;
}

TSharedPtr<SWidget> FContentSourceDragDropOp::GetDefaultDecorator() const
{
	return SNew(SImage)
		.Image(ContentSource->GetIconBrush().Get());
}

TSharedPtr<FContentSourceViewModel> FContentSourceDragDropOp::GetContentSource()
{
	return ContentSource;
}
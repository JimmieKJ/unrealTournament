// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#if !UE_SERVER
class UNREALTOURNAMENT_API SUTAspectPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTAspectPanel)
	{
	}

	/** Slot for this designers content (optional) */
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;

	/** See Content slot */
	void SetContent(TSharedRef<SWidget> InContent);

};
#endif
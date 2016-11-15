// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#if !UE_SERVER
class UNREALTOURNAMENT_API SUTAspectPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTAspectPanel)
		: _bCenterPanel(false)
	{
	}

	/** Slot for this designers content (optional) */
	SLATE_DEFAULT_SLOT(FArguments, Content)
		/** If true, this panel will center the content */
		SLATE_ARGUMENT( bool, bCenterPanel)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual float GetRelativeLayoutScale(const FSlotBase& Child) const override;

	/** See Content slot */
	void SetContent(TSharedRef<SWidget> InContent);

protected:
	bool bCenterPanel;

private:
	mutable float CachedLayoutScale;
};
#endif
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeWidgetHost.generated.h"

/**
 * A NativeWidgetHost is a container widget that can contain one child slate widget.  This should
 * be used when all you need is to nest a native widget inside a UMG widget.
 */
UCLASS()
class UMG_API UNativeWidgetHost : public UWidget
{
	GENERATED_UCLASS_BODY()

	void SetContent(TSharedRef<SWidget> InContent);
	TSharedPtr< SWidget > GetContent() const { return NativeWidget; }

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FSlateBrush* GetEditorIcon() override;
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	TSharedRef<SWidget> GetDefaultContent();

protected:
	TSharedPtr<SWidget> NativeWidget;
};

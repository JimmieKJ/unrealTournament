// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICursor.h"
#include "Math/IntRect.h"

@class FCocoaWindow;

class FMacCursor : public ICursor
{
public:

	FMacCursor();

	virtual ~FMacCursor();

	virtual FVector2D GetPosition() const override;

	virtual void SetPosition(const int32 X, const int32 Y) override;

	virtual void SetType(const EMouseCursor::Type InNewCursor) override;

	virtual EMouseCursor::Type GetType() const override { return CurrentType; }

	virtual void GetSize(int32& Width, int32& Height) const override;

	virtual void Show(bool bShow) override;

	virtual void Lock(const RECT* const Bounds) override;

	virtual void SetCustomShape(void* CursorHandle) override;

public:

	bool UpdateCursorClipping(FVector2D& CursorPosition);

	void WarpCursor(const int32 X, const int32 Y);

	FVector2D GetMouseWarpDelta();

	void SetHighPrecisionMouseMode(const bool bEnable);

	void SetMouseScaling(const FVector2D& Scale, FCocoaWindow* InFullScreenWindow);

	const FVector2D& GetMouseScaling() const;

	FVector2D GetPositionNoScaling() const;

	void SetPositionNoScaling(const int32 X, const int32 Y);

	FCocoaWindow* GetFullScreenWindow() const { return FullScreenWindow; }

	void UpdateCurrentPosition(const FVector2D& Position);

	void UpdateVisibility();

	bool IsLocked() const { return CursorClipRect.Area() > 0; }

	void SetShouldIgnoreLocking(bool bIgnore) { bShouldIgnoreLocking = bIgnore; }

private:

	EMouseCursor::Type CurrentType;

	NSCursor* CursorHandles[EMouseCursor::TotalCursorCount];

	FIntRect CursorClipRect;

	bool bIsVisible;
	bool bUseHighPrecisionMode;
	NSCursor* CurrentCursor;

	FVector2D CurrentPosition;
	FVector2D MouseWarpDelta;
	FVector2D MouseScale;
	bool bIsPositionInitialised;
	bool bShouldIgnoreLocking;
	FCocoaWindow* FullScreenWindow;

	io_object_t HIDInterface;
	double SavedAcceleration;
};

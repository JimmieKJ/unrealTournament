// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "LayoutGeometry.h"

FGeometry::FGeometry() 
	: Size(0.0f, 0.0f)
	, Scale(1.0f)
	, AbsolutePosition(0.0f, 0.0f)
{
}

FGeometry& FGeometry::operator=(const FGeometry& RHS)
{
	// HACK to allow us to make FGeometry public members immutable to catch misuse.
	if (this != &RHS)
	{
		FMemory::Memcpy(*this, RHS);
	}
	return *this;
}

FGeometry::FGeometry(const FVector2D& OffsetFromParent, const FVector2D& ParentAbsolutePosition, const FVector2D& InLocalSize, float InScale) 
	: Size(InLocalSize)
	, Scale(1.0f)
	, AbsolutePosition(0.0f, 0.0f)
{
	// Since OffsetFromParent is given as a LocalSpaceOffset, we MUST convert this offset into the space of the parent to construct a valid layout transform.
	// The extra TransformPoint below does this by converting the local offset to an offset in parent space.
	FVector2D LayoutOffset = TransformPoint(InScale, OffsetFromParent);

	FSlateLayoutTransform ParentAccumulatedLayoutTransform(InScale, ParentAbsolutePosition);
	FSlateLayoutTransform LocalLayoutTransform(LayoutOffset);
	FSlateLayoutTransform AccumulatedLayoutTransform = Concatenate(LocalLayoutTransform, ParentAccumulatedLayoutTransform);
	AccumulatedRenderTransform = TransformCast<FSlateRenderTransform>(AccumulatedLayoutTransform);
	// HACK to allow us to make FGeometry public members immutable to catch misuse.
	const_cast<FVector2D&>(AbsolutePosition) = AccumulatedLayoutTransform.GetTranslation();
	const_cast<float&>(Scale) = AccumulatedLayoutTransform.GetScale();
	const_cast<FVector2D&>(Position) = LocalLayoutTransform.GetTranslation();
}

FGeometry::FGeometry(const FVector2D& InLocalSize, const FSlateLayoutTransform& InLocalLayoutTransform, const FSlateRenderTransform& InLocalRenderTransform, const FVector2D& InLocalRenderTransformPivot, const FSlateLayoutTransform& ParentAccumulatedLayoutTransform, const FSlateRenderTransform& ParentAccumulatedRenderTransform) 
	: Size(InLocalSize)
	, Scale(1.0f)
	, AbsolutePosition(0.0f, 0.0f)
	, AccumulatedRenderTransform(
		Concatenate(
			// convert the pivot to local space and make it the origin
			Inverse(TransformPoint(FScale2D(InLocalSize), InLocalRenderTransformPivot)), 
			// apply the render transform in local space centered around the pivot
			InLocalRenderTransform, 
			// translate the pivot point back.
			TransformPoint(FScale2D(InLocalSize), InLocalRenderTransformPivot), 
			// apply the layout transform next.
			InLocalLayoutTransform, 
			// finally apply the parent accumulated transform, which takes us to the root.
			ParentAccumulatedRenderTransform))
{
	FSlateLayoutTransform AccumulatedLayoutTransform = Concatenate(InLocalLayoutTransform, ParentAccumulatedLayoutTransform);
	// HACK to allow us to make FGeometry public members immutable to catch misuse.
	const_cast<FVector2D&>(AbsolutePosition) = AccumulatedLayoutTransform.GetTranslation();
	const_cast<float&>(Scale) = AccumulatedLayoutTransform.GetScale();
	const_cast<FVector2D&>(Position) = InLocalLayoutTransform.GetTranslation();
}

FGeometry::FGeometry(const FVector2D& InLocalSize, const FSlateLayoutTransform& InLocalLayoutTransform, const FSlateLayoutTransform& ParentAccumulatedLayoutTransform, const FSlateRenderTransform& ParentAccumulatedRenderTransform) 
	: Size(InLocalSize)
	, Scale(1.0f)
	, AbsolutePosition(0.0f, 0.0f)
	, AccumulatedRenderTransform(Concatenate(InLocalLayoutTransform, ParentAccumulatedRenderTransform))
{
	FSlateLayoutTransform AccumulatedLayoutTransform = Concatenate(InLocalLayoutTransform, ParentAccumulatedLayoutTransform);
	// HACK to allow us to make FGeometry public members immutable to catch misuse.
	const_cast<FVector2D&>(AbsolutePosition) = AccumulatedLayoutTransform.GetTranslation();
	const_cast<float&>(Scale) = AccumulatedLayoutTransform.GetScale();
	const_cast<FVector2D&>(Position) = InLocalLayoutTransform.GetTranslation();
}

void FGeometry::AppendTransform(const FSlateLayoutTransform& LayoutTransform)
{
	FSlateLayoutTransform AccumulatedLayoutTransform = ::Concatenate(GetAccumulatedLayoutTransform(), LayoutTransform);
	AccumulatedRenderTransform = ::Concatenate(AccumulatedRenderTransform, LayoutTransform);
	const_cast<FVector2D&>(AbsolutePosition) = AccumulatedLayoutTransform.GetTranslation();
	const_cast<float&>(Scale) = AccumulatedLayoutTransform.GetScale();
}

FGeometry FGeometry::MakeRoot(const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform)
{
	return FGeometry(LocalSize, LayoutTransform, FSlateLayoutTransform(), FSlateRenderTransform());
}

FGeometry FGeometry::MakeChild(const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform, const FSlateRenderTransform& RenderTransform, const FVector2D& RenderTransformPivot) const
{
	return FGeometry(LocalSize, LayoutTransform, RenderTransform, RenderTransformPivot, GetAccumulatedLayoutTransform(), GetAccumulatedRenderTransform());
}

FGeometry FGeometry::MakeChild( const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform ) const
{
	return FGeometry(LocalSize, LayoutTransform, GetAccumulatedLayoutTransform(), GetAccumulatedRenderTransform());
}

FArrangedWidget FGeometry::MakeChild(const TSharedRef<SWidget>& ChildWidget, const FLayoutGeometry& LayoutGeometry) const
{
	return MakeChild(ChildWidget, LayoutGeometry.GetSizeInLocalSpace(), LayoutGeometry.GetLocalToParentTransform());
}

FArrangedWidget FGeometry::MakeChild(const TSharedRef<SWidget>& ChildWidget, const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform) const
{
	// If there is no render transform set, use the simpler MakeChild call that doesn't bother concatenating the render transforms.
	// This saves a significant amount of overhead since every widget does this, and most children don't have a render transform.
	if (ChildWidget->GetRenderTransform().IsSet())
	{
		return FArrangedWidget(ChildWidget, MakeChild(LocalSize, LayoutTransform, ChildWidget->GetRenderTransform().GetValue(), ChildWidget->GetRenderTransformPivot()));
	}
	else
	{
		return FArrangedWidget(ChildWidget, MakeChild(LocalSize, LayoutTransform));
	}
}

FGeometry FGeometry::MakeChild( const FVector2D& ChildOffset, const FVector2D& LocalSize, float LocalScale ) const
{
	// Since ChildOffset is given as a LocalSpaceOffset, we MUST convert this offset into the space of the parent to construct a valid layout transform.
	// The extra TransformPoint below does this by converting the local offset to an offset in parent space.
	return FGeometry(LocalSize, FSlateLayoutTransform(LocalScale, TransformPoint(LocalScale, ChildOffset)), GetAccumulatedLayoutTransform(), GetAccumulatedRenderTransform());
}

FArrangedWidget FGeometry::MakeChild( const TSharedRef<SWidget>& ChildWidget, const FVector2D& ChildOffset, const FVector2D& LocalSize, float ChildScale) const
{
	// Since ChildOffset is given as a LocalSpaceOffset, we MUST convert this offset into the space of the parent to construct a valid layout transform.
	// The extra TransformPoint below does this by converting the local offset to an offset in parent space.
	return MakeChild(ChildWidget, LocalSize, FSlateLayoutTransform(ChildScale, TransformPoint(ChildScale, ChildOffset)));
}

FPaintGeometry FGeometry::ToPaintGeometry() const
{
	return FPaintGeometry(GetAccumulatedLayoutTransform(), GetAccumulatedRenderTransform(), Size);
}

FPaintGeometry FGeometry::ToPaintGeometry(const FSlateLayoutTransform& LayoutTransform) const
{
	return ToPaintGeometry(Size, LayoutTransform);
}


FPaintGeometry FGeometry::ToPaintGeometry(const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform) const
{
	FSlateLayoutTransform NewAccumulatedLayoutTransform = Concatenate(LayoutTransform, GetAccumulatedLayoutTransform());
	return FPaintGeometry(NewAccumulatedLayoutTransform, Concatenate(LayoutTransform, GetAccumulatedRenderTransform()), LocalSize);
}

FPaintGeometry FGeometry::ToPaintGeometry(const FVector2D& LocalOffset, const FVector2D& LocalSize, float LocalScale) const
{
	// Since ChildOffset is given as a LocalSpaceOffset, we MUST convert this offset into the space of the parent to construct a valid layout transform.
	// The extra TransformPoint below does this by converting the local offset to an offset in parent space.
	return ToPaintGeometry(LocalSize, FSlateLayoutTransform(LocalScale, TransformPoint(LocalScale, LocalOffset)));
}

FPaintGeometry FGeometry::ToOffsetPaintGeometry(const FVector2D& LocalOffset) const
{
	return ToPaintGeometry(FSlateLayoutTransform(LocalOffset));
}

FPaintGeometry FGeometry::ToInflatedPaintGeometry( const FVector2D& InflateAmount ) const
{
	// This essentially adds (or subtracts) a border around the widget. We scale the size then offset by the border amount.
	// Note this is not scaling child widgets, so the scale is not changing.
	FVector2D NewSize = Size + InflateAmount * 2;
	return ToPaintGeometry(NewSize, FSlateLayoutTransform(-InflateAmount));
}

FSlateRect FGeometry::GetClippingRect() const
{
	return TransformRect(GetAccumulatedLayoutTransform(), FSlateRect(FVector2D(0.0f,0.0f), Size));
}

FVector2D FGeometry::AbsoluteToLocal(FVector2D AbsoluteCoordinate) const
{
	// this render transform invert is a little expensive. We might consider caching it.
	return TransformPoint(Inverse(GetAccumulatedRenderTransform()), AbsoluteCoordinate);
}


FVector2D FGeometry::LocalToAbsolute(FVector2D LocalCoordinate) const
{
	return TransformPoint(GetAccumulatedRenderTransform(), LocalCoordinate);
}

bool FGeometry::IsUnderLocation(const FVector2D& AbsoluteCoordinate) const
{
	// this render transform invert is a little expensive. We might consider caching it.
	return FSlateRect(FVector2D(0.0f, 0.0f), Size).ContainsPoint(TransformPoint(Inverse(GetAccumulatedRenderTransform()), AbsoluteCoordinate));
}


FString FGeometry::ToString() const
{
	return FString::Printf(TEXT("[Abs=%s, Scale=%.2f, Size=%s]"), *AbsolutePosition.ToString(), Scale, *Size.ToString());
}

FVector2D FGeometry::GetDrawSize() const
{
	return TransformVector(GetAccumulatedLayoutTransform(), Size);
}

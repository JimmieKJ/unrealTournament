// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateLayoutTransform.h"
#include "SlateRenderTransform.h"
#include "PaintGeometry.h"
#include "Geometry.generated.h"

class FArrangedWidget;
class SWidget;
class FLayoutGeometry;

/**
 * Represents the position, size, and absolute position of a Widget in Slate.
 * The absolute location of a geometry is usually screen space or
 * window space depending on where the geometry originated.
 * Geometries are usually paired with a SWidget pointer in order
 * to provide information about a specific widget (see FArrangedWidget).
 * A Geometry's parent is generally thought to be the Geometry of the
 * the corresponding parent widget.
 */
USTRUCT(BlueprintType)
struct SLATECORE_API FGeometry
{
	GENERATED_USTRUCT_BODY()

public:

	/**
	 * Default constructor. Creates a geometry with identity transforms.
	 */
	FGeometry();

	/**
	 * !!! HACK!!! We're keeping members of FGeometry const to prevent mutability without making them private, for backward compatibility.
	 * But this means the assignment operator no longer works. We implement one ourselves now and force a memcpy.
	 */
	FGeometry& operator=(const FGeometry& RHS);

	/**
	 * !!! DEPRECATED FUNCTION !!! Use MakeChild taking a layout transform instead!
	 * Construct a new geometry given the following parameters:
	 * 
	 * @param OffsetFromParent         Local position of this geometry within its parent geometry.
	 * @param ParentAbsolutePosition   The absolute position of the parent geometry containing this geometry.
	 * @param InSize                   The size of this geometry.
	 * @param InScale                  The scale of this geometry with respect to Normal Slate Coordinates.
	 */
	FGeometry( const FVector2D& OffsetFromParent, const FVector2D& ParentAbsolutePosition, const FVector2D& InLocalSize, float InScale );

private:
	/**
	 * Construct a new geometry with a given size in LocalSpace that is attached to a parent geometry with the given layout and render transform. 
	 * 
	 * @param InLocalSize						The size of the geometry in Local Space.
	 * @param InLocalLayoutTransform			A layout transform from local space to the parent geoemtry's local space.
	 * @param InLocalRenderTransform			A render-only transform in local space that will be prepended to the LocalLayoutTransform when rendering.
	 * @param InLocalRenderTransformPivot		Pivot in normalizes local space for the local render transform.
	 * @param ParentAccumulatedLayoutTransform	The accumulated layout transform of the parent widget. AccumulatedLayoutTransform = Concat(LocalLayoutTransform, ParentAccumulatedLayoutTransform).
	 * @param ParentAccumulatedRenderTransform	The accumulated render transform of the parent widget. AccumulatedRenderTransform = Concat(LocalRenderTransform, LocalLayoutTransform, ParentAccumulatedRenderTransform).
	 */
	FGeometry( 
		const FVector2D& InLocalSize, 
		const FSlateLayoutTransform& InLocalLayoutTransform, 
		const FSlateRenderTransform& InLocalRenderTransform, 
		const FVector2D& InLocalRenderTransformPivot, 
		const FSlateLayoutTransform& ParentAccumulatedLayoutTransform, 
		const FSlateRenderTransform& ParentAccumulatedRenderTransform);

	/**
	 * Construct a new geometry with a given size in LocalSpace (and identity render transform) that is attached to a parent geometry with the given layout and render transform. 
	 * 
	 * @param InLocalSize						The size of the geometry in Local Space.
	 * @param InLocalLayoutTransform			A layout transform from local space to the parent geoemtry's local space.
	 * @param ParentAccumulatedLayoutTransform	The accumulated layout transform of the parent widget. AccumulatedLayoutTransform = Concat(LocalLayoutTransform, ParentAccumulatedLayoutTransform).
	 * @param ParentAccumulatedRenderTransform	The accumulated render transform of the parent widget. AccumulatedRenderTransform = Concat(LocalRenderTransform, LocalLayoutTransform, ParentAccumulatedRenderTransform).
	 */
	FGeometry( 
		const FVector2D& InLocalSize, 
		const FSlateLayoutTransform& InLocalLayoutTransform, 
		const FSlateLayoutTransform& ParentAccumulatedLayoutTransform, 
		const FSlateRenderTransform& ParentAccumulatedRenderTransform);

public:

	/**
	 * Test geometries for strict equality (i.e. exact float equality).
	 *
	 * @param Other A geometry to compare to.
	 * @return true if this is equal to other, false otherwise.
	 */
	bool operator==( const FGeometry& Other ) const
	{
		return
			this->Size == Other.Size && 
			this->AbsolutePosition == Other.AbsolutePosition &&
			this->Position == Other.Position &&
			this->Scale == Other.Scale;
	}
	
	/**
	 * Test geometries for strict inequality (i.e. exact float equality negated).
	 *
	 * @param Other A geometry to compare to.
	 * @return false if this is equal to other, true otherwise.
	 */
	bool operator!=( const FGeometry& Other ) const
	{
		return !( this->operator==(Other) );
	}

public:
	/**
	 * Makes a new geometry that is essentially the root of a hierarchy (has no parent transforms to inherit).
	 * For a root Widget, the LayoutTransform is often the window DPI scale + window offset.
	 * 
	 * @param LocalSize			Size of the geoemtry in Local Space.
	 * @param LayoutTransform	Layout transform of the geometry.
	 * @return					The new root geometry
	 */
	static FGeometry MakeRoot( const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform );

	/**
	 * Create a child geometry relative to this one with a given local space size, layout transform, and render transform.
	 * For example, a widget with a 5x5 margin will create a geometry for it's child contents having a LayoutTransform of Translate(5,5) and a LocalSize 10 units smaller 
	 * than it's own.
	 *
	 * @param LocalSize			The size of the child geometry in local space.
	 * @param LayoutTransform	Layout transform of the new child relative to this Geometry. Goes from the child's layout space to the this widget's layout space.
	 * @param RenderTransform	Render-only transform of the new child that is applied before the layout transform for rendering purposes only.
	 * @param RenderTransformPivot	Pivot in normalized local space of the Render transform.
	 *
	 * @return					The new child geometry.
	 */
	FGeometry MakeChild( const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform, const FSlateRenderTransform& RenderTransform, const FVector2D& RenderTransformPivot ) const;

	/**
	 * Create a child geometry relative to this one with a given local space size, layout transform, and identity render transform.
	 * For example, a widget with a 5x5 margin will create a geometry for it's child contents having a LayoutTransform of Translate(5,5) and a LocalSize 10 units smaller 
	 * than it's own.
	 *
	 * @param LocalSize			The size of the child geometry in local space.
	 * @param LayoutTransform	Layout transform of the new child relative to this Geometry. Goes from the child's layout space to the this widget's layout space.
	 *
	 * @return					The new child geometry.
	 */
	FGeometry MakeChild( const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform ) const;

	/**
	 * Create a child geometry+widget relative to this one using the given LayoutGeometry.
	 *
	 * @param ChildWidget		The child widget this geometry is being created for.
	 * @param LayoutGeometry	Layout geometry of the child.
	 *
	 * @return					The new child geometry.
	 */
	FArrangedWidget MakeChild( const TSharedRef<SWidget>& ChildWidget, const FLayoutGeometry& LayoutGeometry ) const;

	/**
	 * Create a child geometry+widget relative to this one with a given local space size and layout transform.
	 * The Geometry inherits the child widget's render transform.
	 * For example, a widget with a 5x5 margin will create a geometry for it's child contents having a LayoutTransform of Translate(5,5) and a LocalSize 10 units smaller 
	 * than it's own.
	 *
	 * @param ChildWidget		The child widget this geometry is being created for.
	 * @param LocalSize			The size of the child geometry in local space.
	 * @param LayoutTransform	Layout transform of the new child relative to this Geometry. 
	 *
	 * @return					The new child geometry+widget.
	 */
	FArrangedWidget MakeChild( const TSharedRef<SWidget>& ChildWidget, const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform ) const;

	/**
	 * !!! DEPRECATED FUNCTION !!! Use MakeChild taking a layout transform instead!
	 * Create a child geometry relative to this one with a given local space size and layout transform given by a scale+offset. The Render Transform is identity.
	 *
	 * @param ChildOffset	Offset of the child relative to the parent. Scale+Offset effectively define the layout transform.
	 * @param LocalSize		The size of the child geometry in local space.
	 * @param ChildScale	Scale of the child relative to the parent. Scale+Offset effectively define the layout transform.
	 *
	 * @return				The new child geometry.
	 */
	FGeometry MakeChild( const FVector2D& ChildOffset, const FVector2D& LocalSize, float ChildScale = 1.0f ) const;

	/**
	 * !!! DEPRECATED FUNCTION !!! Use MakeChild taking a layout transform instead!
	 * Create a child geometry+widget relative to this one with a given local space size and layout transform given by a scale+offset.
	 * The Geometry inherits the child widget's render transform.
	 *
	 * @param ChildWidget	The child widget this geometry is being created for.
	 * @param ChildOffset	Offset of the child relative to the parent. Scale+Offset effectively define the layout transform.
	 * @param LocalSize		The size of the child geometry in local space.
	 * @param ChildScale	Scale of the child relative to the parent. Scale+Offset effectively define the layout transform.
	 *
	 * @return				The new child geometry+widget.
	 */
	FArrangedWidget MakeChild( const TSharedRef<SWidget>& ChildWidget, const FVector2D& ChildOffset, const FVector2D& LocalSize, float ChildScale = 1.0f) const;

	/**
	 * Create a paint geometry that represents this geometry.
	 * 
	 * @return	The new paint geometry.
	 */
	FPaintGeometry ToPaintGeometry() const;

	/**
	 * Create a paint geometry relative to this one with a given local space size and layout transform.
	 * The paint geometry inherits the widget's render transform.
	 *
	 * @param LocalSize			The size of the child geometry in local space.
	 * @param LayoutTransform	Layout transform of the paint geometry relative to this Geometry. 
	 *
	 * @return					The new paint geometry derived from this one.
	 */
	FPaintGeometry ToPaintGeometry( const FVector2D& LocalSize, const FSlateLayoutTransform& LayoutTransform) const;

	/**
	 * Create a paint geometry with the same size as this geometry with a given layout transform.
	 * The paint geometry inherits the widget's render transform.
	 *
	 * @param LayoutTransform	Layout transform of the paint geometry relative to this Geometry. 
	 * 
	 * @return					The new paint geometry derived from this one.
	 */
	FPaintGeometry ToPaintGeometry( const FSlateLayoutTransform& LayoutTransform) const;

	/**
	 * !!! DEPRECATED FUNCTION !!! Use ToPaintGeometry taking a layout transform instead!
	 * Create a paint geometry relative to this one with a given local space size and layout transform given as a scale+offset.
	 * The paint geometry inherits the widget's render transform.
	 *
	 * @param LocalOffset	Offset of the paint geometry relative to the parent. Scale+Offset effectively define the layout transform.
	 * @param LocalSize		The size of the paint geometry in local space.
	 * @param LocalScale	Scale of the paint geometry relative to the parent. Scale+Offset effectively define the layout transform.
	 * 
	 * @return				The new paint geometry derived from this one.
	 */
	FPaintGeometry ToPaintGeometry( const FVector2D& LocalOffset, const FVector2D& LocalSize, float LocalScale = 1.0f ) const;

	/**
	 * !!! DEPRECATED FUNCTION !!! Use ToPaintGeometry taking a layout transform instead!
	 * Create a paint geometry relative to this one with the same size size and a given local space offset.
	 * The paint geometry inherits the widget's render transform.
	 *
	 * @param LocalOffset	Offset of the paint geometry relative to the parent. Effectively defines the layout transform.
	 * 
	 * @return				The new paint geometry derived from this one.
	 */
	FPaintGeometry ToOffsetPaintGeometry( const FVector2D& LocalOffset ) const;

	/**
	 * Create a paint geometry relative to this one that whose local space is "inflated" by the specified amount in each direction.
	 * The paint geometry inherits the widget's render transform.
	 *
	 * @param InflateAmount	Amount by which to "inflate" the geometry in each direction around the center point. Effectively defines a layout transform offset and an inflation of the LocalSize.
	 * 
	 * @return				The new paint geometry derived from this one.
	 */
	FPaintGeometry ToInflatedPaintGeometry( const FVector2D& InflateAmount ) const;

	/** 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 * 
	 * @return true if the provided location in absolute coordinates is within the bounds of this geometry. 
	 */
	bool IsUnderLocation( const FVector2D& AbsoluteCoordinate ) const;

	/** 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 * 
	 * @return Transforms AbsoluteCoordinate into the local space of this Geometry. 
	 */
	FVector2D AbsoluteToLocal( FVector2D AbsoluteCoordinate ) const;

	/**
	 * Translates local coordinates into absolute coordinates
	 * 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 * 
	 * @return  Absolute coordinates
	 */
	FVector2D LocalToAbsolute( FVector2D LocalCoordinate ) const;
	
	/**
	 * !!! DEPRECATED !!! This legacy function does not account for render transforms.
	 * 
	 * Returns a clipping rectangle corresponding to the allocated geometry's absolute position and size.
	 * Note that the clipping rectangle starts 1 pixel above and left of the geometry because clipping is not
	 * inclusive on the lower bound.
	 * 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 *
	 * @return  Allotted geometry rectangle in absolute coordinates.
	 */
	FSlateRect GetClippingRect( ) const;
	
	/** @return A String representation of this Geometry */
	FString ToString( ) const;

	/** 
	 * !!! DEPRECATED !!! This legacy function does not account for render transforms.
	 * 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 *
	 * @return the size of the geometry in absolute space */
	FVector2D GetDrawSize() const;

	/** @return the size of the geometry in local space. */
	const FVector2D& GetLocalSize() const { return Size; }

	/** @return the accumulated render transform. Shouldn't be needed in general. */
	const FSlateRenderTransform& GetAccumulatedRenderTransform() const { return AccumulatedRenderTransform; }

	/** @return the accumulated layout transform. Shouldn't be needed in general. */
	FSlateLayoutTransform GetAccumulatedLayoutTransform() const { return FSlateLayoutTransform(Scale, AbsolutePosition); }

	/**
	 * Special case method to append a layout transform to a geometry.
	 * This is used in cases where the FGeometry was arranged in window space
	 * and we need to add the root desktop translation.
	 * If you find yourself wanting to use this function, ask someone if there's a better way.
	 * 
	 * @param LayoutTransform	An additional layout transform to append to this geoemtry.
	 */
	void AppendTransform(const FSlateLayoutTransform& LayoutTransform);
public:
	/** 
	 * 
	 * !!! DEPRECATED !!! Use GetLocalSize() accessor instead of directly accessing public members.
	 * 
	 *	   This member has been made const to prevent mutation.
	 *	   There is no way to easily detect mutation of public members, thus no way to update the render transforms when they are modified.
	 * 
	 * 
	 * Size of the geometry in local space. 
	 */
	const FVector2D /*Local*/Size;

	/** 
	 * !!! DEPRECATED !!! These legacy public members should ideally not be referenced, as they do not account for the render transform.
	 *     FGeometry manipulation should be done in local space as much as possible so logic can be done in aligned local space, but 
	 *     still support arbitrary render transforms.
	 * 
	 *	   This member has been made const to prevent mutation, which would also break render transforms, which are computed during construction.
	 *	   There is no way to easily detect mutation of public members, thus no way to update the render transforms when they are modified.
	 * 
	 * Scale in absolute space. Equivalent to the scale of the accumulated layout transform. 
	 * 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 */
	const float /*Absolute*/Scale;

	/** 
	 * !!! DEPRECATED !!! These legacy public members should ideally not be referenced, as they do not account for the render transform.
	 *     FGeometry manipulation should be done in local space as much as possible so logic can be done in aligned local space, but 
	 *     still support arbitrary render transforms.
	 * 
	 *	   This member has been made const to prevent mutation, which would also break render transforms, which are computed during construction.
	 *	   There is no way to easily detect mutation of public members, thus no way to update the render transforms when they are modified.
	 * 
	 * Position in absolute space. Equivalent to the translation of the accumulated layout transform. 
	 * 
	 * Absolute coordinates could be either desktop or window space depending on what space the root of the widget hierarchy is in.
	 */
	const FVector2D AbsolutePosition;	

	/** 
	 * !!! DEPRECATED !!! 
	 * 
	 * Position of the geometry with respect to its parent in local space. Equivalent to the translation portion of the Local->Parent layout transform.
	 * If you know your children have no additional scale applied to them, you can use this as the Local->Parent layout transform. If your children
	 * DO have additional scale applied, there is no way to determine the actual Local->Parent layout transform, since the scale is accumulated.
	 */
	const FVector2D /*Local*/Position;

private:

	/** Concatenated Render Transform. Actual transform used for rendering.
	 * Formed as: Concat(LocalRenderTransform, LocalLayoutTransform, Parent->AccumulatedRenderTransform) 
	 * 
	 * For rendering, absolute coordinates will always be in window space (relative to the root window).
	 */
	FSlateRenderTransform AccumulatedRenderTransform;
};


template <> struct TIsPODType<FGeometry> { enum { Value = true }; };

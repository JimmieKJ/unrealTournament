// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 */


#pragma once

/** Flush persistent lines */
ENGINE_API void FlushPersistentDebugLines(const UWorld* InWorld);
/** Draw a debug line */
ENGINE_API void DrawDebugLine(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug point */
ENGINE_API void DrawDebugPoint(const UWorld* InWorld, FVector const& Position, float Size, FColor const& PointColor, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw directional arrow **/
ENGINE_API void DrawDebugDirectionalArrow(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, float ArrowSize, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug box */
ENGINE_API void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw a debug box with rotation */
ENGINE_API void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Box, const FQuat& Rotation, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw Debug coordinate system */
ENGINE_API void DrawDebugCoordinateSystem(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw Debug Circle */
ENGINE_API void DrawDebugCircle(const UWorld* InWorld, const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw Debug 2D donut */
ENGINE_API void DrawDebug2DDonut(const UWorld* InWorld, const FMatrix& TransformMatrix, float InnerRadius, float OuterRadius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw a debug sphere */
ENGINE_API void DrawDebugSphere(const UWorld* InWorld, FVector const& Center, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw a debug cylinder */
ENGINE_API void DrawDebugCylinder(const UWorld* InWorld, FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugCone(const UWorld* InWorld, FVector const& Origin, FVector const& Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FColor const& Color, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Used by gameplay when defining a cone by a vertical and horizontal dot products. */
ENGINE_API void DrawDebugAltCone(const UWorld* InWorld, FVector const& Origin, FRotator const& Rotation, float Length, float AngleWidth, float AngleHeight, FColor const& DrawColor, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority=0);
ENGINE_API void DrawDebugString(const UWorld* InWorld, FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor = NULL, FColor const& TextColor = FColor::White, float Duration = -1.000000);
ENGINE_API void DrawDebugFrustum(const UWorld* InWorld, const FMatrix& FrustumToWorld, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw a capsule using the LineBatcher */
ENGINE_API void DrawDebugCapsule(const UWorld* InWorld, FVector const& Center, float HalfHeight, float Radius, const FQuat& Rotation, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0);
/** Draw a debug camera shape.  FOV is full angle in degrees. */
ENGINE_API void DrawDebugCamera(const UWorld* InWorld, FVector const& Location, FRotator const& Rotation, float FOVDeg, float Scale=1.f, FColor const& Color=FColor::White, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void FlushDebugStrings(const UWorld* InWorld);

ENGINE_API void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Box, FColor const& Color, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FQuat const& Rotation, FColor const& Color, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugMesh(const UWorld* InWorld, TArray<FVector> const& Verts, TArray<int32> const& Indices, FColor const& Color, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, float Size, FColor const& Color, bool bPersistent=false, float LifeTime=-1, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, FVector2D const& Extents, FColor const& Color, bool bPersistent=false, float LifeTime=-1, uint8 DepthPriority = 0);

/* Draws a 2D Histogram of size 'DrawSize' based FDebugFloatHistory struct, using DrawTransform for the position in the world. */
ENGINE_API void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FTransform const & DrawTransform, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent = false, float const & LifeTime = -1.f, uint8 const & DepthPriority = 0);

/* Draws a 2D Histogram of size 'DrawSize' based FDebugFloatHistory struct, using DrawLocation for the location in the world, rotation will face camera of first player. */
ENGINE_API void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FVector const & DrawLocation, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent = false, float const & LifeTime = -1.f, uint8 const & DepthPriority = 0);

/**
 * Draws a 2D line on a canvas
 *
 * @param	Canvas		The Canvas to draw on
 * @param	Start		The start of the line in screen space
 * @param	End			The end of the line in screen space
 * @param	LineColor	The color to use for the line
 */

ENGINE_API void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor);

/**
 * Draws a line on a canvas.
 *
 * @param	Canvas		The Canvas to draw on.
 * @param	Start		The start of the line in world space
 * @param	End			The end of the line in world space
 * @param	LineColor	The color to use for the line
 */

ENGINE_API void DrawDebugCanvasLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor);

/**
 * Draws a circle using lines.
 *
 * @param	Canvas		The Canvas to draw on.
 * @param	Base			Center of the circle.
 * @param	X				X alignment axis to draw along.
 * @param	Y				Y alignment axis to draw along.
 * @param	Color			Color of the circle.
 * @param	Radius			Radius of the circle.
 * @param	NumSides		Numbers of sides that the circle has.
 */
ENGINE_API void DrawDebugCanvasCircle(UCanvas* Canvas, const FVector& Base, const FVector& X, const FVector& Y, FColor Color, float Radius, int32 NumSides);

/**
 * Draws a sphere using circles.
 *
 * @param	Canvas		The Canvas to draw on.
 * @param	Base			Center of the sphere.
 * @param	Color			Color of the sphere.
 * @param	Radius			Radius of the sphere.
 * @param	NumSides		Numbers of sides that the circle has.
 */
ENGINE_API void DrawDebugCanvasWireSphere(UCanvas* Canvas, const FVector& Base, FColor Color, float Radius, int32 NumSides);

/**
 * Draws a wireframe cone
 *
 * @param	Canvas			Canvas to draw on
 * @param	Transform		Generic transform to apply (ex. a local-to-world transform).
 * @param	ConeRadius		Radius of the cone.
 * @param	ConeAngle		Angle of the cone.
 * @param	ConeSides		Numbers of sides that the cone has.
 * @param	Color			Color of the cone.
 */
ENGINE_API void DrawDebugCanvasWireCone(UCanvas* Canvas, const FTransform& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, FColor Color);


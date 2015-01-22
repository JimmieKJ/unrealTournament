// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightSceneProxy.h: Point light scene info definition.
=============================================================================*/

#ifndef __PointLightSceneProxy_H__
#define __PointLightSceneProxy_H__
#include "Components/PointLightComponent.h"

/**
 * Compute the screen bounds of a point light along one axis.
 * Based on http://www.gamasutra.com/features/20021011/lengyel_06.htm
 * and http://sourceforge.net/mailarchive/message.php?msg_id=10501105
 */
static bool GetPointLightBounds(
	float LightX,
	float LightZ,
	float Radius,
	const FVector& Axis,
	float AxisSign,
	const FSceneView& View,
	float ViewX,
	float ViewSizeX,
	int32& OutMinX,
	int32& OutMaxX
	)
{
	// Vertical planes: T = <Nx, 0, Nz, 0>
	float Discriminant = (FMath::Square(LightX) - FMath::Square(Radius) + FMath::Square(LightZ)) * FMath::Square(LightZ);
	if(Discriminant >= 0)
	{
		float SqrtDiscriminant = FMath::Sqrt(Discriminant);
		float InvLightSquare = 1.0f / (FMath::Square(LightX) + FMath::Square(LightZ));

		float Nxa = (Radius * LightX - SqrtDiscriminant) * InvLightSquare;
		float Nxb = (Radius * LightX + SqrtDiscriminant) * InvLightSquare;
		float Nza = (Radius - Nxa * LightX) / LightZ;
		float Nzb = (Radius - Nxb * LightX) / LightZ;
		float Pza = LightZ - Radius * Nza;
		float Pzb = LightZ - Radius * Nzb;

		// Tangent a
		if(Pza > 0)
		{
			float Pxa = -Pza * Nza / Nxa;
			FVector4 P = View.ViewMatrices.ProjMatrix.TransformFVector4(FVector4(Axis.X * Pxa,Axis.Y * Pxa,Pza,1));
			float X = (Dot3(P,Axis) / P.W + 1.0f * AxisSign) / 2.0f * AxisSign;
			if(FMath::IsNegativeFloat(Nxa) ^ FMath::IsNegativeFloat(AxisSign))
			{
				OutMaxX = FMath::Min<int64>(FMath::CeilToInt(ViewSizeX * X + ViewX),OutMaxX);
			}
			else
			{
				OutMinX = FMath::Max<int64>(FMath::FloorToInt(ViewSizeX * X + ViewX),OutMinX);
			}
		}

		// Tangent b
		if(Pzb > 0)
		{
			float Pxb = -Pzb * Nzb / Nxb;
			FVector4 P = View.ViewMatrices.ProjMatrix.TransformFVector4(FVector4(Axis.X * Pxb,Axis.Y * Pxb,Pzb,1));
			float X = (Dot3(P,Axis) / P.W + 1.0f * AxisSign) / 2.0f * AxisSign;
			if(FMath::IsNegativeFloat(Nxb) ^ FMath::IsNegativeFloat(AxisSign))
			{
				OutMaxX = FMath::Min<int64>(FMath::CeilToInt(ViewSizeX * X + ViewX),OutMaxX);
			}
			else
			{
				OutMinX = FMath::Max<int64>(FMath::FloorToInt(ViewSizeX * X + ViewX),OutMinX);
			}
		}
	}

	return OutMinX <= OutMaxX;
}

/** The parts of the point light scene info that aren't dependent on the light policy type. */
class FPointLightSceneProxyBase : public FLightSceneProxy
{
public:

	/** The light radius. */
	float Radius;

	/** One over the light's radius. */
	float InvRadius;

	/** The light falloff exponent. */
	float FalloffExponent;

	/** Radius of light source shape */
	float SourceRadius;

	/** Length of light source shape */
	float SourceLength;

	/** Whether light uses inverse squared falloff. */
	const uint32 bInverseSquared : 1;

	/** Initialization constructor. */
	FPointLightSceneProxyBase(const UPointLightComponent* Component)
	:	FLightSceneProxy(Component)
	,	FalloffExponent(Component->LightFalloffExponent)
	,	SourceRadius(Component->SourceRadius)
	,	SourceLength(Component->SourceLength)
	,	bInverseSquared(Component->bUseInverseSquaredFalloff)
	{
		UpdateRadius(Component->AttenuationRadius);
	}

	/**
	* Called on the light scene info after it has been passed to the rendering thread to update the rendering thread's cached info when
	* the light's radius changes.
	*/
	void UpdateRadius_GameThread(UPointLightComponent* Component);

	// FLightSceneInfo interface.

	/** @return radius of the light or 0 if no radius */
	virtual float GetRadius() const 
	{ 
		return Radius; 
	}

	virtual float GetSourceRadius() const 
	{ 
		return SourceRadius; 
	}

	virtual bool IsInverseSquared() const
	{
		return bInverseSquared;
	}

	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const
	{
		if((Bounds.Origin - GetLightToWorld().GetOrigin()).SizeSquared() > FMath::Square(Radius + Bounds.SphereRadius))
		{
			return false;
		}

		if(!FLightSceneProxy::AffectsBounds(Bounds))
		{
			return false;
		}

		return true;
	}

	virtual bool GetScissorRect(FIntRect& ScissorRect, const FSceneView& View) const override
	{
		ScissorRect = View.ViewRect;

		// Calculate a scissor rectangle for the light's radius.
		if((GetLightToWorld().GetOrigin() - View.ViewMatrices.ViewOrigin).Size() > Radius)
		{
			FVector LightVector = View.ViewMatrices.ViewMatrix.TransformPosition(GetLightToWorld().GetOrigin());

			if(!GetPointLightBounds(
				LightVector.X,
				LightVector.Z,
				Radius,
				FVector(+1,0,0),
				+1,
				View,
				View.ViewRect.Min.X,
				View.ViewRect.Width(),
				ScissorRect.Min.X,
				ScissorRect.Max.X))
			{
				return false;
			}

			int32 ScissorMinY = View.ViewRect.Min.Y;
			int32 ScissorMaxY = View.ViewRect.Max.Y;
			if(!GetPointLightBounds(
				LightVector.Y,
				LightVector.Z,
				Radius,
				FVector(0,+1,0),
				-1,
				View,
				View.ViewRect.Min.Y,
				View.ViewRect.Height(),
				ScissorRect.Min.Y,
				ScissorRect.Max.Y))
			{
				return false;
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	virtual void SetScissorRect(FRHICommandList& RHICmdList, const FSceneView& View) const override
	{
		FIntRect ScissorRect;

		if (FPointLightSceneProxyBase::GetScissorRect(ScissorRect, View))
		{
			RHICmdList.SetScissorRect(true, ScissorRect.Min.X, ScissorRect.Min.Y, ScissorRect.Max.X, ScissorRect.Max.Y);
		}
		else
		{
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		}
	}

	virtual bool GetPerObjectProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,class FPerObjectProjectedShadowInitializer& OutInitializer) const
	{
		// Use a perspective projection looking at the primitive from the light position.
		FVector LightPosition = LightToWorld.GetOrigin();
		FVector LightVector = SubjectBounds.Origin - LightPosition;
		float LightDistance = LightVector.Size();
		float SilhouetteRadius = 1.0f;
		const float SubjectRadius = SubjectBounds.SphereRadius;
		const float ShadowRadiusMultiplier = 1.1f;

		if (LightDistance > SubjectRadius)
		{
			SilhouetteRadius = FMath::Min(SubjectRadius * FMath::InvSqrt((LightDistance - SubjectRadius) * (LightDistance + SubjectRadius)), 1.0f);
		}

		if (LightDistance <= SubjectRadius * ShadowRadiusMultiplier)
		{
			// Make the primitive fit in a single < 90 degree FOV projection.
			LightVector = SubjectRadius * LightVector.GetSafeNormal() * ShadowRadiusMultiplier;
			LightPosition = (SubjectBounds.Origin - LightVector );
			LightDistance = SubjectRadius * ShadowRadiusMultiplier;
			SilhouetteRadius = 1.0f;
		}

		OutInitializer.bDirectionalLight = false;
		OutInitializer.PreShadowTranslation = -LightPosition;
		OutInitializer.WorldToLight = FInverseRotationMatrix((LightVector / LightDistance).Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / SilhouetteRadius,1.0f / SilhouetteRadius);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(SubjectBounds.Origin - LightPosition,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
		OutInitializer.WAxis = FVector4(0,0,1,0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		return true;
	}

private:

	/** Updates the light scene info's radius from the component. */
	void UpdateRadius(float ComponentRadius)
	{
		Radius = ComponentRadius;

		// Min to avoid div by 0 (NaN in InvRadius)
		InvRadius = 1.0f / FMath::Max(0.00001f, ComponentRadius);
	}
};

/**
 * The scene info for a point light.
 */
template<typename LightPolicyType>
class TPointLightSceneProxy : public FPointLightSceneProxyBase
{
public:

	/** Initialization constructor. */
	TPointLightSceneProxy(const UPointLightComponent* Component)
		:	FPointLightSceneProxyBase(Component)
	{
	}
};

#endif

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	UniformExpressions.h: Uniform expression definitions.
=============================================================================*/

#pragma once

#include "EnginePrivate.h"
#include "MaterialShared.h"
#include "Materials/MaterialExpressionTextureProperty.h"

/**
 */
class FMaterialUniformExpressionConstant: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionConstant);
public:
	FMaterialUniformExpressionConstant() {}
	FMaterialUniformExpressionConstant(const FLinearColor& InValue,uint8 InValueType):
		Value(InValue),
		ValueType(InValueType)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Value << ValueType;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue = Value;
	}
	virtual bool IsConstant() const
	{
		return true;
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionConstant* OtherConstant = (FMaterialUniformExpressionConstant*)OtherExpression;
		return OtherConstant->ValueType == ValueType && OtherConstant->Value == Value;
	}

private:
	FLinearColor Value;
	uint8 ValueType;
};

/**
 */
class FMaterialUniformExpressionTime: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTime);
public:

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = Context.Time;
		OutValue.G = Context.Time;
		OutValue.B = Context.Time;
		OutValue.A = Context.Time;
	}
	virtual bool IsConstant() const
	{
		return false;
	}
	virtual bool IsChangingPerFrame() const { return true; }
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		return GetType() == OtherExpression->GetType();
	}
};

/**
 */
class FMaterialUniformExpressionRealTime: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionRealTime);
public:

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = Context.RealTime;
		OutValue.G = Context.RealTime;
		OutValue.B = Context.RealTime;
		OutValue.A = Context.RealTime;
	}
	virtual bool IsConstant() const
	{
		return false;
	}
	virtual bool IsChangingPerFrame() const { return true; }
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		return GetType() == OtherExpression->GetType();
	}
};

/**
 */
class FMaterialUniformExpressionVectorParameter: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionVectorParameter);
public:

	FMaterialUniformExpressionVectorParameter() :
		bUseOverriddenDefault(false)
	{}

	FMaterialUniformExpressionVectorParameter(FName InParameterName,const FLinearColor& InDefaultValue):
		ParameterName(InParameterName),
		DefaultValue(InDefaultValue),
		bUseOverriddenDefault(false)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ParameterName << DefaultValue;
	}

	// inefficient compared to GetGameThreadNumberValue(), for editor purpose
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = OutValue.G = OutValue.B = OutValue.A = 0;

		if(!Context.MaterialRenderProxy->GetVectorValue(ParameterName, &OutValue, Context))
		{
			GetDefaultValue(OutValue);
		}
	}

	void GetDefaultValue(FLinearColor& OutValue) const
	{
		OutValue = bUseOverriddenDefault ? OverriddenDefaultValue : DefaultValue;
	}

	// faster than GetNumberValue(), good for run-time use
	void GetGameThreadNumberValue(const UMaterialInterface* SourceMaterialToCopyFrom, FLinearColor& OutValue) const;

	virtual bool IsConstant() const
	{
		return false;
	}

	FName GetParameterName() const
	{
		return ParameterName;
	}

	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionVectorParameter* OtherParameter = (FMaterialUniformExpressionVectorParameter*)OtherExpression;
		return ParameterName == OtherParameter->ParameterName && DefaultValue == OtherParameter->DefaultValue;
	}

	void SetTransientOverrideDefaultValue(const FLinearColor& InOverrideDefaultValue, bool bInUseOverriddenDefault)
	{
		bUseOverriddenDefault = bInUseOverriddenDefault;
		OverriddenDefaultValue = InOverrideDefaultValue;
	}

private:
	FName ParameterName;
	FLinearColor DefaultValue;
	bool bUseOverriddenDefault;
	FLinearColor OverriddenDefaultValue;
};

/**
 */
class FMaterialUniformExpressionScalarParameter: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionScalarParameter);
public:

	FMaterialUniformExpressionScalarParameter() :
		bUseOverriddenDefault(false)
	{}

	FMaterialUniformExpressionScalarParameter(FName InParameterName,float InDefaultValue):
		ParameterName(InParameterName),
		DefaultValue(InDefaultValue),
		bUseOverriddenDefault(false)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ParameterName << DefaultValue;
	}

	// inefficient compared to GetGameThreadNumberValue(), for editor purpose
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		if(Context.MaterialRenderProxy->GetScalarValue(ParameterName, &OutValue.R, Context))
		{
			OutValue.G = OutValue.B = OutValue.A = OutValue.R;
		}
		else
		{
			GetDefaultValue(OutValue.A);
			OutValue.R = OutValue.G = OutValue.B = OutValue.A;
		}
	}

	void GetDefaultValue(float& OutValue) const
	{
		OutValue = bUseOverriddenDefault ? OverriddenDefaultValue : DefaultValue;
	}
	
	// faster than GetNumberValue(), good for run-time use
	void GetGameThreadNumberValue(const UMaterialInterface* SourceMaterialToCopyFrom, float& OutValue) const;

	virtual bool IsConstant() const
	{
		return false;
	}

	FName GetParameterName() const
	{
		return ParameterName;
	}

	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionScalarParameter* OtherParameter = (FMaterialUniformExpressionScalarParameter*)OtherExpression;
		return ParameterName == OtherParameter->ParameterName && DefaultValue == OtherParameter->DefaultValue;
	}

	void SetTransientOverrideDefaultValue(float InOverrideDefaultValue, bool bInUseOverriddenDefault)
	{
		bUseOverriddenDefault = bInUseOverriddenDefault;
		OverriddenDefaultValue = InOverrideDefaultValue;
	}

private:
	FName ParameterName;
	float DefaultValue;
	bool bUseOverriddenDefault;
	float OverriddenDefaultValue;
};

/** @return The texture that was associated with the given index when the given material had its uniform expressions/HLSL code generated. */
static UTexture* GetIndexedTexture(const FMaterial& Material, int32 TextureIndex)
{
	const TArray<UTexture*>& ReferencedTextures = Material.GetReferencedTextures();
	UTexture* IndexedTexture = NULL;

	if (ReferencedTextures.IsValidIndex(TextureIndex))
	{
		IndexedTexture = ReferencedTextures[TextureIndex];
	}
	else
	{
		static bool bWarnedOnce = false;
		if (!bWarnedOnce)
		{
			UE_LOG(LogMaterial, Warning, TEXT("FMaterialUniformExpressionTexture had invalid TextureIndex! (%u / %u)"), TextureIndex, ReferencedTextures.Num());
			bWarnedOnce = true;
		}
	}
	return IndexedTexture;
}

/**
 * A texture parameter expression.
 */
class FMaterialUniformExpressionTextureParameter: public FMaterialUniformExpressionTexture
{
	typedef FMaterialUniformExpressionTexture Super;
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureParameter);
public:

	FMaterialUniformExpressionTextureParameter() {}

	FMaterialUniformExpressionTextureParameter(FName InParameterName, int32 InTextureIndex, ESamplerSourceMode InSourceMode) :
		Super(InTextureIndex, InSourceMode),
		ParameterName(InParameterName)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << ParameterName;
		Super::Serialize(Ar);
	}
	virtual void GetTextureValue(const FMaterialRenderContext& Context,const FMaterial& Material,const UTexture*& OutValue,ESamplerSourceMode& OutSamplerSource) const
	{
		check(IsInParallelRenderingThread());
		OutSamplerSource = SamplerSource;
		if( TransientOverrideValue_RenderThread != NULL )
		{
			OutValue = TransientOverrideValue_RenderThread;
		}
		else
		{
			OutValue = NULL;
			if(!Context.MaterialRenderProxy->GetTextureValue(ParameterName,&OutValue,Context))
			{
				OutValue = GetIndexedTexture(Material, TextureIndex);
			}
		}
	}
	virtual void GetGameThreadTextureValue(const UMaterialInterface* MaterialInterface,const FMaterial& Material,UTexture*& OutValue,bool bAllowOverride=true) const
	{
		check(IsInGameThread());
		if( bAllowOverride && TransientOverrideValue_GameThread != NULL )
		{
			OutValue = TransientOverrideValue_GameThread;
		}
		else
		{
			OutValue = NULL;
			if(!MaterialInterface->GetTextureParameterOverrideValue(ParameterName,OutValue))
			{
				OutValue = GetIndexedTexture(Material, TextureIndex);
			}
		}
	}

	virtual bool IsConstant() const
	{
		return false;
	}

	FName GetParameterName() const
	{
		return ParameterName;
	}

	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionTextureParameter* OtherParameter = (FMaterialUniformExpressionTextureParameter*)OtherExpression;
		return ParameterName == OtherParameter->ParameterName && Super::IsIdentical(OtherParameter);
	}

private:
	FName ParameterName;
};

/**
 * A flipbook texture parameter expression.
 */
class FMaterialUniformExpressionFlipBookTextureParameter : public FMaterialUniformExpressionTexture
{
	typedef FMaterialUniformExpressionTexture Super;
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFlipBookTextureParameter);
public:

	FMaterialUniformExpressionFlipBookTextureParameter() {}

	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		OutValue.R = OutValue.G = OutValue.B = OutValue.A = 0;
	}

	virtual bool IsConstant() const
	{
		return false;
	}
};

/**
 */
class FMaterialUniformExpressionSine: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSine);
public:

	FMaterialUniformExpressionSine() {}
	FMaterialUniformExpressionSine(FMaterialUniformExpression* InX,bool bInIsCosine):
		X(InX),
		bIsCosine(bInIsCosine)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X << bIsCosine;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = bIsCosine ? FMath::Cos(ValueX.R) : FMath::Sin(ValueX.R);
		OutValue.G = bIsCosine ? FMath::Cos(ValueX.G) : FMath::Sin(ValueX.G);
		OutValue.B = bIsCosine ? FMath::Cos(ValueX.B) : FMath::Sin(ValueX.B);
		OutValue.A = bIsCosine ? FMath::Cos(ValueX.A) : FMath::Sin(ValueX.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionSine* OtherSine = (FMaterialUniformExpressionSine*)OtherExpression;
		return X->IsIdentical(OtherSine->X) && bIsCosine == OtherSine->bIsCosine;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
	bool bIsCosine;
};

/**
 */
class FMaterialUniformExpressionSquareRoot: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSquareRoot);
public:

	FMaterialUniformExpressionSquareRoot() {}
	FMaterialUniformExpressionSquareRoot(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = FMath::Sqrt(ValueX.R);
		OutValue.G = FMath::Sqrt(ValueX.G);
		OutValue.B = FMath::Sqrt(ValueX.B);
		OutValue.A = FMath::Sqrt(ValueX.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionSquareRoot* OtherSqrt = (FMaterialUniformExpressionSquareRoot*)OtherExpression;
		return X->IsIdentical(OtherSqrt->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionLength: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLength);
public:

	FMaterialUniformExpressionLength() {}
	FMaterialUniformExpressionLength(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = OutValue.G = OutValue.B = FMath::Sqrt(ValueX.R * ValueX.R + ValueX.G * ValueX.G + ValueX.B * ValueX.B);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionLength* OtherSqrt = (FMaterialUniformExpressionLength*)OtherExpression;
		return X->IsIdentical(OtherSqrt->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionLogarithm2: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionLogarithm2);
public:

	FMaterialUniformExpressionLogarithm2() {}
	FMaterialUniformExpressionLogarithm2(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	void Serialize(FArchive& Ar) override
	{
		Ar << X;
	}
	void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const override
	{
		FLinearColor ValueX = FLinearColor::Black;
		X->GetNumberValue(Context,ValueX);
		OutValue.R = FMath::Log2(ValueX.R);
		OutValue.G = FMath::Log2(ValueX.G);
		OutValue.B = FMath::Log2(ValueX.B);
		OutValue.A = FMath::Log2(ValueX.A);
	}
	bool IsConstant() const override
	{
		return X->IsConstant();
	}
	bool IsChangingPerFrame() const override
	{
		return X->IsChangingPerFrame();
	}
	bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const override
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}

		auto OtherLog = static_cast<const FMaterialUniformExpressionLogarithm2 *>(OtherExpression);
		return X->IsIdentical(OtherLog->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
enum EFoldedMathOperation
{
	FMO_Add,
	FMO_Sub,
	FMO_Mul,
	FMO_Div,
	FMO_Dot
};

/** Converts an arbitrary number into a safe divisor. i.e. FMath::Abs(Number) >= DELTA */
static float GetSafeDivisor(float Number)
{
	if(FMath::Abs(Number) < DELTA)
	{
		if(Number < 0.0f)
		{
			return -DELTA;
		}
		else
		{
			return +DELTA;
		}
	}
	else
	{
		return Number;
	}
}

class FMaterialUniformExpressionFoldedMath: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFoldedMath);
public:

	FMaterialUniformExpressionFoldedMath() {}
	FMaterialUniformExpressionFoldedMath(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB,uint8 InOp):
		A(InA),
		B(InB),
		Op(InOp)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B << Op;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		switch(Op)
		{
			case FMO_Add: OutValue = ValueA + ValueB; break;
			case FMO_Sub: OutValue = ValueA - ValueB; break;
			case FMO_Mul: OutValue = ValueA * ValueB; break;
			case FMO_Div: 
				OutValue.R = ValueA.R / GetSafeDivisor(ValueB.R);
				OutValue.G = ValueA.G / GetSafeDivisor(ValueB.G);
				OutValue.B = ValueA.B / GetSafeDivisor(ValueB.B);
				OutValue.A = ValueA.A / GetSafeDivisor(ValueB.A);
				break;
			case FMO_Dot: 
				{
					float DotProduct = ValueA.R * ValueB.R + ValueA.G * ValueB.G + ValueA.B * ValueB.B + ValueA.A * ValueB.A;
					OutValue.R = OutValue.G = OutValue.B = OutValue.A = DotProduct;
				}
				break;
			default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown folded math operation: %08x"),(int32)Op);
		};
	}
	virtual bool IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		const bool AChangesPerFrame = A->IsChangingPerFrame();
		const bool BChangesPerFrame = B->IsChangingPerFrame();
		return AChangesPerFrame || BChangesPerFrame;
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionFoldedMath* OtherMath = (FMaterialUniformExpressionFoldedMath*)OtherExpression;
		return A->IsIdentical(OtherMath->A) && B->IsIdentical(OtherMath->B) && Op == OtherMath->Op;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
	uint8 Op;
};

/**
 * A hint that only the fractional part of this expession's value matters.
 */
class FMaterialUniformExpressionPeriodic: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionPeriodic);
public:

	FMaterialUniformExpressionPeriodic() {}
	FMaterialUniformExpressionPeriodic(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor TempValue = FLinearColor::Black;
		X->GetNumberValue(Context,TempValue);

		OutValue.R = FMath::Fractional(TempValue.R);
		OutValue.G = FMath::Fractional(TempValue.G);
		OutValue.B = FMath::Fractional(TempValue.B);
		OutValue.A = FMath::Fractional(TempValue.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionPeriodic* OtherPeriodic = (FMaterialUniformExpressionPeriodic*)OtherExpression;
		return X->IsIdentical(OtherPeriodic->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionAppendVector: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAppendVector);
public:

	FMaterialUniformExpressionAppendVector() {}
	FMaterialUniformExpressionAppendVector(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB,uint32 InNumComponentsA):
		A(InA),
		B(InB),
		NumComponentsA(InNumComponentsA)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B << NumComponentsA;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = NumComponentsA >= 1 ? ValueA.R : (&ValueB.R)[0 - NumComponentsA];
		OutValue.G = NumComponentsA >= 2 ? ValueA.G : (&ValueB.R)[1 - NumComponentsA];
		OutValue.B = NumComponentsA >= 3 ? ValueA.B : (&ValueB.R)[2 - NumComponentsA];
		OutValue.A = NumComponentsA >= 4 ? ValueA.A : (&ValueB.R)[3 - NumComponentsA];
	}
	virtual bool IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		const bool AChangesPerFrame = A->IsChangingPerFrame();
		const bool BChangesPerFrame = B->IsChangingPerFrame();
		return AChangesPerFrame || BChangesPerFrame;
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionAppendVector* OtherAppend = (FMaterialUniformExpressionAppendVector*)OtherExpression;
		return A->IsIdentical(OtherAppend->A) && B->IsIdentical(OtherAppend->B) && NumComponentsA == OtherAppend->NumComponentsA;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
	uint32 NumComponentsA;
};

/**
 */
class FMaterialUniformExpressionMin: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMin);
public:

	FMaterialUniformExpressionMin() {}
	FMaterialUniformExpressionMin(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB):
		A(InA),
		B(InB)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = FMath::Min(ValueA.R, ValueB.R);
		OutValue.G = FMath::Min(ValueA.G, ValueB.G);
		OutValue.B = FMath::Min(ValueA.B, ValueB.B);
		OutValue.A = FMath::Min(ValueA.A, ValueB.A);
	}
	virtual bool IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		const bool AChangesPerFrame = A->IsChangingPerFrame();
		const bool BChangesPerFrame = B->IsChangingPerFrame();
		return AChangesPerFrame || BChangesPerFrame;
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionMin* OtherMin = (FMaterialUniformExpressionMin*)OtherExpression;
		return A->IsIdentical(OtherMin->A) && B->IsIdentical(OtherMin->B);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
};

/**
 */
class FMaterialUniformExpressionMax: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionMax);
public:

	FMaterialUniformExpressionMax() {}
	FMaterialUniformExpressionMax(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB):
		A(InA),
		B(InB)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = FMath::Max(ValueA.R, ValueB.R);
		OutValue.G = FMath::Max(ValueA.G, ValueB.G);
		OutValue.B = FMath::Max(ValueA.B, ValueB.B);
		OutValue.A = FMath::Max(ValueA.A, ValueB.A);
	}
	virtual bool IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		const bool AChangesPerFrame = A->IsChangingPerFrame();
		const bool BChangesPerFrame = B->IsChangingPerFrame();
		return AChangesPerFrame || BChangesPerFrame;
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionMax* OtherMax = (FMaterialUniformExpressionMax*)OtherExpression;
		return A->IsIdentical(OtherMax->A) && B->IsIdentical(OtherMax->B);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
};

/**
 */
class FMaterialUniformExpressionClamp: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionClamp);
public:

	FMaterialUniformExpressionClamp() {}
	FMaterialUniformExpressionClamp(FMaterialUniformExpression* InInput,FMaterialUniformExpression* InMin,FMaterialUniformExpression* InMax):
		Input(InInput),
		Min(InMin),
		Max(InMax)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Input << Min << Max;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueMin = FLinearColor::Black;
		FLinearColor ValueMax = FLinearColor::Black;
		FLinearColor ValueInput = FLinearColor::Black;
		Min->GetNumberValue(Context, ValueMin);
		Max->GetNumberValue(Context, ValueMax);
		Input->GetNumberValue(Context, ValueInput);

		OutValue.R = FMath::Clamp(ValueInput.R, ValueMin.R, ValueMax.R);
		OutValue.G = FMath::Clamp(ValueInput.G, ValueMin.G, ValueMax.G);
		OutValue.B = FMath::Clamp(ValueInput.B, ValueMin.B, ValueMax.B);
		OutValue.A = FMath::Clamp(ValueInput.A, ValueMin.A, ValueMax.A);
	}
	virtual bool IsConstant() const
	{
		return Input->IsConstant() && Min->IsConstant() && Max->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return Input->IsChangingPerFrame() || Min->IsChangingPerFrame() || Max->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionClamp* OtherClamp = (FMaterialUniformExpressionClamp*)OtherExpression;
		return Input->IsIdentical(OtherClamp->Input) && Min->IsIdentical(OtherClamp->Min) && Max->IsIdentical(OtherClamp->Max);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> Input;
	TRefCountPtr<FMaterialUniformExpression> Min;
	TRefCountPtr<FMaterialUniformExpression> Max;
};

/**
 */
class FMaterialUniformExpressionSaturate: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionSaturate);
public:

	FMaterialUniformExpressionSaturate() {}
	FMaterialUniformExpressionSaturate(FMaterialUniformExpression* InInput):
		Input(InInput)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Input;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueInput = FLinearColor::Black;
		Input->GetNumberValue(Context, ValueInput);

		OutValue.R = FMath::Clamp<float>(ValueInput.R, 0, 1);
		OutValue.G = FMath::Clamp<float>(ValueInput.G, 0, 1);
		OutValue.B = FMath::Clamp<float>(ValueInput.B, 0, 1);
		OutValue.A = FMath::Clamp<float>(ValueInput.A, 0, 1);
	}
	virtual bool IsConstant() const
	{
		return Input->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return Input->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionSaturate* OtherClamp = (FMaterialUniformExpressionSaturate*)OtherExpression;
		return Input->IsIdentical(OtherClamp->Input);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> Input;
};

class FMaterialUniformExpressionComponentSwizzle : public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionComponentSwizzle);
public:

	FMaterialUniformExpressionComponentSwizzle() {}
	FMaterialUniformExpressionComponentSwizzle(FMaterialUniformExpression* InX, int8 InR, int8 InG, int8 InB, int8 InA) :
		X(InX),
		IndexR(InR),
		IndexG(InG),
		IndexB(InB),
		IndexA(InA)
	{
		NumElements = 0;
		if (InA >= 0)
		{
			check(InA <= 3);
			++NumElements;
			check(InB >= 0);
		}

		if (InB >= 0)
		{
			check(InB <= 3);
			++NumElements;
			check(InG >= 0);
		}

		if (InG >= 0)
		{
			check(InG <= 3);
			++NumElements;
		}

		// At least one proper index
		check(InR >= 0 && InR <= 3);
		++NumElements;

		if (NumElements == 1)
		{
			// Replicate scalar
			IndexG = IndexR;
			IndexB = IndexR;
			IndexA = IndexR;
			NumElements = 4;
		}
	}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
		Ar << IndexR;
		Ar << IndexG;
		Ar << IndexB;
		Ar << IndexA;
		Ar << NumElements;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context, FLinearColor& OutValue) const
	{
		FLinearColor Temp = OutValue;
		X->GetNumberValue(Context, Temp);
		// Clear
		OutValue *= 0;
		switch (NumElements)
		{
		case 4:
			OutValue.A = Temp.Component(IndexA);
			// Fallthrough...
		case 3:
			OutValue.B = Temp.Component(IndexB);
			// Fallthrough...
		case 2:
			OutValue.G = Temp.Component(IndexG);
			// Fallthrough...
		case 1:
			OutValue.R = Temp.Component(IndexR);
			break;
		default: UE_LOG(LogMaterial, Fatal, TEXT("Invalid number of swizzle elements: %d"), NumElements);
		}
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		auto* OtherSwizzle = (FMaterialUniformExpressionComponentSwizzle*)OtherExpression;
		return X->IsIdentical(OtherSwizzle->X) &&
			NumElements == OtherSwizzle->NumElements &&
			IndexR == OtherSwizzle->IndexR &&
			IndexG == OtherSwizzle->IndexG &&
			IndexB == OtherSwizzle->IndexB &&
			IndexA == OtherSwizzle->IndexA;
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
	int8 IndexR;
	int8 IndexG;
	int8 IndexB;
	int8 IndexA;
	int8 NumElements;
};

/**
 */
class FMaterialUniformExpressionFloor: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFloor);
public:

	FMaterialUniformExpressionFloor() {}
	FMaterialUniformExpressionFloor(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);

		OutValue.R = FMath::FloorToInt(OutValue.R);
		OutValue.G = FMath::FloorToInt(OutValue.G);
		OutValue.B = FMath::FloorToInt(OutValue.B);
		OutValue.A = FMath::FloorToInt(OutValue.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionFloor* OtherFloor = (FMaterialUniformExpressionFloor*)OtherExpression;
		return X->IsIdentical(OtherFloor->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionCeil: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionCeil);
public:

	FMaterialUniformExpressionCeil() {}
	FMaterialUniformExpressionCeil(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);

		OutValue.R = FMath::CeilToInt(OutValue.R);
		OutValue.G = FMath::CeilToInt(OutValue.G);
		OutValue.B = FMath::CeilToInt(OutValue.B);
		OutValue.A = FMath::CeilToInt(OutValue.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionCeil* OtherCeil = (FMaterialUniformExpressionCeil*)OtherExpression;
		return X->IsIdentical(OtherCeil->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionFrac: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFrac);
public:

	FMaterialUniformExpressionFrac() {}
	FMaterialUniformExpressionFrac(FMaterialUniformExpression* InX):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);

		OutValue.R = OutValue.R - FMath::FloorToInt(OutValue.R);
		OutValue.G = OutValue.G - FMath::FloorToInt(OutValue.G);
		OutValue.B = OutValue.B - FMath::FloorToInt(OutValue.B);
		OutValue.A = OutValue.A - FMath::FloorToInt(OutValue.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionFrac* OtherFrac = (FMaterialUniformExpressionFrac*)OtherExpression;
		return X->IsIdentical(OtherFrac->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionFmod : public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionFmod);
public:

	FMaterialUniformExpressionFmod() {}
	FMaterialUniformExpressionFmod(FMaterialUniformExpression* InA,FMaterialUniformExpression* InB):
		A(InA),
		B(InB)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << A << B;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		FLinearColor ValueA = FLinearColor::Black;
		FLinearColor ValueB = FLinearColor::Black;
		A->GetNumberValue(Context, ValueA);
		B->GetNumberValue(Context, ValueB);

		OutValue.R = FMath::Fmod(ValueA.R, ValueB.R);
		OutValue.G = FMath::Fmod(ValueA.G, ValueB.G);
		OutValue.B = FMath::Fmod(ValueA.B, ValueB.B);
		OutValue.A = FMath::Fmod(ValueA.A, ValueB.A);
	}
	virtual bool IsConstant() const
	{
		return A->IsConstant() && B->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		const bool AChangesPerFrame = A->IsChangingPerFrame();
		const bool BChangesPerFrame = B->IsChangingPerFrame();
		return AChangesPerFrame || BChangesPerFrame;
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionFmod* OtherMax = (FMaterialUniformExpressionFmod*)OtherExpression;
		return A->IsIdentical(OtherMax->A) && B->IsIdentical(OtherMax->B);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> A;
	TRefCountPtr<FMaterialUniformExpression> B;
};

/**
 * Absolute value evaluator for a given input expression
 */
class FMaterialUniformExpressionAbs: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionAbs);
public:

	FMaterialUniformExpressionAbs() {}
	FMaterialUniformExpressionAbs( FMaterialUniformExpression* InX ):
		X(InX)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar)
	{
		Ar << X;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const
	{
		X->GetNumberValue(Context, OutValue);
		OutValue.R = FMath::Abs<float>(OutValue.R);
		OutValue.G = FMath::Abs<float>(OutValue.G);
		OutValue.B = FMath::Abs<float>(OutValue.B);
		OutValue.A = FMath::Abs<float>(OutValue.A);
	}
	virtual bool IsConstant() const
	{
		return X->IsConstant();
	}
	virtual bool IsChangingPerFrame() const
	{
		return X->IsChangingPerFrame();
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}
		FMaterialUniformExpressionAbs* OtherAbs = (FMaterialUniformExpressionAbs*)OtherExpression;
		return X->IsIdentical(OtherAbs->X);
	}

private:
	TRefCountPtr<FMaterialUniformExpression> X;
};

/**
 */
class FMaterialUniformExpressionTextureProperty: public FMaterialUniformExpression
{
	DECLARE_MATERIALUNIFORMEXPRESSION_TYPE(FMaterialUniformExpressionTextureProperty);
public:
	
	FMaterialUniformExpressionTextureProperty() {}
	FMaterialUniformExpressionTextureProperty(FMaterialUniformExpressionTexture* InTextureExpression, EMaterialExposedTextureProperty InTextureProperty)
		: TextureExpression(InTextureExpression)
		, TextureProperty(InTextureProperty)
	{}

	// FMaterialUniformExpression interface.
	virtual void Serialize(FArchive& Ar) override
	{
		Ar << TextureExpression << TextureProperty;
	}
	virtual void GetNumberValue(const FMaterialRenderContext& Context,FLinearColor& OutValue) const override
	{
		const UTexture* Texture;

		{
			ESamplerSourceMode SamplerSource;
			TextureExpression->GetTextureValue(Context, Context.Material, Texture, SamplerSource);
		}

		if (!Texture || !Texture->Resource)
		{
			return;
		}
	
		if (TextureProperty == TMTM_TextureSize)
		{
			OutValue.R = Texture->Resource->GetSizeX();
			OutValue.G = Texture->Resource->GetSizeY();
		}
		else if (TextureProperty == TMTM_TexelSize)
		{
			OutValue.R = 1.0f / float(Texture->Resource->GetSizeX());
			OutValue.G = 1.0f / float(Texture->Resource->GetSizeY());
		}
		else
		{
			check(0);
		}
	}
	virtual bool IsIdentical(const FMaterialUniformExpression* OtherExpression) const override
	{
		if (GetType() != OtherExpression->GetType())
		{
			return false;
		}

		auto OtherTexturePropertyExpression = (const FMaterialUniformExpressionTextureProperty*)OtherExpression;
		
		if (TextureProperty != OtherTexturePropertyExpression->TextureProperty)
		{
			return false;
		}

		return TextureExpression->IsIdentical(OtherTexturePropertyExpression->TextureExpression);
	}
	
private:
	TRefCountPtr<FMaterialUniformExpressionTexture> TextureExpression;
	int8 TextureProperty;
};

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "Blueprint/BlueprintSupport.h"
#include "Kismet/KismetMathLibrary.h"
#include "CoreStats.h"
#include "Math/DualQuat.h"

#define LOCTEXT_NAMESPACE "UKismetMathLibrary"

/** Interpolate a linear alpha value using an ease mode and function. */
float EaseAlpha(float InAlpha, uint8 EasingFunc, float BlendExp, int32 Steps)
{
	switch (EasingFunc)
	{
	case EEasingFunc::Step:					return FMath::InterpStep<float>(0.f, 1.f, InAlpha, Steps);
	case EEasingFunc::SinusoidalIn:			return FMath::InterpSinIn<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::SinusoidalOut:		return FMath::InterpSinOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::SinusoidalInOut:		return FMath::InterpSinInOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::EaseIn:				return FMath::InterpEaseIn<float>(0.f, 1.f, InAlpha, BlendExp);
	case EEasingFunc::EaseOut:				return FMath::InterpEaseOut<float>(0.f, 1.f, InAlpha, BlendExp);
	case EEasingFunc::EaseInOut:			return FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, BlendExp);
	case EEasingFunc::ExpoIn:				return FMath::InterpExpoIn<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::ExpoOut:				return FMath::InterpExpoOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::ExpoInOut:			return FMath::InterpExpoInOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::CircularIn:			return FMath::InterpCircularIn<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::CircularOut:			return FMath::InterpCircularOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::CircularInOut:		return FMath::InterpCircularInOut<float>(0.f, 1.f, InAlpha);
	}
	return InAlpha;
}

const FName DivideByZeroWarning = FName("DivideByZeroWarning");
const FName NegativeSqrtWarning = FName("NegativeSqrtWarning");
const FName ZeroLengthProjectionWarning = FName("ZeroLengthProjectionWarning");
const FName InvalidDateWarning = FName("InvalidDateWarning");

UKismetMathLibrary::UKismetMathLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			DivideByZeroWarning,
			LOCTEXT("DivideByZeroWarning", "Divide by zero")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			NegativeSqrtWarning,
			LOCTEXT("NegativeSqrtWarning", "Square root of negative number")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			ZeroLengthProjectionWarning,
			LOCTEXT("ZeroLengthProjectionWarning", "Projection onto vector of zero length")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			InvalidDateWarning,
			LOCTEXT("InvalidDateWarning", "Invalid date warning")
		)
	);
}

bool UKismetMathLibrary::RandomBool()
{
	return FMath::RandBool();
}

bool UKismetMathLibrary::RandomBoolWithWeight(float Weight)
{
	//If the Weight equals to 0.0f then always return false
	if (Weight <= 0.0f)
	{
		return false;
	}
	else
	{
		//If the Weight is higher or equal to the random number then return true
		return Weight >= FMath::FRandRange(0.0f, 1.0f);
	}

}

bool UKismetMathLibrary::RandomBoolWithWeightFromStream(float Weight, const FRandomStream& RandomStream)
{
	//If the Weight equals to 0.0f then always return false
	if (Weight <= 0.0f)
	{
		return false;
	}
	else
	{
		//Create the random float from the specified stream
		float Number = UKismetMathLibrary::RandomFloatFromStream(RandomStream);

		//If the Weight is higher or equal to number generated from stream then return true
		return Weight >= Number;
	}

}

bool UKismetMathLibrary::Not_PreBool(bool A)
{
	return !A;
}

bool UKismetMathLibrary::EqualEqual_BoolBool(bool A, bool B)
{
	return ((!A) == (!B));
}

bool UKismetMathLibrary::NotEqual_BoolBool(bool A, bool B)
{
	return ((!A) != (!B));
}

bool UKismetMathLibrary::BooleanAND(bool A, bool B)
{
	return A && B;
}

bool UKismetMathLibrary::BooleanNAND(bool A, bool B)
{
	return !(A && B);
}

bool UKismetMathLibrary::BooleanOR(bool A, bool B)
{
	return A || B;
}

bool UKismetMathLibrary::BooleanXOR(bool A, bool B)
{
	return A ^ B;
}

bool UKismetMathLibrary::BooleanNOR(bool A, bool B)
{
	return !(A || B);
}

uint8 UKismetMathLibrary::Multiply_ByteByte(uint8 A, uint8 B)
{
	return A * B;
}

uint8 UKismetMathLibrary::Divide_ByteByte(uint8 A, uint8 B)
{
	if (B == 0)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_ByteByte"), ELogVerbosity::Warning, DivideByZeroWarning);
	}

	return (B != 0) ? (A / B) : 0;
}

uint8 UKismetMathLibrary::Percent_ByteByte(uint8 A, uint8 B)
{
	if (B == 0)
	{
		FFrame::KismetExecutionMessage(TEXT("Modulo by zero"), ELogVerbosity::Warning, DivideByZeroWarning);
	}

	return (B != 0) ? (A % B) : 0;
}

uint8 UKismetMathLibrary::Add_ByteByte(uint8 A, uint8 B)
{
	return A + B;
}

uint8 UKismetMathLibrary::Subtract_ByteByte(uint8 A, uint8 B)
{
	return A - B;
}

uint8 UKismetMathLibrary::BMin(uint8 A, uint8 B)
{
	return FMath::Min<uint8>(A, B);
}

uint8 UKismetMathLibrary::BMax(uint8 A, uint8 B)
{
	return FMath::Max<uint8>(A, B);
}
 
bool UKismetMathLibrary::Less_ByteByte(uint8 A, uint8 B)
{
	return A < B;
}

bool UKismetMathLibrary::Greater_ByteByte(uint8 A, uint8 B)
{
	return A > B;
}

bool UKismetMathLibrary::LessEqual_ByteByte(uint8 A, uint8 B)
{
	return A <= B;
}

bool UKismetMathLibrary::GreaterEqual_ByteByte(uint8 A, uint8 B)
{
	return A >= B;
}

bool UKismetMathLibrary::EqualEqual_ByteByte(uint8 A, uint8 B)
{
	return A == B;
}

bool UKismetMathLibrary::NotEqual_ByteByte(uint8 A, uint8 B)
{
	return A != B;
}

int32 UKismetMathLibrary::Multiply_IntInt(int32 A, int32 B)
{
	return A * B;
}

int32 UKismetMathLibrary::Divide_IntInt(int32 A, int32 B)
{
	if (B == 0)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_IntInt"), ELogVerbosity::Warning, DivideByZeroWarning);
	}

	return (B != 0) ? (A / B) : 0;
}

int32 UKismetMathLibrary::Percent_IntInt(int32 A, int32 B)
{
	if (B == 0)
	{
		FFrame::KismetExecutionMessage(TEXT("Modulo by zero"), ELogVerbosity::Warning, DivideByZeroWarning);
	}

	return (B != 0) ? (A % B) : 0;
}

int32 UKismetMathLibrary::Add_IntInt(int32 A, int32 B)
{
	return A + B;
}

int32 UKismetMathLibrary::Subtract_IntInt(int32 A, int32 B)
{
	return A - B;
}

bool UKismetMathLibrary::Less_IntInt(int32 A, int32 B)
{
	return A < B;
}

bool UKismetMathLibrary::Greater_IntInt(int32 A, int32 B)
{
	return A > B;
}

bool UKismetMathLibrary::LessEqual_IntInt(int32 A, int32 B)
{
	return A <= B;
}

bool UKismetMathLibrary::GreaterEqual_IntInt(int32 A, int32 B)
{
	return A >= B;
}

bool UKismetMathLibrary::EqualEqual_IntInt(int32 A, int32 B)
{
	return A == B;
}

bool UKismetMathLibrary::NotEqual_IntInt(int32 A, int32 B)
{
	return A != B;
}

int32 UKismetMathLibrary::And_IntInt(int32 A, int32 B)
{
	return A & B;
}

int32 UKismetMathLibrary::Xor_IntInt(int32 A, int32 B)
{
	return A ^ B;
}

int32 UKismetMathLibrary::Or_IntInt(int32 A, int32 B)
{
	return A | B;
}

int32 UKismetMathLibrary::Not_Int(int32 A)
{
	return ~A;
}

int32 UKismetMathLibrary::SignOfInteger(int32 A)
{
	return FMath::Sign<int32>(A);
}

int32 UKismetMathLibrary::RandomInteger(int32 A)
{
	return FMath::RandHelper(A);
}

int32 UKismetMathLibrary::RandomIntegerInRange(int32 Min, int32 Max)
{
	return FMath::RandRange(Min, Max);
}

int32 UKismetMathLibrary::Min(int32 A, int32 B)
{
	return FMath::Min(A, B);
}

int32 UKismetMathLibrary::Max(int32 A, int32 B)
{
	return FMath::Max(A, B);
}

int32 UKismetMathLibrary::Clamp(int32 V, int32 A, int32 B)
{
	return FMath::Clamp(V, A, B);
}

int32 UKismetMathLibrary::Abs_Int(int32 A)
{
	return FMath::Abs(A);
}


float UKismetMathLibrary::MultiplyMultiply_FloatFloat(float Base, float Exp)
{
	return FMath::Pow(Base, Exp);
}	

float UKismetMathLibrary::Multiply_FloatFloat(float A, float B)
{
	return A * B;
}	

float UKismetMathLibrary::Multiply_IntFloat(int32 A, float B)
{
	return A * B;
}	

/* This function is custom thunked, the real function is GenericDivide_FloatFloat */
float UKismetMathLibrary::Divide_FloatFloat(float A, float B)
{
	check(0);
	return 0;
}

float UKismetMathLibrary::GenericDivide_FloatFloat(float A, float B)
{
	return A / B;
}	

/* This function is custom thunked, the real function is GenericPercent_FloatFloat */
float UKismetMathLibrary::Percent_FloatFloat(float A, float B)
{
	check(0);
	return 0;
}

float UKismetMathLibrary::GenericPercent_FloatFloat(float A, float B)
{
	return (B != 0.f) ? FMath::Fmod(A, B) : 0.f;
}	

float UKismetMathLibrary::Fraction(float A)
{
	return FMath::Fractional(A);
}

float UKismetMathLibrary::Add_FloatFloat(float A, float B)
{
	return A + B;
}	

float UKismetMathLibrary::Subtract_FloatFloat(float A, float B)
{
	return A - B;
}	

bool UKismetMathLibrary::Less_FloatFloat(float A, float B)
{
	return A < B;
}	

bool UKismetMathLibrary::Greater_FloatFloat(float A, float B)
{
	return A > B;
}	

bool UKismetMathLibrary::LessEqual_FloatFloat(float A, float B)
{
	return A <= B;
}	

bool UKismetMathLibrary::GreaterEqual_FloatFloat(float A, float B)
{
	return A >= B;
}	

bool UKismetMathLibrary::EqualEqual_FloatFloat(float A, float B)
{
	return A == B;
}	

bool UKismetMathLibrary::NearlyEqual_FloatFloat(float A, float B, float ErrorTolerance)
{
	return FMath::IsNearlyEqual(A, B, ErrorTolerance);
}

bool UKismetMathLibrary::NotEqual_FloatFloat(float A, float B)
{
	return A != B;
}	

bool UKismetMathLibrary::InRange_FloatFloat(float Value, float Min, float Max, bool InclusiveMin, bool InclusiveMax)
{
	return ((InclusiveMin ? (Value >= Min) : (Value > Min)) && (InclusiveMax ? (Value <= Max) : (Value < Max)));
}	

float UKismetMathLibrary::Hypotenuse(float Width, float Height)
{
	// This implementation avoids overflow/underflow caused by squaring width and height:
	Width = FMath::Abs(Width);
	Height = FMath::Abs(Height);

	float Min = FGenericPlatformMath::Min(Width, Height);
	float Max = FGenericPlatformMath::Max(Width, Height);
	float Ratio = Min / Max;
	return Max * FMath::Sqrt(1.f + Ratio * Ratio);
}

float UKismetMathLibrary::GridSnap_Float(float Location, float GridSize)
{
	return FMath::GridSnap(Location, GridSize);
}

float UKismetMathLibrary::GetPI()
{
	return PI;
}

float UKismetMathLibrary::GetTAU()
{
	return 2.f * PI;
}

float UKismetMathLibrary::DegreesToRadians(float A)
{
	return FMath::DegreesToRadians(A);
}

float UKismetMathLibrary::RadiansToDegrees(float A)
{
	return FMath::RadiansToDegrees(A);
}


float UKismetMathLibrary::Abs(float A)
{
	return FMath::Abs(A);
}	

float UKismetMathLibrary::Sin(float A)
{
	return FMath::Sin(A);
}	

float UKismetMathLibrary::Asin(float A)
{
	return FMath::Asin(A);
}	

float UKismetMathLibrary::Cos(float A)
{
	return FMath::Cos(A);
}	

float UKismetMathLibrary::Acos(float A)
{
	return FMath::Acos(A);
}	

float UKismetMathLibrary::Tan(float A)
{
	return FMath::Tan(A);
}	

float UKismetMathLibrary::Atan(float A)
{
	return FMath::Atan(A);
}

float UKismetMathLibrary::Atan2(float A, float B)
{
	return FMath::Atan2(A, B);
}	

float UKismetMathLibrary::DegSin(float A)
{
	return FMath::Sin(PI/(180.f) * A);
}

float UKismetMathLibrary::DegAsin(float A)
{
	return (180.f)/PI * FMath::Asin(A);
}

float UKismetMathLibrary::DegCos(float A)
{
	return FMath::Cos(PI/(180.f) * A);
}

float UKismetMathLibrary::DegAcos(float A)
{
	return (180.f)/PI * FMath::Acos(A);
}

float UKismetMathLibrary::DegTan(float A)
{
	return FMath::Tan(PI/(180.f) * A);
}

float UKismetMathLibrary::DegAtan(float A)
{
	return (180.f)/PI * FMath::Atan(A);
}

float UKismetMathLibrary::DegAtan2(float A, float B)
{
	return (180.f)/PI * FMath::Atan2(A, B);
}

float UKismetMathLibrary::ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees)
{
	return FMath::ClampAngle(AngleDegrees, MinAngleDegrees, MaxAngleDegrees);
}

float UKismetMathLibrary::Exp(float A)
{
	return FMath::Exp(A);
}	

float UKismetMathLibrary::Loge(float A)
{
	return FMath::Loge(A);
}	

float UKismetMathLibrary::Sqrt(float A)
{
	float Result = 0.f;
	if (A > 0.f)
	{
		// Can't use FMath::Sqrt(0) as it computes it as 1 / FMath::InvSqrt(). Not a problem as Sqrt variable defaults to 0 == sqrt(0).
		Result = FMath::Sqrt( A );
	}
	else if (A < 0.f)
	{
		FFrame::KismetExecutionMessage(TEXT("Attempt to take Sqrt() of negative number - returning 0."), ELogVerbosity::Warning, NegativeSqrtWarning);
	}

	return Result;
}	

float UKismetMathLibrary::Square(float A)
{
	return FMath::Square(A);
}	

int32 UKismetMathLibrary::Round(float A)
{
	return FMath::RoundToInt(A);
}	

int32 UKismetMathLibrary::FFloor(float A)
{
	return FMath::FloorToInt(A);
}	

int32 UKismetMathLibrary::FTrunc(float A)
{
	return FMath::TruncToInt(A);
}	

int32 UKismetMathLibrary::FCeil(float A)
{
	return FMath::CeilToInt(A);
}	

int32 UKismetMathLibrary::FMod(float Dividend, float Divisor, float& Remainder)
{
	int32 Result;
	if( Divisor != 0.f )
	{
		const float Quotient = Dividend / Divisor;
		Result = (Quotient < 0.f ? -1 : 1) * FMath::FloorToInt( FMath::Abs(Quotient) );
		Remainder = FMath::Fmod(Dividend, Divisor);
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("Attempted modulo 0 - returning 0."), ELogVerbosity::Warning, DivideByZeroWarning);

		Result = 0;
		Remainder = 0.f;
	}

	return Result;
}

float UKismetMathLibrary::SignOfFloat(float A)
{
	return FMath::Sign<float>(A);
}

float UKismetMathLibrary::NormalizeToRange(float Value, float RangeMin, float RangeMax)
{
	if (RangeMin == RangeMax)
	{
		return RangeMin;
	}

	if (RangeMin > RangeMax)
	{
		Swap(RangeMin, RangeMax);
	}
	return (Value - RangeMin) / (RangeMax - RangeMin);
}

float UKismetMathLibrary::MapRangeUnclamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB)
{
	return FMath::GetMappedRangeValueUnclamped(FVector2D(InRangeA,InRangeB),FVector2D(OutRangeA,OutRangeB),Value);
}

float UKismetMathLibrary::MapRangeClamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB)
{ 
	return FMath::GetMappedRangeValueClamped(FVector2D(InRangeA,InRangeB),FVector2D(OutRangeA,OutRangeB),Value);
}

float UKismetMathLibrary::MultiplyByPi(float Value)
{
	return Value * PI;
}

float UKismetMathLibrary::FInterpEaseInOut(float A, float B, float Alpha, float Exponent)
{
	return FMath::InterpEaseInOut<float>(A, B, Alpha, Exponent);
}

float UKismetMathLibrary::MakePulsatingValue(float InCurrentTime, float InPulsesPerSecond, float InPhase)
{
	return FMath::MakePulsatingValue((double)InCurrentTime, InPulsesPerSecond, InPhase);
}

float UKismetMathLibrary::FixedTurn(float InCurrent, float InDesired, float InDeltaRate)
{
	return FMath::FixedTurn(InCurrent, InDesired, InDeltaRate);
}

float UKismetMathLibrary::RandomFloat()
{
	return FMath::FRand();
}	

float UKismetMathLibrary::RandomFloatInRange(float Min, float Max)
{
	return FMath::FRandRange(Min, Max);
}	

float UKismetMathLibrary::FMin(float A, float B)
{
	return FMath::Min(A, B);
}	

float UKismetMathLibrary::FMax(float A, float B)
{
	return FMath::Max(A, B);
}	

float UKismetMathLibrary::FClamp(float V, float A, float B)
{
	return FMath::Clamp(V, A, B);
}	

void UKismetMathLibrary::MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue)
{
	MaxValue = FMath::Max(IntArray, &IndexOfMaxValue);
}

void UKismetMathLibrary::MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue)
{
	MinValue = FMath::Min<int32>(IntArray, &IndexOfMinValue);
}

void UKismetMathLibrary::MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue)
{
	MaxValue = FMath::Max(FloatArray, &IndexOfMaxValue);
}

void UKismetMathLibrary::MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue)
{
	MinValue = FMath::Min(FloatArray, &IndexOfMinValue);
}

void UKismetMathLibrary::MaxOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMaxValue, uint8& MaxValue)
{
	MaxValue = FMath::Max(ByteArray, &IndexOfMaxValue);
}

void UKismetMathLibrary::MinOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMinValue, uint8& MinValue)
{
	MinValue = FMath::Min(ByteArray, &IndexOfMinValue);
}

float UKismetMathLibrary::Lerp(float A, float B, float V)
{
	return A + V*(B-A);
}	

float UKismetMathLibrary::InverseLerp(float A, float B, float Value)
{
	if (FMath::IsNearlyEqual(A, B))
	{
		if (Value < A)
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return ((Value - A) / (B - A));
	}
}

float UKismetMathLibrary::Ease(float A, float B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return Lerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps));
}

float UKismetMathLibrary::FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	return FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed);
}

float UKismetMathLibrary::FInterpTo_Constant(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	return FMath::FInterpConstantTo(Current, Target, DeltaTime, InterpSpeed);
}

FVector UKismetMathLibrary::Multiply_VectorFloat(FVector A, float B)
{
	return A * B;
}

FVector UKismetMathLibrary::Multiply_VectorInt(FVector A, int32 B)
{
	return A * (float)B;
}

FVector UKismetMathLibrary::Multiply_VectorVector(FVector A, FVector B)
{
	return A * B;
}	

FVector UKismetMathLibrary::Divide_VectorFloat(FVector A, float B)
{
	if (B == 0.f)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_VectorFloat"), ELogVerbosity::Warning, DivideByZeroWarning);
		return FVector::ZeroVector;
	}

	return A / B;
}

FVector UKismetMathLibrary::Divide_VectorInt(FVector A, int32 B)
{
	if (B == 0)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_VectorInt"), ELogVerbosity::Warning, DivideByZeroWarning);
		return FVector::ZeroVector;
	}

	return A / (float)B;
}

FVector UKismetMathLibrary::Divide_VectorVector(FVector A, FVector B)
{
	if (B.X == 0.f || B.Y == 0.f || B.Z == 0.f)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_VectorVector"), ELogVerbosity::Warning, DivideByZeroWarning);
		return FVector::ZeroVector;
	}

	return A/B;
}


FVector UKismetMathLibrary::Add_VectorVector(FVector A, FVector B)
{
	return A + B;
}	

FVector UKismetMathLibrary::Add_VectorFloat(FVector A, float B)
{
	return A + B;
}	

FVector UKismetMathLibrary::Add_VectorInt(FVector A, int32 B)
{
	return A + (float)B;
}

FVector UKismetMathLibrary::Subtract_VectorVector(FVector A, FVector B)
{
	return A - B;
}	

FVector UKismetMathLibrary::Subtract_VectorFloat(FVector A, float B)
{
	return A - B;
}

FVector UKismetMathLibrary::Subtract_VectorInt(FVector A, int32 B)
{
	return A - (float)B;
}

FVector UKismetMathLibrary::LessLess_VectorRotator(FVector A, FRotator B)
{
	return B.UnrotateVector(A);
}	

FVector UKismetMathLibrary::GreaterGreater_VectorRotator(FVector A, FRotator B)
{
	return B.RotateVector(A);
}	

FVector  UKismetMathLibrary::RotateAngleAxis(FVector InVect, float AngleDeg, FVector Axis)
{
	return InVect.RotateAngleAxis(AngleDeg, Axis.GetSafeNormal());
}

bool UKismetMathLibrary::EqualEqual_VectorVector(FVector A, FVector B, float ErrorTolerance)
{
	return A.Equals(B, ErrorTolerance);
}	

bool UKismetMathLibrary::NotEqual_VectorVector(FVector A, FVector B, float ErrorTolerance)
{
	return !A.Equals(B, ErrorTolerance);
}	

float UKismetMathLibrary::Dot_VectorVector(FVector A, FVector B)
{
	return FVector::DotProduct(A, B);
}	

FVector UKismetMathLibrary::Cross_VectorVector(FVector A, FVector B)
{
	return FVector::CrossProduct(A, B);
}

float UKismetMathLibrary::DotProduct2D(FVector2D A, FVector2D B)
{
	return FVector2D::DotProduct(A, B);
}

float UKismetMathLibrary::CrossProduct2D(FVector2D A, FVector2D B)
{
	return FVector2D::CrossProduct(A, B);
}

float UKismetMathLibrary::VSize(FVector A)
{
	return A.Size();
}	

float UKismetMathLibrary::VSize2D(FVector2D A)
{
	return A.Size();
}

float UKismetMathLibrary::VSizeSquared(FVector A)
{
	return A.SizeSquared();
}

float UKismetMathLibrary::VSize2DSquared(FVector2D A)
{
	return A.SizeSquared();
}

FVector UKismetMathLibrary::Normal(FVector A)
{
	return A.GetSafeNormal();
}

FVector2D UKismetMathLibrary::Normal2D(FVector2D A)
{
	return A.GetSafeNormal();
}

FVector UKismetMathLibrary::VLerp(FVector A, FVector B, float V)
{
	return A + V*(B-A);
}	

FVector UKismetMathLibrary::VEase(FVector A, FVector B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return VLerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps));
}

FVector UKismetMathLibrary::VInterpTo(FVector Current, FVector Target, float DeltaTime, float InterpSpeed)
{
	return FMath::VInterpTo( Current, Target, DeltaTime, InterpSpeed );
}

FVector UKismetMathLibrary::VInterpTo_Constant(FVector Current, FVector Target, float DeltaTime, float InterpSpeed)
{
	return FMath::VInterpConstantTo(Current, Target, DeltaTime, InterpSpeed);
}

FVector2D UKismetMathLibrary::Vector2DInterpTo(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed)
{
	return FMath::Vector2DInterpTo( Current, Target, DeltaTime, InterpSpeed );
}
FVector2D UKismetMathLibrary::Vector2DInterpTo_Constant(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed)
{
	return FMath::Vector2DInterpConstantTo( Current, Target, DeltaTime, InterpSpeed );
}

FVector UKismetMathLibrary::RandomUnitVector()
{
	return FMath::VRand();
}

FVector UKismetMathLibrary::RandomPointInBoundingBox(const FVector& Origin, const FVector& BoxExtent)
{
	const FVector BoxMin = Origin - BoxExtent;
	const FVector BoxMax = Origin + BoxExtent;
	return FMath::RandPointInBox(FBox(BoxMin, BoxMax));
}

FVector UKismetMathLibrary::RandomUnitVectorInCone(FVector ConeDir, float ConeHalfAngle)
{
	return FMath::VRandCone(ConeDir, ConeHalfAngle);
}

FVector UKismetMathLibrary::RandomUnitVectorInConeWithYawAndPitch(FVector ConeDir, float MaxYawInDegrees, float MaxPitchInDegrees)
{
	return FMath::VRandCone(ConeDir, DegreesToRadians(MaxYawInDegrees), DegreesToRadians(MaxPitchInDegrees));
}

FVector UKismetMathLibrary::MirrorVectorByNormal(FVector A, FVector B)
{
	return FMath::GetReflectionVector(A, B);
}

FRotator UKismetMathLibrary::RandomRotator(bool bRoll)
{
	FRotator RRot;
	RRot.Yaw = FMath::FRand() * 360.f;
	RRot.Pitch = FMath::FRand() * 360.f;

	if (bRoll)
	{
		RRot.Roll = FMath::FRand() * 360.f;
	}
	else
	{
		RRot.Roll = 0;
	}
	return RRot;
}

FVector UKismetMathLibrary::ProjectVectorOnToVector(FVector V, FVector Target)
{
	if (Target.SizeSquared() > SMALL_NUMBER)
	{
		return V.ProjectOnTo(Target);
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: ProjectVectorOnToVector with zero Target vector"), ELogVerbosity::Warning, ZeroLengthProjectionWarning);
		return FVector::ZeroVector;
	}
}


void UKismetMathLibrary::FindNearestPointsOnLineSegments(FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point)
{
	FMath::SegmentDistToSegmentSafe(Segment1Start, Segment1End, Segment2Start, Segment2End, Segment1Point, Segment2Point);
}

FVector UKismetMathLibrary::FindClosestPointOnSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd)
{
	return FMath::ClosestPointOnSegment(Point, SegmentStart, SegmentEnd);
}

FVector UKismetMathLibrary::FindClosestPointOnLine(FVector Point, FVector LineOrigin, FVector LineDirection)
{
	const FVector SafeDir = LineDirection.GetSafeNormal();
	const FVector ClosestPoint = LineOrigin + (SafeDir * ((Point-LineOrigin) | SafeDir));
	return ClosestPoint;
}

float UKismetMathLibrary::GetPointDistanceToSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd)
{
	return FMath::PointDistToSegment(Point, SegmentStart, SegmentEnd);
}

float UKismetMathLibrary::GetPointDistanceToLine(FVector Point, FVector LineOrigin, FVector LineDirection)
{
	return FMath::PointDistToLine(Point, LineDirection, LineOrigin);
}

FVector UKismetMathLibrary::ProjectPointOnToPlane(FVector Point, FVector PlaneBase, FVector PlaneNormal)
{
	return FVector::PointPlaneProject(Point, PlaneBase, PlaneNormal);
}

FVector UKismetMathLibrary::ProjectVectorOnToPlane(FVector V, FVector PlaneNormal)
{
	return FVector::VectorPlaneProject(V, PlaneNormal);
}

FVector UKismetMathLibrary::NegateVector(FVector A)
{
	return -A;
}

FVector UKismetMathLibrary::ClampVectorSize(FVector A, float Min, float Max)
{
	return A.GetClampedToSize(Min, Max);
}

float UKismetMathLibrary::GetMinElement(FVector A)
{
	return A.GetMin();
}

float UKismetMathLibrary::GetMaxElement(FVector A)
{
	return A.GetMax();
}

FVector UKismetMathLibrary::GetVectorArrayAverage(const TArray<FVector>& Vectors)
{
	FVector Sum(0.f);
	FVector Average(0.f);

	if(Vectors.Num() > 0)
	{
		for(int32 VecIdx=0; VecIdx<Vectors.Num(); VecIdx++)
		{
			Sum += Vectors[VecIdx];
		}

		Average = Sum / ((float)Vectors.Num());
	}

	return Average;
}

/** Find the unit direction vector from one position to another. */
FVector UKismetMathLibrary::GetDirectionVector(FVector From, FVector To)
{
	return (To - From).GetSafeNormal();
}


bool UKismetMathLibrary::EqualEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance)
{
	return A.Equals(B, ErrorTolerance);
}	

bool UKismetMathLibrary::NotEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance)
{
	return !A.Equals(B, ErrorTolerance);
}

FRotator UKismetMathLibrary::Multiply_RotatorFloat(FRotator A, float B)
{
	return A * B;
}

FRotator UKismetMathLibrary::Multiply_RotatorInt(FRotator A, int32 B)
{
	return A * (float)B;
}	

FRotator UKismetMathLibrary::ComposeRotators(FRotator A, FRotator B)
{
	FQuat AQuat = FQuat(A);
	FQuat BQuat = FQuat(B);

	return FRotator(BQuat*AQuat);
}

FRotator UKismetMathLibrary::NegateRotator( FRotator A )
{
	return A.GetInverse();
}

void UKismetMathLibrary::GetAxes(FRotator A, FVector& X, FVector& Y, FVector& Z)
{
	FRotationMatrix R(A);
	R.GetScaledAxes(X,Y,Z);
}

FRotator UKismetMathLibrary::RLerp(FRotator A, FRotator B, float Alpha, bool bShortestPath)
{
	FRotator DeltaAngle = B - A;

	// if shortest path, we use Quaternion to interpolate instead of using FRotator
	if( bShortestPath )
	{
		FQuat AQuat(A);
		FQuat BQuat(B);

		FQuat Result = FQuat::Slerp(AQuat, BQuat, Alpha);
		Result.Normalize();

		return Result.Rotator();
	}

	return A + Alpha*DeltaAngle;
}

FRotator UKismetMathLibrary::REase(FRotator A, FRotator B, float Alpha, bool bShortestPath, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return RLerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps), bShortestPath);
}

FRotator UKismetMathLibrary::NormalizedDeltaRotator(FRotator A, FRotator B)
{
	FRotator Delta = A - B;
	Delta.Normalize();
	return Delta;
}

FRotator UKismetMathLibrary::RotatorFromAxisAndAngle(FVector Axis, float Angle)
{
	FVector SafeAxis = Axis.GetSafeNormal(); // Make sure axis is unit length
	return FQuat(SafeAxis, FMath::DegreesToRadians(Angle)).Rotator();
}


float UKismetMathLibrary::ClampAxis(float Angle)
{
	return FRotator::ClampAxis(Angle);
}

float UKismetMathLibrary::NormalizeAxis(float Angle)
{
	return FRotator::NormalizeAxis(Angle);
}

FRotator UKismetMathLibrary::RInterpTo(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed)
{
	return FMath::RInterpTo( Current, Target, DeltaTime, InterpSpeed);
}

FRotator UKismetMathLibrary::RInterpTo_Constant(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed)
{
	return FMath::RInterpConstantTo(Current, Target, DeltaTime, InterpSpeed);
}

FLinearColor UKismetMathLibrary::CInterpTo(FLinearColor Current, FLinearColor Target, float DeltaTime, float InterpSpeed)
{
	return FMath::CInterpTo(Current, Target, DeltaTime, InterpSpeed);
}

FLinearColor UKismetMathLibrary::LinearColorLerp(FLinearColor A, FLinearColor B, float Alpha)
{
	return A + Alpha * (B - A);
}

FLinearColor UKismetMathLibrary::LinearColorLerpUsingHSV(FLinearColor A, FLinearColor B, float Alpha)
{
	return FLinearColor::LerpUsingHSV( A, B, Alpha );
}

FLinearColor UKismetMathLibrary::Multiply_LinearColorLinearColor(FLinearColor A, FLinearColor B)
{
	return A * B;
}

FLinearColor UKismetMathLibrary::Multiply_LinearColorFloat(FLinearColor A, float B)
{
	return A * B;
}

FVector UKismetMathLibrary::TransformLocation(const FTransform& T, FVector Location)
{
	return T.TransformPosition(Location);
}

FVector UKismetMathLibrary::TransformDirection(const FTransform& T, FVector Direction)
{
	return T.TransformVectorNoScale(Direction);
}

FVector UKismetMathLibrary::InverseTransformLocation(const FTransform& T, FVector Location)
{
	return T.InverseTransformPosition(Location);
}

FVector UKismetMathLibrary::InverseTransformDirection(const FTransform& T, FVector Direction)
{
	return T.InverseTransformVectorNoScale(Direction);
}

FTransform UKismetMathLibrary::ComposeTransforms(const FTransform& A, const FTransform& B)
{
	return A * B;
}

FTransform UKismetMathLibrary::ConvertTransformToRelative(const FTransform& Transform, const FTransform& ParentTransform)
{
	return ParentTransform.GetRelativeTransform(Transform);
}

FTransform UKismetMathLibrary::InvertTransform(const FTransform& T)
{
	return T.Inverse();
}



FTransform UKismetMathLibrary::TLerp(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<ELerpInterpolationMode::Type> LerpInterpolationMode)
{
	FTransform Result;

	FTransform NA = A;
	FTransform NB = B;
	NA.NormalizeRotation();
	NB.NormalizeRotation();

	// Quaternion interpolation
	if (LerpInterpolationMode == ELerpInterpolationMode::QuatInterp)
	{
		Result.Blend(NA, NB, Alpha);
		return Result;
	}
	// Euler Angle interpolation
	else if (LerpInterpolationMode == ELerpInterpolationMode::EulerInterp)
	{
		Result.SetTranslation(FMath::Lerp(NA.GetTranslation(), NB.GetTranslation(), Alpha));
		Result.SetScale3D(FMath::Lerp(NA.GetScale3D(), NB.GetScale3D(), Alpha));
		Result.SetRotation(FQuat(RLerp(NA.Rotator(), NB.Rotator(), Alpha, false)));
		return Result;
	}
	// Dual quaternion interpolation
	else
	{
		if ((NB.GetRotation() | NA.GetRotation()) < 0.0f)
		{
			NB.SetRotation(NB.GetRotation()*-1.0f);
		}
		return (FDualQuat(NA)*(1 - Alpha) + FDualQuat(NB)*Alpha).Normalized().AsFTransform(FMath::Lerp(NA.GetScale3D(), NB.GetScale3D(), Alpha));
	}
}

FTransform UKismetMathLibrary::TEase(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return TLerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps));
}

FTransform UKismetMathLibrary::TInterpTo(const FTransform& Current, const FTransform& Target, float DeltaTime, float InterpSpeed)
{
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	const float Alpha = FClamp(DeltaTime * InterpSpeed, 0.f, 1.f);

	return TLerp(Current, Target, Alpha);
}

bool UKismetMathLibrary::EqualEqual_TransformTransform(const FTransform& A, const FTransform& B)
{
	return NearlyEqual_TransformTransform(A, B);
}

bool UKismetMathLibrary::NearlyEqual_TransformTransform(const FTransform& A, const FTransform& B, float LocationTolerance, float RotationTolerance, float Scale3DTolerance)
{
	return 
		FTransform::AreRotationsEqual(A, B, RotationTolerance) && 
		FTransform::AreTranslationsEqual(A, B, LocationTolerance) && 
		FTransform::AreScale3DsEqual(A, B, Scale3DTolerance);
}



FVector2D UKismetMathLibrary::Add_Vector2DVector2D(FVector2D A, FVector2D B)
{
	return A + B;
}

FVector2D UKismetMathLibrary::Subtract_Vector2DVector2D(FVector2D A, FVector2D B)
{
	return A - B;
}

FVector2D UKismetMathLibrary::Multiply_Vector2DFloat(FVector2D A, float B)
{
	return A * B;
}

FVector2D UKismetMathLibrary::Multiply_Vector2DVector2D(FVector2D A, FVector2D B)
{
	return A * B;
}

FVector2D UKismetMathLibrary::Divide_Vector2DFloat(FVector2D A, float B)
{
	if (B == 0.f)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_Vector2DFloat"), ELogVerbosity::Warning, DivideByZeroWarning);
		return FVector2D::ZeroVector;
	}

	return A / B;
}

FVector2D UKismetMathLibrary::Add_Vector2DFloat(FVector2D A, float B)
{
	return A+B;
}

FVector2D UKismetMathLibrary::Subtract_Vector2DFloat(FVector2D A, float B)
{
	return A-B;
}

bool UKismetMathLibrary::EqualEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance)
{ 
    return A.Equals(B,ErrorTolerance);
}

bool UKismetMathLibrary::NotEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance)
{
    return !A.Equals(B,ErrorTolerance);
}

bool UKismetMathLibrary::EqualEqual_NameName(FName A, FName B)
{
	return A == B;
}

bool UKismetMathLibrary::NotEqual_NameName(FName A, FName B)
{
	return A != B;
}

bool UKismetMathLibrary::EqualEqual_ObjectObject(class UObject* A, class UObject* B)
{
	return A == B;
}

bool UKismetMathLibrary::NotEqual_ObjectObject(class UObject* A, class UObject* B)
{
	return A != B;
}

bool UKismetMathLibrary::EqualEqual_ClassClass(class UClass* A, class UClass* B)
{
	return A == B;
}

bool UKismetMathLibrary::NotEqual_ClassClass(class UClass* A, class UClass* B)
{
	return A != B;
}

bool UKismetMathLibrary::ClassIsChildOf(TSubclassOf<class UObject> TestClass, TSubclassOf<class UObject> ParentClass)
{
	return ((*ParentClass != NULL) && (*TestClass != NULL)) ? (*TestClass)->IsChildOf(*ParentClass) : false;
}

/* Plane functions
 *****************************************************************************/
FPlane UKismetMathLibrary::MakePlaneFromPointAndNormal(FVector Point, FVector Normal)
{ 
	return FPlane(Point, Normal.GetSafeNormal());
}
 
/* DateTime functions
 *****************************************************************************/
FDateTime UKismetMathLibrary::MakeDateTime(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
{
	if (!FDateTime::Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("DateTime in bad format (year %d, month %d, day %d, hour %d, minute %d, second %d, millisecond %d). E.g. year, month and day can't be zero."), Year, Month, Day, Hour, Minute, Second, Millisecond), ELogVerbosity::Warning, InvalidDateWarning);

		return FDateTime(1, 1, 1, 0, 0, 0, 0);
	}

	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}

void UKismetMathLibrary::BreakDateTime(FDateTime InDateTime, int32& Year, int32& Month, int32& Day, int32& Hour, int32& Minute, int32& Second, int32& Millisecond)
{
	Year = GetYear(InDateTime);
	Month = GetMonth(InDateTime);
	Day = GetDay(InDateTime);
	Hour = GetHour(InDateTime);
	Minute = GetMinute(InDateTime);
	Second = GetSecond(InDateTime);
	Millisecond = GetMillisecond(InDateTime);
}

FDateTime UKismetMathLibrary::Add_DateTimeTimespan( FDateTime A, FTimespan B )
{
	return A + B;
}


FDateTime UKismetMathLibrary::Subtract_DateTimeTimespan( FDateTime A, FTimespan B )
{
	return A - B;
}


FTimespan UKismetMathLibrary::Subtract_DateTimeDateTime(FDateTime A, FDateTime B)
{
	return A - B;
}

bool UKismetMathLibrary::EqualEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A == B;
}


bool UKismetMathLibrary::NotEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A != B;
}


bool UKismetMathLibrary::Greater_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A > B;
}


bool UKismetMathLibrary::GreaterEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A >= B;
}


bool UKismetMathLibrary::Less_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A < B;
}


bool UKismetMathLibrary::LessEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A <= B;
}


FDateTime UKismetMathLibrary::GetDate( FDateTime A )
{
	return A.GetDate();
}


int32 UKismetMathLibrary::GetDay( FDateTime A )
{
	return A.GetDay();
}

int32 UKismetMathLibrary::GetDayOfYear( FDateTime A )
{
	return A.GetDayOfYear();
}


int32 UKismetMathLibrary::GetHour( FDateTime A )
{
	return A.GetHour();
}


int32 UKismetMathLibrary::GetHour12( FDateTime A )
{
	return A.GetHour12();
}


int32 UKismetMathLibrary::GetMillisecond( FDateTime A )
{
	return A.GetMillisecond();
}


int32 UKismetMathLibrary::GetMinute( FDateTime A )
{
	return A.GetMinute();
}


int32 UKismetMathLibrary::GetMonth( FDateTime A )
{
	return A.GetMonth();
}


int32 UKismetMathLibrary::GetSecond( FDateTime A )
{
	return A.GetSecond();
}


FTimespan UKismetMathLibrary::GetTimeOfDay( FDateTime A )
{
	return A.GetTimeOfDay();
}


int32 UKismetMathLibrary::GetYear( FDateTime A )
{
	return A.GetYear();
}


bool UKismetMathLibrary::IsAfternoon( FDateTime A )
{
	return A.IsAfternoon();
}


bool UKismetMathLibrary::IsMorning( FDateTime A )
{
	return A.IsMorning();
}


int32 UKismetMathLibrary::DaysInMonth( int32 Year, int32 Month )
{
	if ((Month < 1) || (Month > 12))
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid month (must be between 1 and 12): DaysInMonth"), ELogVerbosity::Warning, InvalidDateWarning);
		return 0;
	}

	return FDateTime::DaysInMonth(Year, Month);
}


int32 UKismetMathLibrary::DaysInYear( int32 Year )
{
	return FDateTime::DaysInYear(Year);
}


bool UKismetMathLibrary::IsLeapYear( int32 Year )
{
	return FDateTime::IsLeapYear(Year);
}


FDateTime UKismetMathLibrary::DateTimeMaxValue( )
{
	return FDateTime::MaxValue();
}


FDateTime UKismetMathLibrary::DateTimeMinValue( )
{
	return FDateTime::MinValue();
}


FDateTime UKismetMathLibrary::Now( )
{
	return FDateTime::Now();
}


FDateTime UKismetMathLibrary::Today( )
{
	return FDateTime::Today();
}


FDateTime UKismetMathLibrary::UtcNow( )
{
	return FDateTime::UtcNow();
}


bool UKismetMathLibrary::DateTimeFromIsoString(FString IsoString, FDateTime& Result)
{
	return FDateTime::ParseIso8601(*IsoString, Result);
}


bool UKismetMathLibrary::DateTimeFromString(FString DateTimeString, FDateTime& Result)
{
	return FDateTime::Parse(DateTimeString, Result);
}


/* Timespan functions
 *****************************************************************************/

FTimespan UKismetMathLibrary::MakeTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds)
{
	return FTimespan(Days, Hours, Minutes, Seconds, Milliseconds);
}

void UKismetMathLibrary::BreakTimespan(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& Milliseconds)
{
	Days = InTimespan.GetDays();
	Hours = InTimespan.GetHours();
	Minutes = InTimespan.GetMinutes();
	Seconds = InTimespan.GetSeconds();
	Milliseconds = InTimespan.GetMilliseconds();
}

FTimespan UKismetMathLibrary::Add_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A + B;
}


FTimespan UKismetMathLibrary::Subtract_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A - B;
}


FTimespan UKismetMathLibrary::Multiply_TimespanFloat( FTimespan A, float Scalar )
{
	return A * Scalar;
}


bool UKismetMathLibrary::EqualEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A == B;
}


bool UKismetMathLibrary::NotEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A != B;
}


bool UKismetMathLibrary::Greater_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A > B;
}


bool UKismetMathLibrary::GreaterEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A >= B;
}


bool UKismetMathLibrary::Less_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A < B;
}


bool UKismetMathLibrary::LessEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A <= B;
}


int32 UKismetMathLibrary::GetDays( FTimespan A )
{
	return A.GetDays();
}


FTimespan UKismetMathLibrary::GetDuration( FTimespan A )
{
	return A.GetDuration();
}


int32 UKismetMathLibrary::GetHours( FTimespan A )
{
	return A.GetHours();
}


int32 UKismetMathLibrary::GetMilliseconds( FTimespan A )
{
	return A.GetMilliseconds();
}


int32 UKismetMathLibrary::GetMinutes( FTimespan A )
{
	return A.GetMinutes();
}


int32 UKismetMathLibrary::GetSeconds( FTimespan A )
{
	return A.GetSeconds();
}


float UKismetMathLibrary::GetTotalDays( FTimespan A )
{
	return A.GetTotalDays();
}


float UKismetMathLibrary::GetTotalHours( FTimespan A )
{
	return A.GetTotalHours();
}


float UKismetMathLibrary::GetTotalMilliseconds( FTimespan A )
{
	return A.GetTotalMilliseconds();
}


float UKismetMathLibrary::GetTotalMinutes( FTimespan A )
{
	return A.GetTotalMinutes();
}


float UKismetMathLibrary::GetTotalSeconds( FTimespan A )
{
	return A.GetTotalSeconds();
}


FTimespan UKismetMathLibrary::FromDays( float Days )
{
	return FTimespan::FromDays(Days);
}


FTimespan UKismetMathLibrary::FromHours( float Hours )
{
	return FTimespan::FromHours(Hours);
}


FTimespan UKismetMathLibrary::FromMilliseconds( float Milliseconds )
{
	return FTimespan::FromMilliseconds(Milliseconds);
}


FTimespan UKismetMathLibrary::FromMinutes( float Minutes )
{
	return FTimespan::FromMinutes(Minutes);
}


FTimespan UKismetMathLibrary::FromSeconds( float Seconds )
{
	return FTimespan::FromSeconds(Seconds);
}


FTimespan UKismetMathLibrary::TimespanMaxValue( )
{
	return FTimespan::MaxValue();
}


FTimespan UKismetMathLibrary::TimespanMinValue( )
{
	return FTimespan::MinValue();
}


float UKismetMathLibrary::TimespanRatio( FTimespan A, FTimespan B )
{
	if (B != FTimespan::Zero())
	{
		return (float)A.GetTicks() / (float)B.GetTicks();
	}

	return 0.0;
}


FTimespan UKismetMathLibrary::TimespanZeroValue( )
{
	return FTimespan::Zero();
}


bool UKismetMathLibrary::TimespanFromString(FString TimespanString, FTimespan& Result)
{
	return FTimespan::Parse(TimespanString, Result);
}


/* K2 Utilities
 *****************************************************************************/

float UKismetMathLibrary::Conv_ByteToFloat(uint8 InByte)
{
	return (float)InByte;
}

float UKismetMathLibrary::Conv_IntToFloat(int32 InInt)
{
	return (float)InInt;
}

uint8 UKismetMathLibrary::Conv_IntToByte(int32 InInt)
{
	return (uint8)InInt;
}

bool UKismetMathLibrary::Conv_IntToBool(int32 InInt)
{
	return InInt == 0 ? false : true;
}

int32 UKismetMathLibrary::Conv_BoolToInt(bool InBool)
{
	return InBool ? 1 : 0;
}

uint8 UKismetMathLibrary::Conv_BoolToByte(bool InBool)
{
	return InBool ? 1 : 0;
}

float UKismetMathLibrary::Conv_BoolToFloat(bool InBool)
{
	return InBool ? 1.0f : 0.0f;
}

int32 UKismetMathLibrary::Conv_ByteToInt(uint8 InByte)
{
	return (int32)InByte;
}

FRotator UKismetMathLibrary::Conv_VectorToRotator(FVector InVec)
{
	return InVec.Rotation();
}

FVector UKismetMathLibrary::Conv_RotatorToVector(FRotator InRot)
{
	return InRot.Vector();
}

FLinearColor UKismetMathLibrary::Conv_VectorToLinearColor(FVector InVec)
{
	return FLinearColor(InVec);	
}

FVector2D UKismetMathLibrary::Conv_VectorToVector2D(FVector InVec)
{
	return FVector2D(InVec);
}

FVector UKismetMathLibrary::Conv_Vector2DToVector(FVector2D InVec2D, float Z)
{
	return FVector(InVec2D, Z);
}

FVector UKismetMathLibrary::Conv_LinearColorToVector(FLinearColor InLinearColor)
{
	return FVector(InLinearColor);
}

FLinearColor UKismetMathLibrary::Conv_ColorToLinearColor(FColor InColor)
{
	return FLinearColor(InColor);
}

FColor UKismetMathLibrary::Conv_LinearColorToColor(FLinearColor InLinearColor)
{
	return InLinearColor.ToFColor(true);
}

FTransform UKismetMathLibrary::Conv_VectorToTransform(FVector InTranslation)
{
	return FTransform(InTranslation);
}

FVector UKismetMathLibrary::Conv_FloatToVector(float InFloat)
{
	return FVector(InFloat);
}

FLinearColor UKismetMathLibrary::Conv_FloatToLinearColor(float InFloat)
{
	return FLinearColor(InFloat, InFloat, InFloat);
}

FVector UKismetMathLibrary::MakeVector(float X, float Y, float Z)
{
	return FVector(X,Y,Z);
}

void UKismetMathLibrary::BreakVector(FVector InVec, float& X, float& Y, float& Z)
{
	X = InVec.X;
	Y = InVec.Y;
	Z = InVec.Z;
}

FVector2D UKismetMathLibrary::MakeVector2D(float X, float Y)
{
	return FVector2D(X, Y);
}

void UKismetMathLibrary::BreakVector2D(FVector2D InVec, float& X, float& Y)
{
	X = InVec.X;
	Y = InVec.Y;
}

FVector UKismetMathLibrary::GetForwardVector(FRotator InRot)
{
	return FRotationMatrix(InRot).GetScaledAxis( EAxis::X );
}

FVector UKismetMathLibrary::GetRightVector(FRotator InRot)
{
	return FRotationMatrix(InRot).GetScaledAxis( EAxis::Y );
}

FVector UKismetMathLibrary::GetUpVector(FRotator InRot)
{
	return FRotationMatrix(InRot).GetScaledAxis( EAxis::Z );
}

FVector UKismetMathLibrary::CreateVectorFromYawPitch(float Yaw, float Pitch, float Length /*= 1.0f */)
{
	// FRotator::Vector() behaviour 
	float CP, SP, CY, SY;
	FMath::SinCos(&SP, &CP, FMath::DegreesToRadians(Pitch));
	FMath::SinCos(&SY, &CY, FMath::DegreesToRadians(Yaw));
	FVector V = FVector(CP*CY, CP*SY, SP) * Length;
		
	return V;
}

void UKismetMathLibrary::GetYawPitchFromVector(FVector InVec, float& Yaw, float& Pitch)
{
	FVector NormalizedVector = InVec.GetSafeNormal();
	// Find yaw.
	Yaw = FMath::Atan2(NormalizedVector.Y, NormalizedVector.X) * 180.f / PI;

	// Find pitch.
	Pitch = FMath::Atan2(NormalizedVector.Z, FMath::Sqrt(NormalizedVector.X*NormalizedVector.X + NormalizedVector.Y*NormalizedVector.Y)) * 180.f / PI;	
}

FRotator UKismetMathLibrary::MakeRotator(float Roll, float Pitch, float Yaw)
{
	return FRotator(Pitch,Yaw,Roll);
}

FRotator UKismetMathLibrary::FindLookAtRotation(const FVector& Start, const FVector& Target)
{
	return MakeRotFromX(Target - Start);
}

FRotator UKismetMathLibrary::MakeRotFromX(const FVector& X)
{
	return FRotationMatrix::MakeFromX(X).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromY(const FVector& Y)
{
	return FRotationMatrix::MakeFromY(Y).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromZ(const FVector& Z)
{
	return FRotationMatrix::MakeFromZ(Z).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromXY(const FVector& X, const FVector& Y)
{
	return FRotationMatrix::MakeFromXY(X, Y).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromXZ(const FVector& X, const FVector& Z)
{
	return FRotationMatrix::MakeFromXZ(X, Z).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromYX(const FVector& Y, const FVector& X)
{
	return FRotationMatrix::MakeFromYX(Y, X).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromYZ(const FVector& Y, const FVector& Z)
{
	return FRotationMatrix::MakeFromYZ(Y, Z).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromZX(const FVector& Z, const FVector& X)
{
	return FRotationMatrix::MakeFromZX(Z, X).Rotator();
}

FRotator UKismetMathLibrary::MakeRotFromZY(const FVector& Z, const FVector& Y)
{
	return FRotationMatrix::MakeFromZY(Z, Y).Rotator();
}


void UKismetMathLibrary::BreakRotator(FRotator InRot, float& Roll, float& Pitch, float& Yaw)
{
	Pitch = InRot.Pitch;
	Yaw = InRot.Yaw;
	Roll = InRot.Roll;
}

void UKismetMathLibrary::BreakRotIntoAxes(const FRotator& InRot, FVector& X, FVector& Y, FVector& Z)
{
	FRotationMatrix(InRot).GetScaledAxes(X, Y, Z);
}


FTransform UKismetMathLibrary::MakeTransform(FVector Translation, FRotator Rotation, FVector Scale)
{
	return FTransform(Rotation,Translation,Scale);
}

void UKismetMathLibrary::BreakTransform(const FTransform& InTransform, FVector& Translation, FRotator& Rotation, FVector& Scale)
{
	Translation = InTransform.GetLocation();
	Rotation = InTransform.Rotator();
	Scale = InTransform.GetScale3D();
}

FLinearColor UKismetMathLibrary::MakeColor(float R, float G, float B, float A)
{
	return FLinearColor(R,G,B,A);
}

void UKismetMathLibrary::BreakColor(const FLinearColor InColor, float& R, float& G, float& B, float& A)
{
	R = InColor.R;
	G = InColor.G;
	B = InColor.B;
	A = InColor.A;
}

FLinearColor UKismetMathLibrary::HSVToRGB(float H, float S, float V, float A)
{
	const FLinearColor HSV(H, S, V, A);
	return HSV.HSVToLinearRGB();
}

void UKismetMathLibrary::RGBToHSV(const FLinearColor InColor, float& H, float& S, float& V, float& A)
{
	const FLinearColor HSV(InColor.LinearRGBToHSV());
	H = HSV.R;
	S = HSV.G;
	V = HSV.B;
	A = HSV.A;
}

void UKismetMathLibrary::HSVToRGB_Vector(const FLinearColor HSV, FLinearColor& RGB)
{
	RGB = HSV.HSVToLinearRGB();
}

void UKismetMathLibrary::RGBToHSV_Vector(const FLinearColor RGB, FLinearColor& HSV)
{
	HSV = RGB.LinearRGBToHSV();
}

FString UKismetMathLibrary::SelectString(const FString& A, const FString& B, bool bSelectA)
{
	return bSelectA ? A : B;
}

int32 UKismetMathLibrary::SelectInt(int32 A, int32 B, bool bSelectA)
{
	return bSelectA ? A : B;
}

float UKismetMathLibrary::SelectFloat(float A, float B, bool bSelectA)
{
	return bSelectA ? A : B;
}

FVector UKismetMathLibrary::SelectVector(FVector A, FVector B, bool bSelectA)
{
	return bSelectA ? A : B;
}

FRotator UKismetMathLibrary::SelectRotator(FRotator A, FRotator B, bool bSelectA)
{
	return bSelectA ? A : B;
}

FLinearColor UKismetMathLibrary::SelectColor(FLinearColor A, FLinearColor B, bool bSelectA)
{
	return bSelectA ? A : B;
}

FTransform UKismetMathLibrary::SelectTransform(const FTransform& A, const FTransform& B, bool bSelectA)
{
	return bSelectA ? A : B;
}

UObject* UKismetMathLibrary::SelectObject(UObject* A, UObject* B, bool bSelectA)
{
	return bSelectA ? A : B;
}

UClass* UKismetMathLibrary::SelectClass(UClass* A, UClass* B, bool bSelectA)
{
	return bSelectA ? A : B;
}

FRotator UKismetMathLibrary::MakeRotationFromAxes(FVector Forward, FVector Right, FVector Up)
{
	Forward.Normalize();
	Right.Normalize();
	Up.Normalize();

	FMatrix RotMatrix(Forward, Right, Up, FVector::ZeroVector);

	return RotMatrix.Rotator();
}


// Random stream functions


int32 UKismetMathLibrary::RandomIntegerFromStream(int32 Max, const FRandomStream& Stream)
{
	return Stream.RandHelper(Max);
}

int32 UKismetMathLibrary::RandomIntegerInRangeFromStream(int32 Min, int32 Max, const FRandomStream& Stream)
{
	return Stream.RandRange(Min, Max);
}

bool UKismetMathLibrary::RandomBoolFromStream(const FRandomStream& Stream)
{
	return (Stream.RandRange(0,1) == 1) ? true : false;
}

float UKismetMathLibrary::RandomFloatFromStream(const FRandomStream& Stream)
{
	return Stream.FRand();
}

float UKismetMathLibrary::RandomFloatInRangeFromStream(float Min, float Max, const FRandomStream& Stream)
{
	return Min + (Max - Min) * RandomFloatFromStream(Stream);
}

FVector UKismetMathLibrary::RandomUnitVectorFromStream(const FRandomStream& Stream)
{
	return Stream.VRand();
}

FRotator UKismetMathLibrary::RandomRotatorFromStream(bool bRoll, const FRandomStream& Stream)
{
	FRotator RRot;
	RRot.Yaw = RandomFloatFromStream(Stream) * 360.f;
	RRot.Pitch = RandomFloatFromStream(Stream) * 360.f;

	if (bRoll)
	{
		RRot.Roll = RandomFloatFromStream(Stream) * 360.f;
	}
	else
	{
		RRot.Roll = 0;
	}
	return RRot;
}

void UKismetMathLibrary::ResetRandomStream(const FRandomStream& Stream)
{
	Stream.Reset();
}

void UKismetMathLibrary::SeedRandomStream(FRandomStream& Stream)
{
	Stream.GenerateNewSeed();
}

void UKismetMathLibrary::SetRandomStreamSeed(FRandomStream& Stream, int32 NewSeed)
{
	Stream.Initialize(NewSeed);
}


void UKismetMathLibrary::MinimumAreaRectangle(class UObject* WorldContextObject, const TArray<FVector>& InVerts, const FVector& SampleSurfaceNormal, FVector& OutRectCenter, FRotator& OutRectRotation, float& OutSideLengthX, float& OutSideLengthY, bool bDebugDraw)
{
	float MinArea = -1.f;
	float CurrentArea = -1.f;
	FVector SupportVectorA, SupportVectorB;
	FVector RectSideA, RectSideB;
	float MinDotResultA, MinDotResultB, MaxDotResultA, MaxDotResultB;
	FVector TestEdge;
	float TestEdgeDot = 0.f;
	FVector PolyNormal(0.f, 0.f, 1.f);
	TArray<int32> PolyVertIndices;

	// Bail if we receive an empty InVerts array
	if( InVerts.Num() == 0 )
	{
		return;
	}

	// Compute the approximate normal of the poly, using the direction of SampleSurfaceNormal for guidance
	PolyNormal = (InVerts[InVerts.Num() / 3] - InVerts[0]) ^ (InVerts[InVerts.Num() * 2 / 3] - InVerts[InVerts.Num() / 3]);
	if( (PolyNormal | SampleSurfaceNormal) < 0.f )
	{
		PolyNormal = -PolyNormal;
	}

	// Transform the sample points to 2D
	FMatrix SurfaceNormalMatrix = FRotationMatrix::MakeFromZX(PolyNormal, FVector(1.f, 0.f, 0.f));
	TArray<FVector> TransformedVerts;
	OutRectCenter = FVector(0.f);
	for( int32 Idx = 0; Idx < InVerts.Num(); ++Idx )
	{
		OutRectCenter += InVerts[Idx];
		TransformedVerts.Add(SurfaceNormalMatrix.InverseTransformVector(InVerts[Idx]));
	}
	OutRectCenter /= InVerts.Num();

	// Compute the convex hull of the sample points
	ConvexHull2D::ComputeConvexHull(TransformedVerts, PolyVertIndices);

	// Minimum area rectangle as computed by http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
	for( int32 Idx = 1; Idx < PolyVertIndices.Num() - 1; ++Idx )
	{
		SupportVectorA = (TransformedVerts[PolyVertIndices[Idx]] - TransformedVerts[PolyVertIndices[Idx-1]]).GetSafeNormal();
		SupportVectorA.Z = 0.f;
		SupportVectorB.X = -SupportVectorA.Y;
		SupportVectorB.Y = SupportVectorA.X;
		SupportVectorB.Z = 0.f;
		MinDotResultA = MinDotResultB = MaxDotResultA = MaxDotResultB = 0.f;

		for (int TestVertIdx = 1; TestVertIdx < PolyVertIndices.Num(); ++TestVertIdx )
		{
			TestEdge = TransformedVerts[PolyVertIndices[TestVertIdx]] - TransformedVerts[PolyVertIndices[0]];
			TestEdgeDot = SupportVectorA | TestEdge;
			if( TestEdgeDot < MinDotResultA )
			{
				MinDotResultA = TestEdgeDot;
			}
			else if(TestEdgeDot > MaxDotResultA )
			{
				MaxDotResultA = TestEdgeDot;
			}

			TestEdgeDot = SupportVectorB | TestEdge;
			if( TestEdgeDot < MinDotResultB )
			{
				MinDotResultB = TestEdgeDot;
			}
			else if( TestEdgeDot > MaxDotResultB )
			{
				MaxDotResultB = TestEdgeDot;
			}
		}

		CurrentArea = (MaxDotResultA - MinDotResultA) * (MaxDotResultB - MinDotResultB);
		if( MinArea < 0.f || CurrentArea < MinArea )
		{
			MinArea = CurrentArea;
			RectSideA = SupportVectorA * (MaxDotResultA - MinDotResultA);
			RectSideB = SupportVectorB * (MaxDotResultB - MinDotResultB);
		}
	}

	RectSideA = SurfaceNormalMatrix.TransformVector(RectSideA);
	RectSideB = SurfaceNormalMatrix.TransformVector(RectSideB);
	OutRectRotation = FRotationMatrix::MakeFromZX(PolyNormal, RectSideA).Rotator();
	OutSideLengthX = RectSideA.Size();
	OutSideLengthY = RectSideB.Size();

	if( bDebugDraw )
	{
		UWorld* World = (WorldContextObject) ? GEngine->GetWorldFromContextObject(WorldContextObject) : nullptr;
		if(World != nullptr)
		{
			DrawDebugSphere(World, OutRectCenter, 10.f, 12, FColor::Yellow, true);
			DrawDebugCoordinateSystem(World, OutRectCenter, SurfaceNormalMatrix.Rotator(), 100.f, true);
			DrawDebugLine(World, OutRectCenter - RectSideA * 0.5f + FVector(0,0,10.f), OutRectCenter + RectSideA * 0.5f + FVector(0,0,10.f), FColor::Green, true,-1, 0, 5.f);
			DrawDebugLine(World, OutRectCenter - RectSideB * 0.5f + FVector(0,0,10.f), OutRectCenter + RectSideB * 0.5f + FVector(0,0,10.f), FColor::Blue, true,-1, 0, 5.f);
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("WorldContext required for MinimumAreaRectangle to draw a debug visualization."), ELogVerbosity::Warning);
		}
	}
}

bool UKismetMathLibrary::PointsAreCoplanar(const TArray<FVector>& Points, float Tolerance)
{
	return FMath::PointsAreCoplanar(Points, Tolerance);
}

bool UKismetMathLibrary::LinePlaneIntersection(const FVector& LineStart, const FVector& LineEnd, const FPlane& APlane, float& T, FVector& Intersection)
{
	FVector RayDir = LineEnd - LineStart;

	// Check ray is not parallel to plane
	if ((RayDir | APlane) == 0.0f)
	{
		T = -1.0f;
		Intersection = FVector::ZeroVector;
		return false;
	}

	T = ((APlane.W - (LineStart | APlane)) / (RayDir | APlane));

	// Check intersection is not outside line segment
	if (T < 0.0f || T > 1.0f)
	{
		Intersection = FVector::ZeroVector;
		return false;
	}

	// Calculate intersection point
	Intersection = LineStart + RayDir * T;

	return true;
}

bool UKismetMathLibrary::LinePlaneIntersection_OriginNormal(const FVector& LineStart, const FVector& LineEnd, FVector PlaneOrigin, FVector PlaneNormal, float& T, FVector& Intersection)
{
	FVector RayDir = LineEnd - LineStart;

	// Check ray is not parallel to plane
	if ((RayDir | PlaneNormal) == 0.0f)
	{
		T = -1.0f;
		Intersection = FVector::ZeroVector;
		return false;
	}

	T = (((PlaneOrigin - LineStart) | PlaneNormal) / (RayDir | PlaneNormal));

	// Check intersection is not outside line segment
	if (T < 0.0f || T > 1.0f)
	{
		Intersection = FVector::ZeroVector;
		return false;
	}

	// Calculate intersection point
	Intersection = LineStart + RayDir * T;

	return true;
}

void UKismetMathLibrary::BreakRandomStream(const FRandomStream& InRandomStream, int32& InitialSeed)
{
	InitialSeed = InRandomStream.GetInitialSeed();
}

FRandomStream UKismetMathLibrary::MakeRandomStream(int32 InitialSeed)
{
	return FRandomStream(InitialSeed);
}

#undef LOCTEXT_NAMESPACE

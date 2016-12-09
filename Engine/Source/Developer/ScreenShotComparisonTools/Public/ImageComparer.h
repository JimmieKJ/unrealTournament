// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ImageComparer.generated.h"

class Error;
class FComparableImage;

/**
 * 
 */
USTRUCT()
struct FImageTolerance
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY()
	uint8 Red;

	UPROPERTY()
	uint8 Green;

	UPROPERTY()
	uint8 Blue;

	UPROPERTY()
	uint8 Alpha;

	UPROPERTY()
	uint8 MinBrightness;

	UPROPERTY()
	uint8 MaxBrightness;

	UPROPERTY()
	bool IgnoreAntiAliasing;

	UPROPERTY()
	bool IgnoreColors;

	UPROPERTY()
	float MaximumLocalError;

	UPROPERTY()
	float MaximumGlobalError;

	FImageTolerance()
		: Red(0)
		, Green(0)
		, Blue(0)
		, Alpha(0)
		, MinBrightness(0)
		, MaxBrightness(255)
		, IgnoreAntiAliasing(false)
		, IgnoreColors(false)
		, MaximumLocalError(0.0f)
		, MaximumGlobalError(0.0f)
	{
	}

	FImageTolerance(uint8 R, uint8 G, uint8 B, uint8 A, uint8 InMinBrightness, uint8 InMaxBrightness, bool InIgnoreAntiAliasing, bool InIgnoreColors, float InMaximumLocalError, float InMaximumGlobalError)
		: Red(R)
		, Green(G)
		, Blue(B)
		, Alpha(A)
		, MinBrightness(InMinBrightness)
		, MaxBrightness(InMaxBrightness)
		, IgnoreAntiAliasing(InIgnoreAntiAliasing)
		, IgnoreColors(InIgnoreColors)
		, MaximumLocalError(InMaximumLocalError)
		, MaximumGlobalError(InMaximumGlobalError)
	{
	}

public:
	const static FImageTolerance DefaultIgnoreNothing;
	const static FImageTolerance DefaultIgnoreLess;
	const static FImageTolerance DefaultIgnoreAntiAliasing;
	const static FImageTolerance DefaultIgnoreColors;
};

class FComparableImage;

class FPixelOperations
{
public:
	static FORCEINLINE float GetBrightness(const FColor& Color)
	{
		// https://en.wikipedia.org/wiki/Relative_luminance
		return 0.2126 * Color.R + 0.7152 * Color.G + 0.0722 * Color.B;
	}

	static bool IsBrightnessSimilar(const FColor& ColorA, const FColor& ColorB, const FImageTolerance& Tolerance)
	{
		const bool AlphaSimilar = FMath::IsNearlyEqual((float)ColorA.A, ColorB.A, Tolerance.Alpha);

		const float BrightnessA = FPixelOperations::GetBrightness(ColorA);
		const float BrightnessB = FPixelOperations::GetBrightness(ColorB);
		const bool BrightnessSimilar = FMath::IsNearlyEqual(BrightnessA, BrightnessB, Tolerance.MinBrightness);

		return BrightnessSimilar && AlphaSimilar;
	}

	static FORCEINLINE bool IsRGBSame(const FColor& ColorA, const FColor& ColorB)
	{
		return ColorA.R == ColorB.R &&
			ColorA.G == ColorB.G &&
			ColorA.B == ColorB.B;
	}

	static FORCEINLINE bool IsRGBSimilar(const FColor& ColorA, const FColor& ColorB, const FImageTolerance& Tolerance)
	{
		const bool RedSimilar = FMath::IsNearlyEqual((float)ColorA.R, ColorB.R, Tolerance.Red);
		const bool GreenSimilar = FMath::IsNearlyEqual((float)ColorA.G, ColorB.G, Tolerance.Green);
		const bool BlueSimilar = FMath::IsNearlyEqual((float)ColorA.B, ColorB.B, Tolerance.Blue);
		const bool AlphaSimilar = FMath::IsNearlyEqual((float)ColorA.A, ColorB.A, Tolerance.Alpha);

		return RedSimilar && GreenSimilar && BlueSimilar && AlphaSimilar;
	}

	static FORCEINLINE bool IsContrasting(const FColor& ColorA, const FColor& ColorB, const FImageTolerance& Tolerance)
	{
		const float BrightnessA = FPixelOperations::GetBrightness(ColorA);
		const float BrightnessB = FPixelOperations::GetBrightness(ColorB);

		return FMath::Abs(BrightnessA - BrightnessB) > Tolerance.MaxBrightness;
	}

	static float GetHue(const FColor& Color);

	static bool IsAntialiased(const FColor& SourcePixel, FComparableImage* Image, int32 X, int32 Y, const FImageTolerance& Tolerance);
};

/**
 *
 */
class FComparableImage
{
public:
	int32 Width;
	int32 Height;
	TArray<uint8> Bytes;

	FComparableImage()
		: RedTotal(0)
		, GreenTotal(0)
		, BlueTotal(0)
		, AlphaTotal(0)
		, BrightnessTotal(0)
		, RedAverage(0)
		, GreenAverage(0)
		, BlueAverage(0)
		, AlphaAverage(0)
		, BbrightnessAverage(0)
	{
	}

	FORCEINLINE bool CanGetPixel(int32 X, int32 Y)
	{
		return X >= 0 && Y >= 0 && X < Width && Y < Height;
	}

	FORCEINLINE FColor GetPixel(int32 X, int32 Y)
	{
		int32 Offset = ( Y * Width + X ) * 4;
		check(Offset < ( Width * Height * 4 ));

		return FColor(
			Bytes[Offset],
			Bytes[Offset + 1],
			Bytes[Offset + 2],
			Bytes[Offset + 3]);
	}

	void Process();

public:
	// Processed Data
	double RedTotal;
	double GreenTotal;
	double BlueTotal;
	double AlphaTotal;
	double BrightnessTotal;

	double RedAverage;
	double GreenAverage;
	double BlueAverage;
	double AlphaAverage;
	double BbrightnessAverage;
};

/**
 *
 */
USTRUCT()
struct FImageComparisonResult
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY()
	FString ApprovedFile;

	UPROPERTY()
	FString IncomingFile;

	UPROPERTY()
	FString ComparisonFile;

	UPROPERTY()
	double MaxLocalDifference;

	UPROPERTY()
	double GlobalDifference;

	UPROPERTY()
	FText ErrorMessage;

	UPROPERTY()
	FImageTolerance Tolerance;

	FImageComparisonResult()
		: MaxLocalDifference(0.0f)
		, GlobalDifference(0.0f)
		, ErrorMessage()
	{
	}

	FImageComparisonResult(const FText& Error)
		: MaxLocalDifference(0.0f)
		, GlobalDifference(0.0f)
		, ErrorMessage(Error)
	{
	}

	bool IsNew() const
	{
		return ApprovedFile.IsEmpty();
	}

	bool AreSimilar() const
	{
		if ( IsNew() )
		{
			return false;
		}

		if ( MaxLocalDifference > Tolerance.MaximumLocalError || GlobalDifference > Tolerance.MaximumGlobalError )
		{
			return false;
		}

		return true;
	}
};

/**
 *
 */
USTRUCT()
struct FComparisonResults
{
	GENERATED_USTRUCT_BODY()

public:

	FComparisonResults()
		: ApprovedPath(TEXT("Approved"))
		, IncomingPath(TEXT("Incoming"))
		, DeltaPath(TEXT("Delta"))
	{
	}

	UPROPERTY()
	FString ApprovedPath;

	UPROPERTY()
	FString IncomingPath;

	UPROPERTY()
	FString DeltaPath;

	UPROPERTY()
	TArray<FImageComparisonResult> Comparisons;

	UPROPERTY()
	TArray<FString> Added;

	UPROPERTY()
	TArray<FString> Missing;
};

/**
 * 
 */
class FImageComparer
{
public:
	FString ImageRootA;
	FString ImageRootB;
	FString DeltaDirectory;

	FImageComparer();

	FImageComparisonResult Compare(const FString& ImagePathA, const FString& ImagePathB, FImageTolerance Tolerance);

private:
	TSharedPtr<FComparableImage> Open(const FString& ImagePath, FText& OutError);
};

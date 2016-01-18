// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavAreas/NavArea.h"
#include "NavLinkDefinition.generated.h"

UENUM()
namespace ENavLinkDirection
{
	enum Type
	{
		BothWays,
		LeftToRight,
		RightToLeft,
	};
}

USTRUCT()
struct ENGINE_API FNavigationLinkBase
{
	GENERATED_USTRUCT_BODY()

	/** if greater than 0 nav system will attempt to project navlink's start point on geometry below */
	UPROPERTY(EditAnywhere, Category=Default, meta=(ClampMin = "0.0"))
	float LeftProjectHeight;

	/** if greater than 0 nav system will attempt to project navlink's end point on geometry below */
	UPROPERTY(EditAnywhere, Category=Default, meta=(ClampMin = "0.0", DisplayName="Right Project Height"))
	float MaxFallDownLength;

	UPROPERTY(EditAnywhere, Category=Default)
	TEnumAsByte<ENavLinkDirection::Type> Direction;

	/** ID passed to navigation data generator */
	int32 UserId;

	UPROPERTY(EditAnywhere, Category=Default, meta=(ClampMin = "1.0"))
	float SnapRadius;

	UPROPERTY(EditAnywhere, Category=Default, meta=(ClampMin = "1.0", EditCondition="bUseSnapHeight"))
	float SnapHeight;

	UPROPERTY()
	uint32 bUseSnapHeight : 1;

	/** If set, link will try to snap to cheapest area in given radius */
	UPROPERTY(EditAnywhere, Category=Default)
	uint32 bSnapToCheapestArea : 1;

	/** Area type of this link (empty = default) */
	UPROPERTY(EditAnywhere, Category=Default)
	TSubclassOf<class UNavArea> AreaClass;

	/** restrict area only to specified agents */
	UPROPERTY(EditAnywhere, Category=Default)
	FNavAgentSelector SupportedAgents;

	// DEPRECATED AGENT CONFIG
#if CPP
	union
	{
		struct
		{
#endif
	UPROPERTY()
	uint32 bSupportsAgent0 : 1;
	UPROPERTY()
	uint32 bSupportsAgent1 : 1;
	UPROPERTY()
	uint32 bSupportsAgent2 : 1;
	UPROPERTY()
	uint32 bSupportsAgent3 : 1;
	UPROPERTY()
	uint32 bSupportsAgent4 : 1;
	UPROPERTY()
	uint32 bSupportsAgent5 : 1;
	UPROPERTY()
	uint32 bSupportsAgent6 : 1;
	UPROPERTY()
	uint32 bSupportsAgent7 : 1;
	UPROPERTY()
	uint32 bSupportsAgent8 : 1;
	UPROPERTY()
	uint32 bSupportsAgent9 : 1;
	UPROPERTY()
	uint32 bSupportsAgent10 : 1;
	UPROPERTY()
	uint32 bSupportsAgent11 : 1;
	UPROPERTY()
	uint32 bSupportsAgent12 : 1;
	UPROPERTY()
	uint32 bSupportsAgent13 : 1;
	UPROPERTY()
	uint32 bSupportsAgent14 : 1;
	UPROPERTY()
	uint32 bSupportsAgent15 : 1;
#if CPP
		};
		uint32 SupportedAgentsBits;
	};
#endif

#if WITH_EDITORONLY_DATA
	/** this is an editor-only property to put descriptions in navlinks setup, to be able to identify it easier */
	UPROPERTY(EditAnywhere, Category=Default)
	FString Description;
#endif // WITH_EDITORONLY_DATA

	FNavigationLinkBase();

	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits< FNavigationLinkBase > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithPostSerialize = true,
	};
};

USTRUCT()
struct ENGINE_API FNavigationLink : public FNavigationLinkBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Default, BlueprintReadWrite, meta=(MakeEditWidget=""))
	FVector Left;

	UPROPERTY(EditAnywhere, Category=Default, BlueprintReadWrite, meta=(MakeEditWidget=""))
	FVector Right;

	FNavigationLink()
		: Left(0,-50, 0), Right(0, 50, 0)
	{}

	FNavigationLink(const FVector& InLeft, const FVector& InRight) 
		: Left(InLeft), Right(InRight)
	{}

	FORCEINLINE FNavigationLink Transform(const FTransform& Transformation) const
	{
		FNavigationLink Result = *this;
		Result.Left = Transformation.TransformPosition(Result.Left);
		Result.Right = Transformation.TransformPosition(Result.Right);

		return Result;
	}

	FORCEINLINE FNavigationLink Translate(const FVector& Translation) const
	{
		FNavigationLink Result = *this;
		Result.Left += Translation;
		Result.Right += Translation;

		return Result;
	}

	FORCEINLINE FNavigationLink Rotate(const FRotator& Rotation) const
	{
		FNavigationLink Result = *this;
		
		Result.Left = Rotation.RotateVector(Result.Left);
		Result.Right = Rotation.RotateVector(Result.Right);

		return Result;
	}

	void PostSerialize(const FArchive& Ar)
	{
		FNavigationLinkBase::PostSerialize(Ar);
	}
};

template<>
struct TStructOpsTypeTraits< FNavigationLink > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithPostSerialize = true,
	};
};

USTRUCT()
struct ENGINE_API FNavigationSegmentLink : public FNavigationLinkBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Default, BlueprintReadWrite, meta=(MakeEditWidget=""))
	FVector LeftStart;

	UPROPERTY(EditAnywhere, Category=Default, BlueprintReadWrite, meta=(MakeEditWidget=""))
	FVector LeftEnd;

	UPROPERTY(EditAnywhere, Category=Default, BlueprintReadWrite, meta=(MakeEditWidget=""))
	FVector RightStart;

	UPROPERTY(EditAnywhere, Category=Default, BlueprintReadWrite, meta=(MakeEditWidget=""))
	FVector RightEnd;

	FNavigationSegmentLink() 
		: LeftStart(-25, -50, 0), LeftEnd(25, -50,0), RightStart(-25, 50, 0), RightEnd(25, 50, 0) 
	{}

	FNavigationSegmentLink(const FVector& InLeftStart, const FVector& InLeftEnd, const FVector& InRightStart, const FVector& InRightEnd)
		: LeftStart(InLeftStart), LeftEnd(InLeftEnd), RightStart(InRightStart), RightEnd(InRightEnd)
	{}

	FORCEINLINE FNavigationSegmentLink Transform(const FTransform& Transformation) const
	{
		FNavigationSegmentLink Result = *this;
		Result.LeftStart = Transformation.TransformPosition(Result.LeftStart);
		Result.LeftEnd = Transformation.TransformPosition(Result.LeftEnd);
		Result.RightStart = Transformation.TransformPosition(Result.RightStart);
		Result.RightEnd = Transformation.TransformPosition(Result.RightEnd);

		return Result;
	}

	FORCEINLINE FNavigationSegmentLink Translate(const FVector& Translation) const
	{
		FNavigationSegmentLink Result = *this;
		Result.LeftStart += Translation;
		Result.LeftEnd += Translation;
		Result.RightStart += Translation;
		Result.RightEnd += Translation;

		return Result;
	}

	void PostSerialize(const FArchive& Ar)
	{
		FNavigationLinkBase::PostSerialize(Ar);
	}
};

template<>
struct TStructOpsTypeTraits< FNavigationSegmentLink > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithPostSerialize = true,
	};
};

/** Class containing definition of a navigation area */
UCLASS(abstract, Config=Engine, Blueprintable)
class ENGINE_API UNavLinkDefinition : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=OffMeshLinks)
	TArray<FNavigationLink> Links;

	UPROPERTY(EditAnywhere, Category=OffMeshLinks)
	TArray<FNavigationSegmentLink> SegmentLinks;

	static const TArray<FNavigationLink>& GetLinksDefinition(class UClass* LinkDefinitionClass);
	static const TArray<FNavigationSegmentLink>& GetSegmentLinksDefinition(class UClass* LinkDefinitionClass);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	bool HasMetaAreaClass() const;
	bool HasAdjustableLinks() const;

private:
	// These booleans are mutable to cache the restul for HasMetaAreaClass();
	mutable uint32 bHasDeterminedMetaAreaClass : 1;
	mutable uint32 bHasMetaAreaClass : 1;
	mutable uint32 bHasDeterminedAdjustableLinks : 1;
	mutable uint32 bHasAdjustableLinks : 1;
};

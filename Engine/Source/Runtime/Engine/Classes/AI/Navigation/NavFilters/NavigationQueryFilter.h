// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationQueryFilter.generated.h"

class UNavArea;
class ANavigationData;
struct FNavigationQueryFilter;

USTRUCT()
struct ENGINE_API FNavigationFilterArea
{
	GENERATED_USTRUCT_BODY()

	/** navigation area class */
	UPROPERTY(EditAnywhere, Category=Area)
	TSubclassOf<UNavArea> AreaClass;

	/** override for travel cost */
	UPROPERTY(EditAnywhere, Category=Area, meta=(EditCondition="bOverrideTravelCost",ClampMin=1))
	float TravelCostOverride;

	/** override for entering cost */
	UPROPERTY(EditAnywhere, Category=Area, meta=(EditCondition="bOverrideEnteringCost",ClampMin=0))
	float EnteringCostOverride;

	/** mark as excluded */
	UPROPERTY(EditAnywhere, Category=Area)
	uint32 bIsExcluded : 1;

	UPROPERTY()
	uint32 bOverrideTravelCost : 1;

	UPROPERTY()
	uint32 bOverrideEnteringCost : 1;

	FNavigationFilterArea() : TravelCostOverride(1.0f) {}
};

// 
// Use UNavigationSystem.DescribeFilterFlags() to setup user friendly names of flags
// 
USTRUCT()
struct ENGINE_API FNavigationFilterFlags
{
	GENERATED_USTRUCT_BODY()

#if CPP
	union
	{
		struct
		{
#endif
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag0 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag1 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag2 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag3 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag4 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag5 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag6 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag7 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag8 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag9 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag10 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag11 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag12 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag13 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag14 : 1;
	UPROPERTY(EditAnywhere, Category=Flags)
	uint32 bNavFlag15 : 1;
#if CPP
		};
		uint16 Packed;
	};
#endif
};

class INavigationQueryFilterInterface
{
public:
	virtual ~INavigationQueryFilterInterface(){}

	virtual void Reset() = 0;

	virtual void SetAreaCost(uint8 AreaType, float Cost) = 0;
	virtual void SetFixedAreaEnteringCost(uint8 AreaType, float Cost) = 0;
	virtual void SetExcludedArea(uint8 AreaType) = 0;
	virtual void SetAllAreaCosts(const float* CostArray, const int32 Count) = 0;
	virtual void GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const = 0;
	virtual void SetBacktrackingEnabled(const bool bBacktracking) = 0;
	virtual bool IsBacktrackingEnabled() const = 0;
	virtual bool IsEqual(const INavigationQueryFilterInterface* Other) const = 0;
	virtual void SetIncludeFlags(uint16 Flags) = 0;
	virtual uint16 GetIncludeFlags() const = 0;
	virtual void SetExcludeFlags(uint16 Flags) = 0;
	virtual uint16 GetExcludeFlags() const = 0;

	virtual INavigationQueryFilterInterface* CreateCopy() const = 0;
};

struct ENGINE_API FNavigationQueryFilter : public TSharedFromThis<FNavigationQueryFilter>
{
	FNavigationQueryFilter() : QueryFilterImpl(NULL), MaxSearchNodes(DefaultMaxSearchNodes) {}
private:
	FNavigationQueryFilter(const FNavigationQueryFilter& Source);
	FNavigationQueryFilter(const FNavigationQueryFilter* Source);
	FNavigationQueryFilter(const TSharedPtr<FNavigationQueryFilter> Source);
	FNavigationQueryFilter& operator=(const FNavigationQueryFilter& Source);
public:

	/** set travel cost for area */
	void SetAreaCost(uint8 AreaType, float Cost);

	/** set entering cost for area */
	void SetFixedAreaEnteringCost(uint8 AreaType, float Cost);

	/** mark area as excluded from path finding */
	void SetExcludedArea(uint8 AreaType);

	/** set travel cost for all areas */
	void SetAllAreaCosts(const TArray<float>& CostArray);
	void SetAllAreaCosts(const float* CostArray, const int32 Count);

	/** get travel & entering costs for all areas */
	void GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const;

	/** set required flags of navigation nodes */
	void SetIncludeFlags(uint16 Flags);

	/** get required flags of navigation nodes */
	uint16 GetIncludeFlags() const;

	/** set forbidden flags of navigation nodes */
	void SetExcludeFlags(uint16 Flags);

	/** get forbidden flags of navigation nodes */
	uint16 GetExcludeFlags() const;

	/** set node limit for A* loop */
	void SetMaxSearchNodes(const uint32 MaxNodes) { MaxSearchNodes = MaxNodes; }

	/** get node limit for A* loop */
	FORCEINLINE uint32 GetMaxSearchNodes() const { return MaxSearchNodes; }

	/** mark filter as backtracking - parse directional links in opposite direction
	*  (find path from End to Start, but all links works like on path from Start to End) */
	void SetBacktrackingEnabled(const bool bBacktracking) { QueryFilterImpl->SetBacktrackingEnabled(bBacktracking); }

	/** get backtracking status */
	bool IsBacktrackingEnabled() const { return QueryFilterImpl->IsBacktrackingEnabled(); }

	template<typename FilterType>
	void SetFilterType()
	{
		QueryFilterImpl = MakeShareable(new FilterType());
	}

	FORCEINLINE_DEBUGGABLE void SetFilterImplementation(const INavigationQueryFilterInterface* InQueryFilterImpl)
	{
		QueryFilterImpl = MakeShareable(InQueryFilterImpl->CreateCopy());
	}

	FORCEINLINE const INavigationQueryFilterInterface* GetImplementation() const { return QueryFilterImpl.Get(); }
	FORCEINLINE INavigationQueryFilterInterface* GetImplementation() { return QueryFilterImpl.Get(); }
	void Reset() { GetImplementation()->Reset(); }

	TSharedPtr<FNavigationQueryFilter> GetCopy() const;

	FORCEINLINE bool operator==(const FNavigationQueryFilter& Other) const
	{
		const INavigationQueryFilterInterface* Impl0 = GetImplementation();
		const INavigationQueryFilterInterface* Impl1 = Other.GetImplementation();
		return Impl0 && Impl1 && Impl0->IsEqual(Impl1);
	}

	static const uint32 DefaultMaxSearchNodes;

protected:
	void Assign(const FNavigationQueryFilter& Source);

	TSharedPtr<INavigationQueryFilterInterface, ESPMode::ThreadSafe> QueryFilterImpl;
	uint32 MaxSearchNodes;
};

/** Class containing definition of a navigation query filter */
UCLASS(Abstract, Blueprintable)
class ENGINE_API UNavigationQueryFilter : public UObject
{
	GENERATED_UCLASS_BODY()
	
	/** list of overrides for navigation areas */
	UPROPERTY(EditAnywhere, Category=Filter)
	TArray<FNavigationFilterArea> Areas;

	/** required flags of navigation nodes */
	UPROPERTY(EditAnywhere, Category=Filter)
	FNavigationFilterFlags IncludeFlags;

	/** forbidden flags of navigation nodes */
	UPROPERTY(EditAnywhere, Category=Filter)
	FNavigationFilterFlags ExcludeFlags;

	/** get filter for given navigation data and initialize on first access */
	TSharedPtr<const FNavigationQueryFilter> GetQueryFilter(const ANavigationData& NavData) const;
	
	/** helper functions for accessing filter */
	static TSharedPtr<const FNavigationQueryFilter> GetQueryFilter(const ANavigationData& NavData, TSubclassOf<UNavigationQueryFilter> FilterClass);

	template<class T>
	static TSharedPtr<const FNavigationQueryFilter> GetQueryFilter(const ANavigationData& NavData, TSubclassOf<UNavigationQueryFilter> FilterClass = T::StaticClass())
	{
		return GetQueryFilter(NavData, FilterClass);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** helper functions for adding area overrides */
	void AddTravelCostOverride(TSubclassOf<UNavArea> AreaClass, float TravelCost);
	void AddEnteringCostOverride(TSubclassOf<UNavArea> AreaClass, float EnteringCost);
	void AddExcludedArea(TSubclassOf<UNavArea> AreaClass);

	/** find index of area data */
	int32 FindAreaOverride(TSubclassOf<UNavArea> AreaClass) const;
	
	/** setup filter for given navigation data, use to create custom filters */
	virtual void InitializeFilter(const ANavigationData& NavData, FNavigationQueryFilter& Filter) const;

public:
	//----------------------------------------------------------------------//
	// deprecated
	//----------------------------------------------------------------------//

	DEPRECATED(4.8, "This version of InitializeFilter is deprecated. Please use the version taking NavData and Filter references")
	virtual void InitializeFilter(const ANavigationData* NavData, FNavigationQueryFilter* Filter) const
	{
		if (NavData && Filter)
		{
			InitializeFilter(*NavData, *Filter);
		}
	}

	DEPRECATED(4.8, "This version of GetQueryFilter is deprecated. Please use ANavigationData reference rather than a pointer version")
	TSharedPtr<const FNavigationQueryFilter> GetQueryFilter(const ANavigationData* NavData) const 
	{ 
		return NavData ? GetQueryFilter(*NavData) : nullptr; 
	}

	DEPRECATED(4.8, "This version of GetQueryFilter is deprecated. Please use ANavigationData reference rather than a pointer version")
	static TSharedPtr<const FNavigationQueryFilter> GetQueryFilter(const ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
	{
		return NavData ? GetQueryFilter(*NavData, FilterClass) : nullptr;
	}

	// will "auto-deprecate" due to the function called inside
	template<class T>
	static TSharedPtr<const FNavigationQueryFilter> GetQueryFilter(const ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass = T::StaticClass())
	{
		return NavData ? GetQueryFilter(*NavData, FilterClass) : nullptr;
	}
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "DataProviders/AIDataProvider_QueryParams.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"
float UEnvQueryTypes::SkippedItemValue = -FLT_MAX;

//----------------------------------------------------------------------//
// FEnvQueryResult
//----------------------------------------------------------------------//
AActor* FEnvQueryResult::GetItemAsActor(int32 Index) const
{
	if (Items.IsValidIndex(Index) &&
		ItemType->IsChildOf(UEnvQueryItemType_ActorBase::StaticClass()))
	{
		UEnvQueryItemType_ActorBase* DefTypeOb = static_cast<UEnvQueryItemType_ActorBase*>(ItemType->GetDefaultObject());
		return DefTypeOb->GetActor(RawData.GetData() + Items[Index].DataOffset);
	}

	return nullptr;
}

FVector FEnvQueryResult::GetItemAsLocation(int32 Index) const
{
	if (Items.IsValidIndex(Index) &&
		ItemType->IsChildOf(UEnvQueryItemType_VectorBase::StaticClass()))
	{
		UEnvQueryItemType_VectorBase* DefTypeOb = static_cast<UEnvQueryItemType_VectorBase*>(ItemType->GetDefaultObject());
		return DefTypeOb->GetItemLocation(RawData.GetData() + Items[Index].DataOffset);
	}

	return FVector::ZeroVector;
}

void FEnvQueryResult::GetAllAsActors(TArray<AActor*>& OutActors) const
{
	if (ItemType->IsChildOf(UEnvQueryItemType_ActorBase::StaticClass()) && Items.Num() > 0)
	{
		UEnvQueryItemType_ActorBase* DefTypeOb = static_cast<UEnvQueryItemType_ActorBase*>(ItemType->GetDefaultObject());
		
		OutActors.Reserve(OutActors.Num() + Items.Num());

		for (const FEnvQueryItem& Item : Items)
		{
			OutActors.Add(DefTypeOb->GetActor(RawData.GetData() + Item.DataOffset));
		}
	}
}

void FEnvQueryResult::GetAllAsLocations(TArray<FVector>& OutLocations) const
{
	if (ItemType->IsChildOf(UEnvQueryItemType_VectorBase::StaticClass()) && Items.Num() > 0)
	{
		UEnvQueryItemType_VectorBase* DefTypeOb = static_cast<UEnvQueryItemType_VectorBase*>(ItemType->GetDefaultObject());

		OutLocations.Reserve(OutLocations.Num() + Items.Num());

		for (const FEnvQueryItem& Item : Items)
		{
			OutLocations.Add(DefTypeOb->GetItemLocation(RawData.GetData() + Item.DataOffset));
		}
	}
}

//----------------------------------------------------------------------//
// UEnvQueryTypes
//----------------------------------------------------------------------//
FText UEnvQueryTypes::GetShortTypeName(const UObject* Ob)
{
	if (Ob == NULL)
	{
		return LOCTEXT("Unknown", "unknown");
	}

	const UClass* ObClass = Ob->IsA(UClass::StaticClass()) ? (const UClass*)Ob : Ob->GetClass();
	if (ObClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return FText::FromString(ObClass->GetName().LeftChop(2));
	}

	FString TypeDesc = ObClass->GetName();
	const int32 ShortNameIdx = TypeDesc.Find(TEXT("_"));
	if (ShortNameIdx != INDEX_NONE)
	{
		TypeDesc = TypeDesc.Mid(ShortNameIdx + 1);
	}

	return FText::FromString(TypeDesc);
}

FText UEnvQueryTypes::DescribeContext(TSubclassOf<UEnvQueryContext> ContextClass)
{
	return GetShortTypeName(ContextClass);
}

FText FEnvDirection::ToText() const
{
	if(DirMode == EEnvDirection::TwoPoints)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("LineFrom"), UEnvQueryTypes::DescribeContext(LineFrom));
		Args.Add(TEXT("LineTo"), UEnvQueryTypes::DescribeContext(LineTo));

		return FText::Format(LOCTEXT("DescribeLineFromAndTo", "[{LineFrom} - {LineTo}]"), Args);
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Rotation"), UEnvQueryTypes::DescribeContext(Rotation));

		return FText::Format(LOCTEXT("DescribeRotation", "[{Rotation} rotation]"), Args);
	}
}

FText FEnvTraceData::ToText(FEnvTraceData::EDescriptionMode DescMode) const
{
	FText Desc;

	if (TraceMode == EEnvQueryTrace::Geometry)
	{
		FNumberFormattingOptions NumberFormatOptions;
		NumberFormatOptions.MaximumFractionalDigits = 2;

		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ExtentX"), FText::AsNumber(ExtentX, &NumberFormatOptions));
			Args.Add(TEXT("ExtentY"), FText::AsNumber(ExtentY, &NumberFormatOptions));
			Args.Add(TEXT("ExtentZ"), FText::AsNumber(ExtentZ, &NumberFormatOptions));

			Desc = (TraceShape == EEnvTraceShape::Line) ? LOCTEXT("Line", "line") :
				(TraceShape == EEnvTraceShape::Sphere) ? FText::Format(LOCTEXT("SphereWithRadius", "sphere (radius: {ExtentX})"), Args) :
				(TraceShape == EEnvTraceShape::Capsule) ? FText::Format(LOCTEXT("CasuleWithRadiusHalfHeight", "capsule (radius: {ExtentX}, half height: {ExtentZ})"), Args) :
				(TraceShape == EEnvTraceShape::Box) ? FText::Format(LOCTEXT("BoxWithExtents", "box (extent: {ExtentX} {ExtentY} {ExtentZ)"), Args) :
				LOCTEXT("Unknown", "unknown");
		}

		if (DescMode == FEnvTraceData::Brief)
		{
			static UEnum* ChannelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETraceTypeQuery"), true);

			FFormatNamedArguments Args;
			Args.Add(TEXT("ExtentDescription"), Desc);
			Args.Add(TEXT("ProjectionTraceDesc"), bCanProjectDown ? LOCTEXT("Projection", "projection") : LOCTEXT("Trace", "trace"));
			Args.Add(TEXT("Channel"), ChannelEnum->GetEnumText(TraceChannel));

			Desc = FText::Format(LOCTEXT("GeometryBriefDescription", "{ExtentDescription} {ProjectionTraceDesc} on {Channel}"), Args);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Description"), Desc);

			if (bTraceComplex)
			{
				Desc = FText::Format(LOCTEXT("DescWithComplexCollision", "{Description}, complex collision"), Args);
			}

			if (!bOnlyBlockingHits)
			{
				Desc = FText::Format(LOCTEXT("DescWithNonBlocking", "{Description}, accept non blocking"), Args);
			}
		}
	}
	else if (TraceMode == EEnvQueryTrace::Navigation)
	{
		Desc = LOCTEXT("Navmesh", "navmesh");
		if (bCanProjectDown)
		{
			FNumberFormattingOptions NumberFormatOptions;
			NumberFormatOptions.MaximumFractionalDigits = 0;

			FFormatNamedArguments Args;
			Args.Add(TEXT("Description"), Desc);
			Args.Add(TEXT("Direction"), (ProjectDown == ProjectUp) ? LOCTEXT("Height", "height") : (ProjectDown > ProjectUp) ? LOCTEXT("Down", "down") : LOCTEXT("Up", "up"));
			Args.Add(TEXT("ProjectionAmount"), FText::AsNumber(FMath::Max(ProjectDown, ProjectUp), &NumberFormatOptions));

			Desc = FText::Format(LOCTEXT("DescriptionWithProjection", "projection ({Direction}: {ProjectionAmount}"), Args);

			if (ExtentX > 1.0f)
			{
				NumberFormatOptions.MaximumFractionalDigits = 2;

				FFormatNamedArguments RadiusArgs;
				RadiusArgs.Add(TEXT("Description"), Desc);
				RadiusArgs.Add(TEXT("Radius"), FText::AsNumber(ExtentX, &NumberFormatOptions));

				Desc = FText::Format(LOCTEXT("DescriptionWithRadius", "{Description}, radius {Radius}"), RadiusArgs);
			}

			FFormatNamedArguments EndingArgs;
			EndingArgs.Add(TEXT("Description"), Desc);
			Desc = FText::Format(LOCTEXT("DescriptionWithEnding", "{Description})"), EndingArgs);
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Description"), Desc);

			Desc = FText::Format(LOCTEXT("DescriptionWithTrace", "{Description} trace"), Args);
			if (NavigationFilter)
			{
				FFormatNamedArguments NavFilterArgs;
				NavFilterArgs.Add(TEXT("Description"), Desc);
				NavFilterArgs.Add(TEXT("NavigationFilter"), FText::FromString(NavigationFilter->GetName()));
				Desc = FText::Format(LOCTEXT("DescriptionWithNavigationFilter", "{Description} (filter: {NavigationFilter})"), NavFilterArgs);
			}
		}
	}

	return Desc;
}

void FEnvTraceData::SetGeometryOnly()
{
	TraceMode = EEnvQueryTrace::Geometry;
	bCanTraceOnGeometry = true;
	bCanTraceOnNavMesh = false;
	bCanDisableTrace = false;
}

void FEnvTraceData::SetNavmeshOnly()
{
	TraceMode = EEnvQueryTrace::Navigation;
	bCanTraceOnGeometry = false;
	bCanTraceOnNavMesh = true;
	bCanDisableTrace = false;
}

void FEnvBoolParam_DEPRECATED::Convert(UObject* Owner, FAIDataProviderBoolValue& ValueProvider)
{
	ValueProvider.DefaultValue = Value;
	if (IsNamedParam())
	{
		UAIDataProvider_QueryParams* ParamsProvider = NewObject<UAIDataProvider_QueryParams>(Owner);
		ParamsProvider->ParamName = ParamName;
		ValueProvider.DataBinding = ParamsProvider;
		ValueProvider.DataField = GET_MEMBER_NAME_CHECKED(UAIDataProvider_QueryParams, BoolValue);
	}
}

void FEnvIntParam_DEPRECATED::Convert(UObject* Owner, FAIDataProviderIntValue& ValueProvider)
{
	ValueProvider.DefaultValue = Value;
	if (IsNamedParam())
	{
		UAIDataProvider_QueryParams* ParamsProvider = NewObject<UAIDataProvider_QueryParams>(Owner);
		ParamsProvider->ParamName = ParamName;
		ValueProvider.DataBinding = ParamsProvider;
		ValueProvider.DataField = GET_MEMBER_NAME_CHECKED(UAIDataProvider_QueryParams, IntValue);
	}
}

void FEnvFloatParam_DEPRECATED::Convert(UObject* Owner, FAIDataProviderFloatValue& ValueProvider)
{
	ValueProvider.DefaultValue = Value;
	if (IsNamedParam())
	{
		UAIDataProvider_QueryParams* ParamsProvider = NewObject<UAIDataProvider_QueryParams>(Owner);
		ParamsProvider->ParamName = ParamName;
		ValueProvider.DataBinding = ParamsProvider;
		ValueProvider.DataField = GET_MEMBER_NAME_CHECKED(UAIDataProvider_QueryParams, FloatValue);
	}
}


//----------------------------------------------------------------------//
// namespace FEQSHelpers
//----------------------------------------------------------------------//
namespace FEQSHelpers
{
	const ANavigationData* FindNavigationDataForQuery(FEnvQueryInstance& QueryInstance)
	{
		const UNavigationSystem* NavSys = QueryInstance.World->GetNavigationSystem();
		
		if (NavSys == nullptr)
		{
			return nullptr;
		}

		// try to match navigation agent for querier
		INavAgentInterface* NavAgent = QueryInstance.Owner.IsValid() ? Cast<INavAgentInterface>(QueryInstance.Owner.Get()) : NULL;
		if (NavAgent)
		{
			const FNavAgentProperties& NavAgentProps = NavAgent ? NavAgent->GetNavAgentPropertiesRef() : FNavAgentProperties::DefaultProperties;
			return NavSys->GetNavDataForProps(NavAgentProps);
		}

		return NavSys->GetMainNavData();
	}
}

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
#if WITH_RECAST

namespace FEQSHelpers
{
	const ARecastNavMesh* FindNavMeshForQuery(FEnvQueryInstance& QueryInstance)
	{
		return Cast<const ARecastNavMesh>(FEQSHelpers::FindNavigationDataForQuery(QueryInstance));
	}
}

#endif // WITH_RECAST

#undef LOCTEXT_NAMESPACE

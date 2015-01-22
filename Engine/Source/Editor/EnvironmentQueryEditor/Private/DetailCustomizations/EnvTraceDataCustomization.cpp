// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "EnvTraceDataCustomization.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

#define LOCTEXT_NAMESPACE "EnvTraceDataCustomization"

TSharedRef<IPropertyTypeCustomization> FEnvTraceDataCustomization::MakeInstance()
{
	return MakeShareable( new FEnvTraceDataCustomization );
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FEnvTraceDataCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(this, &FEnvTraceDataCustomization::GetShortDescription)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	PropTraceMode = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,TraceMode));
	PropTraceShape = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,TraceShape));
	CacheTraceModes(StructPropertyHandle);
}

void FEnvTraceDataCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	if (TraceModes.Num() > 1)
	{
		StructBuilder.AddChildContent(LOCTEXT("TraceMode", "Trace Mode"))
		.NameContent()
		[
			PropTraceMode->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FEnvTraceDataCustomization::OnGetTraceModeContent)
			.ContentPadding(FMargin( 2.0f, 2.0f ))
			.ButtonContent()
			[
				SNew(STextBlock) 
				.Text(this, &FEnvTraceDataCustomization::GetCurrentTraceModeDesc)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}

	// navmesh props
	TSharedPtr<IPropertyHandle> PropNavFilter = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,NavigationFilter));
	StructBuilder.AddChildProperty(PropNavFilter.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetNavigationVisibility)));

	// geometry props
	TSharedPtr<IPropertyHandle> PropTraceChannel = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,TraceChannel));
	StructBuilder.AddChildProperty(PropTraceChannel.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetGeometryVisibility)));

	StructBuilder.AddChildProperty(PropTraceShape.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetGeometryVisibility)));

	// common props
	TSharedPtr<IPropertyHandle> PropExtX = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,ExtentX));
	StructBuilder.AddChildProperty(PropExtX.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetExtentX)));

	TSharedPtr<IPropertyHandle> PropExtY = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,ExtentY));
	StructBuilder.AddChildProperty(PropExtY.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetExtentY)));

	TSharedPtr<IPropertyHandle> PropExtZ = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,ExtentZ));
	StructBuilder.AddChildProperty(PropExtZ.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetExtentZ)));

	// projection props
	TSharedPtr<IPropertyHandle> PropHeightDown = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,ProjectDown));
	StructBuilder.AddChildProperty(PropHeightDown.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetProjectionVisibility)));

	TSharedPtr<IPropertyHandle> PropHeightUp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,ProjectUp));
	StructBuilder.AddChildProperty(PropHeightUp.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetProjectionVisibility)));

	// advanced props
	TSharedPtr<IPropertyHandle> PropPostProjectionVerticalOffset = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData, PostProjectionVerticalOffset));
	StructBuilder.AddChildProperty(PropPostProjectionVerticalOffset.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetProjectionVisibility)));

	TSharedPtr<IPropertyHandle> PropTraceComplex = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,bTraceComplex));
	StructBuilder.AddChildProperty(PropTraceComplex.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetGeometryVisibility)));

	TSharedPtr<IPropertyHandle> PropOnlyBlocking = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData,bOnlyBlockingHits));
	StructBuilder.AddChildProperty(PropOnlyBlocking.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FEnvTraceDataCustomization::GetGeometryVisibility)));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FEnvTraceDataCustomization::CacheTraceModes(TSharedRef<class IPropertyHandle> StructPropertyHandle)
{
	TSharedPtr<IPropertyHandle> PropCanNavMesh = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData, bCanTraceOnNavMesh));
	TSharedPtr<IPropertyHandle> PropCanGeometry = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData, bCanTraceOnGeometry));
	TSharedPtr<IPropertyHandle> PropCanDisable = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData, bCanDisableTrace));
	TSharedPtr<IPropertyHandle> PropCanProject = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEnvTraceData, bCanProjectDown));

	bool bCanNavMesh = false;
	bool bCanGeometry = false;
	bool bCanDisable = false;
	bCanShowProjection = false;
	PropCanNavMesh->GetValue(bCanNavMesh);
	PropCanGeometry->GetValue(bCanGeometry);
	PropCanDisable->GetValue(bCanDisable);
	PropCanProject->GetValue(bCanShowProjection);

	static UEnum* TraceModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EEnvQueryTrace"));
	check(TraceModeEnum);

	TraceModes.Reset();
	if (bCanDisable)
	{
		TraceModes.Add(FStringIntPair(TraceModeEnum->GetEnumText(EEnvQueryTrace::None).ToString(), EEnvQueryTrace::None));
	}
	if (bCanNavMesh)
	{
		TraceModes.Add(FStringIntPair(TraceModeEnum->GetEnumText(EEnvQueryTrace::Navigation).ToString(), EEnvQueryTrace::Navigation));
	}
	if (bCanGeometry)
	{
		TraceModes.Add(FStringIntPair(TraceModeEnum->GetEnumText(EEnvQueryTrace::Geometry).ToString(), EEnvQueryTrace::Geometry));
	}

	ActiveMode = EEnvQueryTrace::None;
	PropTraceMode->GetValue(ActiveMode);
}

void FEnvTraceDataCustomization::OnTraceModeChanged(int32 Index)
{
	ActiveMode = Index;
	PropTraceMode->SetValue(ActiveMode);
}

TSharedRef<SWidget> FEnvTraceDataCustomization::OnGetTraceModeContent()
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (int32 i = 0; i < TraceModes.Num(); i++)
	{
		FUIAction ItemAction( FExecuteAction::CreateSP( this, &FEnvTraceDataCustomization::OnTraceModeChanged, TraceModes[i].Int ) );
		MenuBuilder.AddMenuEntry( FText::FromString( TraceModes[i].Str ), TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

FString FEnvTraceDataCustomization::GetCurrentTraceModeDesc() const
{
	for (int32 i = 0; i < TraceModes.Num(); i++)
	{
		if (TraceModes[i].Int == ActiveMode)
		{
			return TraceModes[i].Str;
		}
	}

	return FString();
}

FString FEnvTraceDataCustomization::GetShortDescription() const
{
	FString Desc;

	switch (ActiveMode)
	{
	case EEnvQueryTrace::Geometry:
		Desc = LOCTEXT("TraceGeom","geometry trace").ToString();
		break;

	case EEnvQueryTrace::Navigation:
		Desc = LOCTEXT("TraceNav","navmesh trace").ToString();
		break;

	case EEnvQueryTrace::None:
		Desc = LOCTEXT("TraceNone","trace disabled").ToString();
		break;

	default: break;
	}

	return Desc;
}

EVisibility FEnvTraceDataCustomization::GetGeometryVisibility() const
{
	return (ActiveMode == EEnvQueryTrace::Geometry) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEnvTraceDataCustomization::GetNavigationVisibility() const
{
	return (ActiveMode == EEnvQueryTrace::Navigation) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEnvTraceDataCustomization::GetProjectionVisibility() const
{
	return (ActiveMode != EEnvQueryTrace::None) && bCanShowProjection ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FEnvTraceDataCustomization::GetExtentX() const
{
	if (ActiveMode == EEnvQueryTrace::Navigation)
	{
		// radius
		return EVisibility::Visible;
	}
	else if (ActiveMode == EEnvQueryTrace::Geometry)
	{
		uint8 EnumValue;
		PropTraceShape->GetValue(EnumValue);

		return (EnumValue != EEnvTraceShape::Line) ? EVisibility::Visible : EVisibility::Collapsed;
	}
		
	return EVisibility::Collapsed;
}

EVisibility FEnvTraceDataCustomization::GetExtentY() const
{
	if (ActiveMode == EEnvQueryTrace::Geometry)
	{
		uint8 EnumValue;
		PropTraceShape->GetValue(EnumValue);

		return (EnumValue == EEnvTraceShape::Box) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

EVisibility FEnvTraceDataCustomization::GetExtentZ() const
{
	if (ActiveMode == EEnvQueryTrace::Geometry)
	{
		uint8 EnumValue;
		PropTraceShape->GetValue(EnumValue);

		return (EnumValue == EEnvTraceShape::Box || EnumValue == EEnvTraceShape::Capsule) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE

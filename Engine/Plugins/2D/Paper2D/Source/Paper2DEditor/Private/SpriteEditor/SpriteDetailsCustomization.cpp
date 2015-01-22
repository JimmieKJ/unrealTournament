// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "SpriteDetailsCustomization.h"

#include "PhysicsEngine/BodySetup.h"
#include "Editor/Documentation/Public/IDocumentation.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PropertyRestriction.h"

#define LOCTEXT_NAMESPACE "SpriteEditor"

//////////////////////////////////////////////////////////////////////////
// FSpriteDetailsCustomization

TSharedRef<IDetailCustomization> FSpriteDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FSpriteDetailsCustomization);
}

FDetailWidgetRow& FSpriteDetailsCustomization::GenerateWarningRow(IDetailCategoryBuilder& WarningCategory, bool bExperimental, const FText& WarningText, const FText& Tooltip, const FString& ExcerptLink, const FString& ExcerptName)
{
	const FText SearchString = WarningText;
	const FSlateBrush* WarningIcon = FEditorStyle::GetBrush(bExperimental ? "PropertyEditor.ExperimentalClass" : "PropertyEditor.EarlyAccessClass");

	FDetailWidgetRow& WarningRow = WarningCategory.AddCustomRow(SearchString)
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			.ToolTip(IDocumentation::Get()->CreateToolTip(Tooltip, nullptr, ExcerptLink, ExcerptName))
			.Visibility(EVisibility::Visible)

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[	SNew(SImage)
				.Image(WarningIcon)
			]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(WarningText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

	return WarningRow;
}

void FSpriteDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Make sure sprite properties are near the top
	IDetailCategoryBuilder& SpriteCategory = DetailLayout.EditCategory("Sprite", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	BuildSpriteSection(SpriteCategory, DetailLayout);

	// Build the rendering category
	IDetailCategoryBuilder& RenderingCategory = DetailLayout.EditCategory("Rendering");
	BuildRenderingSection(RenderingCategory, DetailLayout);

	// Build the collision category
	IDetailCategoryBuilder& CollisionCategory = DetailLayout.EditCategory("Collision");
	BuildCollisionSection(CollisionCategory, DetailLayout);
}

void FSpriteDetailsCustomization::BuildSpriteSection(IDetailCategoryBuilder& SpriteCategory, IDetailLayoutBuilder& DetailLayout)
{
	// Show other normal properties in the sprite category so that desired ordering doesn't get messed up
	SpriteCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceUV));
	SpriteCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceDimension));
	SpriteCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, SourceTexture));
	SpriteCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, DefaultMaterial));
	SpriteCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, Sockets));
	SpriteCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, PixelsPerUnrealUnit));

	// Show/hide the experimental atlas group support based on whether or not it is enabled
	TSharedPtr<IPropertyHandle> AtlasGroupProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, AtlasGroup));
	TAttribute<EVisibility> AtlasGroupPropertyVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FSpriteDetailsCustomization::GetAtlasGroupVisibility));
	SpriteCategory.AddProperty(AtlasGroupProperty, EPropertyLocation::Advanced).Visibility(AtlasGroupPropertyVisibility);

	// Show/hide the custom pivot point based on the pivot mode
	TSharedPtr<IPropertyHandle> PivotModeProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, PivotMode));
	TSharedPtr<IPropertyHandle> CustomPivotPointProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, CustomPivotPoint));
	TAttribute<EVisibility> CustomPivotPointVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSpriteDetailsCustomization::GetCustomPivotVisibility, PivotModeProperty));
	SpriteCategory.AddProperty(PivotModeProperty);
	SpriteCategory.AddProperty(CustomPivotPointProperty).Visibility(CustomPivotPointVisibility);
}

void FSpriteDetailsCustomization::BuildRenderingSection(IDetailCategoryBuilder& RenderingCategory, IDetailLayoutBuilder& DetailLayout)
{
	// Add the rendering geometry mode into the parent container (renamed)
	const FString RenderGeometryTypePropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UPaperSprite, RenderGeometry), GET_MEMBER_NAME_STRING_CHECKED(FSpritePolygonCollection, GeometryType));
	RenderingCategory.AddProperty(DetailLayout.GetProperty(*RenderGeometryTypePropertyPath))
		.DisplayName(LOCTEXT("RenderGeometryType", "Render Geometry Type"));

	// Show the rendering geometry settings
	TSharedRef<IPropertyHandle> RenderGeometry = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, RenderGeometry));
	IDetailPropertyRow& RenderGeometryProperty = RenderingCategory.AddProperty(RenderGeometry);

	// Add the render polygons into advanced (renamed)
	const FString RenderGeometryPolygonsPropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UPaperSprite, RenderGeometry), GET_MEMBER_NAME_STRING_CHECKED(FSpritePolygonCollection, Polygons));
	RenderingCategory.AddProperty(DetailLayout.GetProperty(*RenderGeometryPolygonsPropertyPath), EPropertyLocation::Advanced)
		.DisplayName(LOCTEXT("RenderPolygons", "Render Polygons"));
}

void FSpriteDetailsCustomization::BuildCollisionSection(IDetailCategoryBuilder& CollisionCategory, IDetailLayoutBuilder& DetailLayout)
{
	TSharedPtr<IPropertyHandle> SpriteCollisionDomainProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, SpriteCollisionDomain));
	TAttribute<EVisibility> ParticipatesInPhysics = TAttribute<EVisibility>::Create( TAttribute<EVisibility>::FGetter::CreateSP( this, &FSpriteDetailsCustomization::AnyPhysicsMode, SpriteCollisionDomainProperty) ) ;
	TAttribute<EVisibility> ParticipatesInPhysics3D = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSpriteDetailsCustomization::PhysicsModeMatches, SpriteCollisionDomainProperty, ESpriteCollisionMode::Use3DPhysics));
	TAttribute<EVisibility> ParticipatesInPhysics2D = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSpriteDetailsCustomization::PhysicsModeMatches, SpriteCollisionDomainProperty, ESpriteCollisionMode::Use2DPhysics));

	CollisionCategory.AddProperty(SpriteCollisionDomainProperty);

	// Add a warning bar about 2D collision being experimental
	FText WarningFor2D = LOCTEXT("Experimental2DPhysicsWarning", "2D collision support is *experimental*");
	FText TooltipFor2D = LOCTEXT("Experimental2DPhysicsWarningTooltip", "2D collision support is *experimental* and should not be relied on yet.\n\nRigid body collision detection and response works, but there are only precompiled libraries for Windows currently.\n\nRaycasts are partially supported (and need to be enabled in project settings), but queries, sweeps, or overlap tests are not implemented yet.");
	GenerateWarningRow(CollisionCategory, /*bExperimental=*/ true, WarningFor2D, TooltipFor2D, TEXT("Shared/Editors/SpriteEditor"), TEXT("CollisionDomain2DWarning"))
		.Visibility(ParticipatesInPhysics2D);

	// Add a warning bar if 2D collision queries aren't enabled
	TAttribute<EVisibility> WarnAbout2DQueriesBeingDisabledVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSpriteDetailsCustomization::Get2DPhysicsNotEnabledWarningVisibility, SpriteCollisionDomainProperty));
	FText QueryWarningFor2D = LOCTEXT("Query2DPhysicsWarning", "2D collision queries are disabled");
	FText QueryTooltipFor2D = LOCTEXT("Query2DPhysicsWarningTooltip", "You can enable 2D queries in Project Settings..Physics by setting bEnable2DPhysics to true, otherwise only collision detection and response will work.\n\nNote: Only raycasts are partially supported; other queries, sweeps, and overlap tests are not implemented yet.");
	GenerateWarningRow(CollisionCategory, /*bExperimental=*/ false, QueryWarningFor2D, QueryTooltipFor2D, TEXT("Shared/Editors/SpriteEditor"), TEXT("Disabled2DCollisionQueriesWarning"))
		.Visibility(WarnAbout2DQueriesBeingDisabledVisibility);

	// Add the collision geometry mode into the parent container (renamed)
	{
		// Restrict the diced value
		TSharedPtr<FPropertyRestriction> PreventDicedRestriction = MakeShareable(new FPropertyRestriction(LOCTEXT("CollisionGeometryDoesNotSupportDiced", "Collision geometry can not be set to Diced")));
		PreventDicedRestriction->AddValue(TEXT("Diced"));

		// Find and add the property
		const FString CollisionGeometryTypePropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UPaperSprite, CollisionGeometry), GET_MEMBER_NAME_STRING_CHECKED(FSpritePolygonCollection, GeometryType));
		TSharedPtr<IPropertyHandle> CollisionGeometryTypeProperty = DetailLayout.GetProperty(*CollisionGeometryTypePropertyPath);

		CollisionGeometryTypeProperty->AddRestriction(PreventDicedRestriction.ToSharedRef());

		CollisionCategory.AddProperty(CollisionGeometryTypeProperty)
			.DisplayName(LOCTEXT("CollisionGeometryType", "Collision Geometry Type"))
			.Visibility(ParticipatesInPhysics);
	}

	// Show the collision geometry when not None
	CollisionCategory.AddProperty( DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, CollisionGeometry)) )
		.Visibility(ParticipatesInPhysics);

	// Show the collision thickness only in 3D mode
	CollisionCategory.AddProperty( DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UPaperSprite, CollisionThickness)) )
		.Visibility(ParticipatesInPhysics3D);

	// Add the collision polygons into advanced (renamed)
	const FString CollisionGeometryPolygonsPropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UPaperSprite, CollisionGeometry), GET_MEMBER_NAME_STRING_CHECKED(FSpritePolygonCollection, Polygons));
	CollisionCategory.AddProperty(DetailLayout.GetProperty(*CollisionGeometryPolygonsPropertyPath), EPropertyLocation::Advanced)
		.DisplayName(LOCTEXT("CollisionPolygons", "Collision Polygons"))
		.Visibility(ParticipatesInPhysics);

	// Show the default body instance (and only it) from the body setup (if it exists)
	DetailLayout.HideProperty("BodySetup");
	IDetailPropertyRow& BodySetupDefaultInstance = CollisionCategory.AddProperty("BodySetup.DefaultInstance");
	
	TArray<TWeakObjectPtr<UObject>> SpritesBeingEdited;
	DetailLayout.GetObjectsBeingCustomized(/*out*/ SpritesBeingEdited);

	TArray<UObject*> BodySetupList;
	for (auto WeakSpritePtr : SpritesBeingEdited)
	{
		if (UPaperSprite* Sprite = Cast<UPaperSprite>(WeakSpritePtr.Get()))
		{
			if (UBodySetup* BodySetup = Sprite->BodySetup)
			{
				BodySetupList.Add(BodySetup);
			}
		}
	}
	
	if (BodySetupList.Num() > 0)
	{
		IDetailPropertyRow* DefaultInstanceRow = CollisionCategory.AddExternalProperty(BodySetupList, GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance));
		if (DefaultInstanceRow != nullptr)
		{
			DefaultInstanceRow->Visibility(ParticipatesInPhysics);
		}
	}
}

EVisibility FSpriteDetailsCustomization::PhysicsModeMatches(TSharedPtr<IPropertyHandle> Property, ESpriteCollisionMode::Type DesiredMode) const
{
	if (Property.IsValid())
	{
		uint8 ValueAsByte;
		FPropertyAccess::Result Result = Property->GetValue(/*out*/ ValueAsByte);

		if (Result == FPropertyAccess::Success)
		{
			return (((ESpriteCollisionMode::Type)ValueAsByte) == DesiredMode) ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}

	// If there are multiple values, show all properties
	return EVisibility::Visible;
}

EVisibility FSpriteDetailsCustomization::AnyPhysicsMode(TSharedPtr<IPropertyHandle> Property) const
{
	if (Property.IsValid())
	{
		uint8 ValueAsByte;
		FPropertyAccess::Result Result = Property->GetValue(/*out*/ ValueAsByte);

		if (Result == FPropertyAccess::Success)
		{
			return (((ESpriteCollisionMode::Type)ValueAsByte) != ESpriteCollisionMode::None) ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}

	// If there are multiple values, show all properties
	return EVisibility::Visible;
}

EVisibility FSpriteDetailsCustomization::GetAtlasGroupVisibility()
{
	return GetDefault<UPaperRuntimeSettings>()->bEnableSpriteAtlasGroups ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FSpriteDetailsCustomization::GetCustomPivotVisibility(TSharedPtr<IPropertyHandle> Property) const
{
	if (Property.IsValid())
	{
		uint8 ValueAsByte;
		FPropertyAccess::Result Result = Property->GetValue(/*out*/ ValueAsByte);

		if (Result == FPropertyAccess::Success)
		{
			return (((ESpritePivotMode::Type)ValueAsByte) == ESpritePivotMode::Custom) ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}

	// If there are multiple values, show all properties
	return EVisibility::Visible;
}

EVisibility FSpriteDetailsCustomization::Get2DPhysicsNotEnabledWarningVisibility(TSharedPtr<IPropertyHandle> Property) const
{
	// Hide the warning if 2D queries are enabled, or if the collision mode is not 2D physics
	return GetDefault<UPhysicsSettings>()->bEnable2DPhysics ? EVisibility::Collapsed : PhysicsModeMatches(Property, ESpriteCollisionMode::Use2DPhysics);
}

#undef LOCTEXT_NAMESPACE

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PlacementModePrivatePCH.h"
#include "AssetSelection.h"
#include "PlacementMode.h"
#include "IPlacementModeModule.h"
#include "SPlacementModeTools.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "IBspModeModule.h"
#include "KismetEditorUtilities.h"
#include "EditorClassUtils.h"
#include "SSearchBox.h"
#include "SWidgetSwitcher.h"
#include "GameFramework/Volume.h"
#include "Engine/PostProcessVolume.h"

/**
 * These are the tab indexes, if the tabs are reorganized you need to adjust the
 * enum accordingly.
 */
namespace EPlacementTab
{
	enum Type
	{
		RecentlyPlaced = 0,
		Basic,
		Lights,
		Visual,
		Geometry,
		Volumes,
		AllClasses,
	};
}

/**
 * These are the asset thumbnails.
 */
class SPlacementAssetThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPlacementAssetThumbnail )
		: _Width( 32 )
		, _Height( 32 )
	{}

	SLATE_ARGUMENT( uint32, Width )

	SLATE_ARGUMENT( uint32, Height )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const FAssetData& InAsset )
	{
		Asset = InAsset;

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
		TSharedPtr<FAssetThumbnailPool> ThumbnailPool = LevelEditorModule.GetFirstLevelEditor()->GetThumbnailPool();

		Thumbnail = MakeShareable( new FAssetThumbnail( Asset, InArgs._Width, InArgs._Height, ThumbnailPool ) );

		bool bAllowFadeIn = false;
		bool bForceGenericThumbnail = false;
		EThumbnailLabel::Type ThumbnailLabel = EThumbnailLabel::ClassName;
		const TAttribute< FText >& HighlightedText = TAttribute< FText >( FText::GetEmpty() );
		const TAttribute< FLinearColor >& HintColorAndOpacity = FLinearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		bool AllowHintText = true;
		FName ClassThumbnailBrushOverride = NAME_None;
		bool ShowBackground = false;

		ChildSlot
		[
			Thumbnail->MakeThumbnailWidget(
			bAllowFadeIn,
			bForceGenericThumbnail,
			ThumbnailLabel,
			HighlightedText,
			HintColorAndOpacity,
			AllowHintText,
			ClassThumbnailBrushOverride,
			ShowBackground )
		];
	}

private:

	FAssetData Asset;
	TSharedPtr< FAssetThumbnail > Thumbnail;
};

void SPlacementAssetEntry::Construct(const FArguments& InArgs, UActorFactory* InFactory, const FAssetData& InAsset)
{	
	bIsPressed = false;

	FactoryToUse = InFactory;
	AssetData = InAsset;

	TSharedPtr< SHorizontalBox > ActorType = SNew( SHorizontalBox );

	const bool IsClass = AssetData.GetClass() == UClass::StaticClass();
	const bool IsVolume = IsClass ? CastChecked<UClass>( AssetData.GetAsset() )->IsChildOf( AVolume::StaticClass() ) : false;

	if ( IsClass )
	{
		AssetDisplayName = FText::FromString( FName::NameToDisplayString( AssetData.AssetName.ToString(), false ) );
	}
	else
	{
		AssetDisplayName = FText::FromName( AssetData.AssetName );
	}

	FText ActorTypeDisplayName;
	AActor* DefaultActor = nullptr;
	if ( FactoryToUse != nullptr )
	{
		DefaultActor = FactoryToUse->GetDefaultActor( AssetData );
		ActorTypeDisplayName = FactoryToUse->GetDisplayName();
	}
	else if ( IsClass && CastChecked<UClass>( AssetData.GetAsset() )->IsChildOf( AActor::StaticClass() ) )
	{
		DefaultActor = CastChecked<AActor>( CastChecked<UClass>( AssetData.GetAsset() )->ClassDefaultObject );
		ActorTypeDisplayName = FText::FromString( FName::NameToDisplayString( DefaultActor->GetClass()->GetName(), false ) );
	}
	
	UClass* DocClass = nullptr;
	TSharedPtr<IToolTip> ToolTip;
	if(DefaultActor != nullptr)
	{
		DocClass = DefaultActor->GetClass();
		ToolTip = FEditorClassUtils::GetTooltip(DefaultActor->GetClass());
	}

	if ( InArgs._LabelOverride.IsEmpty() )
	{
		if (IsClass && !IsVolume && !ActorTypeDisplayName.IsEmpty())
		{
			AssetDisplayName = ActorTypeDisplayName;
		}
	}
	else
	{
		AssetDisplayName = InArgs._LabelOverride;
	}

	if (!ToolTip.IsValid())
	{
		ToolTip = FSlateApplicationBase::Get().MakeToolTip(AssetDisplayName);
	}
	
	const FButtonStyle& ButtonStyle = FEditorStyle::GetWidgetStyle<FButtonStyle>( "PlacementBrowser.Asset" );

	NormalImage = &ButtonStyle.Normal;
	HoverImage = &ButtonStyle.Hovered;
	PressedImage = &ButtonStyle.Pressed; 

	// Create doc link widget if there is a class to link to
	TSharedRef<SWidget> DocWidget = SNew(SSpacer);
	if(DocClass != NULL)
	{
		DocWidget = FEditorClassUtils::GetDocumentationLinkWidget(DocClass);
		DocWidget->SetCursor( EMouseCursor::Default );
	}

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( this, &SPlacementAssetEntry::GetBorder )
		.Cursor( EMouseCursor::GrabHand )
		.ToolTip( ToolTip )
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			.Padding( 0 )
			.AutoWidth()
			[
				// Drop shadow border
				SNew( SBorder )
				.Padding( 4 )
				.BorderImage( FEditorStyle::GetBrush( "ContentBrowser.ThumbnailShadow" ) )
				[
					SNew( SBox )
					.WidthOverride( 35 )
					.HeightOverride( 35 )
					[
						SNew( SPlacementAssetThumbnail, AssetData )
					]
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 4, 0)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.Padding(0, 0, 0, 1)
				.AutoHeight()
				[
					SNew( STextBlock )
					.TextStyle( FEditorStyle::Get(), "PlacementBrowser.Asset.Name" )
					.Text( AssetDisplayName )
					.HighlightText(InArgs._HighlightText)
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				DocWidget
			]
		]
	];
}

FReply SPlacementAssetEntry::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = true;

		return FReply::Handled().DetectDrag( SharedThis( this ), MouseEvent.GetEffectingButton() );
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetEntry::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetEntry::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;

	return FReply::Handled().BeginDragDrop( FAssetDragDropOp::New( AssetData, FactoryToUse ) );
}

bool SPlacementAssetEntry::IsPressed() const
{
	return bIsPressed;
}

const FSlateBrush* SPlacementAssetEntry::GetBorder() const
{
	if ( IsPressed() )
	{
		return PressedImage;
	}
	else if ( IsHovered() )
	{
		return HoverImage;
	}
	else
	{
		return NormalImage;
	}
}

SPlacementModeTools::~SPlacementModeTools()
{
	if ( IPlacementModeModule::IsAvailable() )
	{
		IPlacementModeModule::Get().OnRecentlyPlacedChanged().RemoveAll( this );
	}
}

void SPlacementModeTools::Construct( const FArguments& InArgs )
{
	bPlaceablesRefreshRequested = false;
	bPlaceablesFullRefreshRequested = false;
	bVolumesRefreshRequested = false;

	FPlacementMode* PlacementEditMode = (FPlacementMode*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Placement );
	PlacementEditMode->AddValidFocusTargetForPlacement( SharedThis( this ) );

	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.Padding(4)
		.AutoHeight()
		[
			SNew( SSearchBox )
			.HintText(NSLOCTEXT("PlacementMode", "SearchPlaceables", "Search Classes"))
			.OnTextChanged(this, &SPlacementModeTools::OnSearchChanged)
		]

		+ SVerticalBox::Slot()
		.Padding( 0 )
		.FillHeight( 1.0f )
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SPlacementModeTools::GetSelectedPanel)

			// Normal Panel
			+ SWidgetSwitcher::Slot()
			[
				CreateStandardPanel()
			]

			// Search Results Panel
			+ SWidgetSwitcher::Slot()
			.Padding(4, 0, 4, 4)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				[
					SNew(SScrollBox)

					+ SScrollBox::Slot()
					[
						SAssignNew(SearchResultsContainer, SVerticalBox)
					]
				]
			]
		]
	];

	RefreshRecentlyPlaced();

	IPlacementModeModule::Get().OnRecentlyPlacedChanged().AddSP( this, &SPlacementModeTools::UpdateRecentlyPlacedAssets );
}

TSharedRef< SWidget > SPlacementModeTools::CreateStandardPanel()
{
	return SNew( SHorizontalBox )

	// The tabs on the left
	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.Padding( 0, 3, 0, 0 )
		.AutoHeight()
		[
			SNew( SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMRecentlyPlaced")))
			[
				CreatePlacementGroupTab( (int32)EPlacementTab::RecentlyPlaced, NSLOCTEXT( "PlacementMode", "RecentlyPlaced", "Recently Placed" ), true )
			]
		]


		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMBasic")))
			[
				CreatePlacementGroupTab((int32)EPlacementTab::Basic, NSLOCTEXT("PlacementMode", "Basic", "Basic"), false)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMLights")))
			[
				CreatePlacementGroupTab( (int32)EPlacementTab::Lights, NSLOCTEXT( "PlacementMode", "Lights", "Lights" ), false )
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMVisual")))
			[
				CreatePlacementGroupTab( (int32)EPlacementTab::Visual, NSLOCTEXT( "PlacementMode", "Visual", "Visual" ), false )
			]
		]


		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMGeometry")))
			[
				CreatePlacementGroupTab((int32)EPlacementTab::Geometry, NSLOCTEXT("PlacementMode", "BSP", "BSP"), false)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMVolumes")))
			[
				CreatePlacementGroupTab( (int32)EPlacementTab::Volumes, NSLOCTEXT( "PlacementMode", "Volumes", "Volumes" ), false )
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("PMAllClasses")))
			[
				CreatePlacementGroupTab( (int32)EPlacementTab::AllClasses, NSLOCTEXT( "PlacementMode", "AllClasses", "All Classes" ), true )
			]
		]
	]

	// The 'tab body' area that is switched out with the widget switcher based on the currently active tab.
	+ SHorizontalBox::Slot()
	.FillWidth( 1.0f )
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )
		.Padding( 0 )
		[
			SNew( SBorder )
			.Padding( FMargin( 3 ) )
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.DarkGroupBorder" ) )
			[
				SAssignNew( WidgetSwitcher, SWidgetSwitcher )
				.WidgetIndex( (int32)EPlacementTab::Basic )

				// Recently Placed
				+ SWidgetSwitcher::Slot()
				[
					SNew( SScrollBox )

					+ SScrollBox::Slot()
					[
						SAssignNew( RecentlyPlacedContainer, SVerticalBox )
					]
				]

				// Basics
				+ SWidgetSwitcher::Slot()
				[
					SNew(SScrollBox)

					+ SScrollBox::Slot()
					[
						BuildBasicWidget()
					]
				]

				// Lights
				+ SWidgetSwitcher::Slot()
				[
					SNew( SScrollBox )
							
					+ SScrollBox::Slot()
					[
						BuildLightsWidget()
					]
				]

				// Visual
				+ SWidgetSwitcher::Slot()
				[
					SNew( SScrollBox )
							
					+ SScrollBox::Slot()
					[
						BuildVisualWidget()
					]
				]

				// Geometry
				+SWidgetSwitcher::Slot()
				[
					FModuleManager::LoadModuleChecked<IBspModeModule>("BspMode").CreateBspModeWidget()
				]

				// Volumes
				+ SWidgetSwitcher::Slot()
				[
					SNew( SScrollBox )

					+ SScrollBox::Slot()
					[
						SAssignNew( VolumesContainer, SVerticalBox )
					]
				]

				// Classes
				+ SWidgetSwitcher::Slot()
				[
					SNew( SScrollBox )

					+ SScrollBox::Slot()
					[
						SAssignNew( PlaceablesContainer, SVerticalBox )
					]
				]
			]
		]
	];
}

TSharedRef< SWidget > SPlacementModeTools::CreatePlacementGroupTab( int32 TabIndex, FText TabText, bool Important )
{
	return SNew( SCheckBox )
	.Style( FEditorStyle::Get(), "PlacementBrowser.Tab" )
	.OnCheckStateChanged( this, &SPlacementModeTools::OnPlacementTabChanged, TabIndex )
	.IsChecked( this, &SPlacementModeTools::GetPlacementTabCheckedState, TabIndex )
	[
		SNew( SOverlay )

		+ SOverlay::Slot()
		.VAlign( VAlign_Center )
		[
			SNew(SSpacer)
			.Size( FVector2D( 1, 30 ) )
		]

		+ SOverlay::Slot()
		.Padding( FMargin(6, 0, 15, 0) )
		.VAlign( VAlign_Center )
		[
			SNew( STextBlock )
			.TextStyle( FEditorStyle::Get(), Important ? "PlacementBrowser.Tab.ImportantText" : "PlacementBrowser.Tab.Text" )
			.Text( TabText )
		]

		+ SOverlay::Slot()
		.VAlign( VAlign_Fill )
		.HAlign( HAlign_Left )
		[
			SNew(SImage)
			.Image( this, &SPlacementModeTools::PlacementGroupBorderImage, TabIndex )
		]
	];
}

ECheckBoxState SPlacementModeTools::GetPlacementTabCheckedState( int32 PlacementGroupIndex ) const
{
	return WidgetSwitcher->GetActiveWidgetIndex() == PlacementGroupIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SPlacementModeTools::OnPlacementTabChanged( ECheckBoxState NewState, int32 PlacementGroupIndex )
{
	if ( NewState == ECheckBoxState::Checked )
	{
		WidgetSwitcher->SetActiveWidgetIndex( PlacementGroupIndex );

		if ( PlacementGroupIndex == (int32)EPlacementTab::Volumes )
		{
			bVolumesRefreshRequested = true;
		}
		else if ( PlacementGroupIndex == (int32)EPlacementTab::AllClasses )
		{
			bPlaceablesFullRefreshRequested = true;
		}
	}
}

const FSlateBrush* SPlacementModeTools::PlacementGroupBorderImage( int32 PlacementGroupIndex ) const
{
	if ( WidgetSwitcher->GetActiveWidgetIndex() == PlacementGroupIndex )
	{
		static FName PlacementBrowserActiveTabBarBrush( "PlacementBrowser.ActiveTabBar" );
		return FEditorStyle::GetBrush( PlacementBrowserActiveTabBarBrush );
	}
	else
	{
		return nullptr;
	}
}

TSharedRef< SWidget > BuildDraggableAssetWidget( UClass* InAssetClass )
{
	UActorFactory* Factory = GEditor->FindActorFactoryByClass( InAssetClass );
	FAssetData AssetData = FAssetData( Factory->GetDefaultActorClass( FAssetData() ) );
	return SNew( SPlacementAssetEntry, Factory, AssetData );
}

TSharedRef< SWidget > BuildDraggableAssetWidget( UClass* InAssetClass, const FAssetData& InAssetData )
{
	UActorFactory* Factory = GEditor->FindActorFactoryByClass( InAssetClass );
	return SNew( SPlacementAssetEntry, Factory, InAssetData );
}

TSharedRef< SWidget > SPlacementModeTools::BuildLightsWidget()
{
	return SNew( SVerticalBox )

	// Lights
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryDirectionalLight::StaticClass() )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryPointLight::StaticClass() )
	]
	
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactorySpotLight::StaticClass() )
	]
	
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactorySkyLight::StaticClass() )
	];
}

TSharedRef< SWidget > SPlacementModeTools::BuildVisualWidget()
{
	UActorFactory* PPFactory = GEditor->FindActorFactoryByClassForActorClass( UActorFactoryBoxVolume::StaticClass(), APostProcessVolume::StaticClass() );

	return SNew( SVerticalBox )

	// Visual
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew( SPlacementAssetEntry, PPFactory, FAssetData( APostProcessVolume::StaticClass() ) )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryAtmosphericFog::StaticClass() )
	]
	
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryExponentialHeightFog::StaticClass() )
	]
	
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactorySphereReflectionCapture::StaticClass() )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryBoxReflectionCapture::StaticClass() )
	]
	
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryDeferredDecal::StaticClass() )
	];
}

TSharedRef< SWidget > SPlacementModeTools::BuildBasicWidget()
{
	return SNew( SVerticalBox )

	// Basics
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryCameraActor::StaticClass() )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryPlayerStart::StaticClass() )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget(UActorFactoryPointLight::StaticClass())
	]


	// Triggers
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryTriggerBox::StaticClass() )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryTriggerSphere::StaticClass() )
	]

	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryTriggerCapsule::StaticClass() )
	]


	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		BuildDraggableAssetWidget( UActorFactoryTargetPoint::StaticClass() )
	];
}

void SPlacementModeTools::UpdateRecentlyPlacedAssets( const TArray< FActorPlacementInfo >& RecentlyPlaced )
{
	RefreshRecentlyPlaced();
}

void SPlacementModeTools::RefreshRecentlyPlaced()
{
	RecentlyPlacedContainer->ClearChildren();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( TEXT( "AssetRegistry" ) );

	const TArray< FActorPlacementInfo > RecentlyPlaced = IPlacementModeModule::Get().GetRecentlyPlaced();
	for ( int Index = 0; Index < RecentlyPlaced.Num(); Index++ )
	{
		UObject* Asset = FindObject<UObject>(NULL, *RecentlyPlaced[Index].ObjectPath);
		// If asset is pending delete, it will not be marked as RF_Standalone, in which case we skip it
		if (Asset != nullptr && Asset->HasAnyFlags(RF_Standalone))
		{
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*RecentlyPlaced[Index].ObjectPath);

			if (AssetData.IsValid())
			{
				TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
				UActorFactory* Factory = FindObject<UActorFactory>(nullptr, *RecentlyPlaced[Index].Factory);

				RecentlyPlacedContainer->AddSlot()
					[
						SNew(SPlacementAssetEntry, Factory, AssetData)
					];
			}
		}
	}
}

void SPlacementModeTools::RefreshVolumes()
{
	bVolumesRefreshRequested = false;

	VolumesContainer->ClearChildren();

	TArray< TSharedRef<SPlacementAssetEntry> > Entries;

	// Make a map of UClasses to ActorFactories that support them
	const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
	TMap<UClass*, UActorFactory*> ActorFactoryMap;
	for ( int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
	{
		UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

		if ( ActorFactory )
		{
			ActorFactoryMap.Add( ActorFactory->GetDefaultActorClass( FAssetData() ), ActorFactory );
		}
	}

	// Add loaded classes
	FText UnusedErrorMessage;
	FAssetData NoAssetData;
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		const UClass* Class = *ClassIt;

		if (!Class->HasAllClassFlags(CLASS_NotPlaceable) &&
			!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
			Class->IsChildOf(AVolume::StaticClass()) &&
			Class->ClassGeneratedBy == nullptr )
		{
			UActorFactory* Factory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), Class);
			Entries.Add(SNew(SPlacementAssetEntry, Factory, FAssetData(Class)));
		}
	}

	struct FCompareFactoryByDisplayName
	{
		FORCEINLINE bool operator()( const TSharedRef<SPlacementAssetEntry>& A, const TSharedRef<SPlacementAssetEntry>& B ) const
		{
			return A->AssetDisplayName.CompareTo( B->AssetDisplayName ) < 0;
		}
	};

	Entries.Sort( FCompareFactoryByDisplayName() );

	for ( int32 i = 0; i < Entries.Num(); i++ )
	{
		VolumesContainer->AddSlot()
		[
			Entries[i]
		];
	}
}

void SPlacementModeTools::RebuildPlaceableClassWidgetCache()
{
	// Make a map of UClasses to ActorFactories that support them
	const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
	TMap<UClass*, UActorFactory*> ActorFactoryMap;
	for ( int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
	{
		UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

		if ( ActorFactory )
		{
			ActorFactoryMap.Add(ActorFactory->GetDefaultActorClass(FAssetData()), ActorFactory);
		}
	}

	TArray< TSharedRef<SPlacementAssetEntry> > Entries;

	// Add loaded classes
	FText UnusedErrorMessage;
	FAssetData NoAssetData;
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		// Don't offer skeleton classes
		bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(*ClassIt);

		if ( !ClassIt->HasAllClassFlags(CLASS_NotPlaceable) &&
			!ClassIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
			ClassIt->IsChildOf(AActor::StaticClass()) &&
			( !ClassIt->IsChildOf(ABrush::StaticClass()) || ClassIt->IsChildOf(AVolume::StaticClass()) ) &&
			!bIsSkeletonClass )
		{
			UActorFactory** ActorFactory = ActorFactoryMap.Find(*ClassIt);

			const bool IsVolume = ClassIt->IsChildOf(AVolume::StaticClass());

			TSharedPtr<SPlacementAssetEntry> Entry;

			if ( IsVolume )
			{
				UActorFactory* Factory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), *ClassIt);
				Entry = SNew(SPlacementAssetEntry, Factory, FAssetData(*ClassIt))
					.HighlightText(this, &SPlacementModeTools::GetHighlightText);
			}
			else
			{
				if ( !ActorFactory || ( *ActorFactory )->CanCreateActorFrom(NoAssetData, UnusedErrorMessage) )
				{
					if ( !ActorFactory )
					{
						Entry = SNew(SPlacementAssetEntry, nullptr, FAssetData(*ClassIt))
							.HighlightText(this, &SPlacementModeTools::GetHighlightText);
					}
					else
					{
						Entry = SNew(SPlacementAssetEntry, *ActorFactory, FAssetData(*ClassIt))
							.HighlightText(this, &SPlacementModeTools::GetHighlightText);
					}
				}
			}

			if ( Entry.IsValid() )
			{
				Entries.Add(Entry.ToSharedRef());
			}
		}
	}

	struct FCompareFactoryByDisplayName
	{
		FORCEINLINE bool operator()(const TSharedRef<SPlacementAssetEntry>& A, const TSharedRef<SPlacementAssetEntry>& B) const
		{
			return A->AssetDisplayName.CompareTo(B->AssetDisplayName) < 0;
		}
	};

	Entries.Sort(FCompareFactoryByDisplayName());

	// Cache the result
	PlaceableClassWidgets.Reset();
	for ( int32 i = 0; i < Entries.Num(); i++ )
	{
		PlaceableClassWidgets.Add(Entries[i]);
	}
}

void SPlacementModeTools::RefreshPlaceables()
{
	if ( bPlaceablesFullRefreshRequested )
	{
		RebuildPlaceableClassWidgetCache();
	}

	PlaceablesContainer->ClearChildren();
	SearchResultsContainer->ClearChildren();

	if ( SearchText.IsEmpty() )
	{
		// Just build the full list with no filtering
		for ( int32 i = 0; i < PlaceableClassWidgets.Num(); i++ )
		{
			PlaceablesContainer->AddSlot()
			[
				PlaceableClassWidgets[i]
			];
		}
	}
	else
	{
		// Filter out the widgets that don't belong
		for ( int32 i = 0; i < PlaceableClassWidgets.Num(); i++ )
		{
			const FString* SourceString = FTextInspector::GetSourceString(PlaceableClassWidgets[i]->AssetDisplayName);
			if ( PlaceableClassWidgets[i]->AssetDisplayName.ToString().Contains( SearchText.ToString() ) || ( SourceString && SourceString->Contains( SearchText.ToString() ) ) )
			{
				SearchResultsContainer->AddSlot()
				[
					PlaceableClassWidgets[i]
				];
			}
		}

		if ( SearchResultsContainer->GetChildren()->Num() == 0 )
		{
			SearchResultsContainer->AddSlot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PlacementMode", "NoResultsFound", "No Results Found"))
				];
		}
	}
}

void SPlacementModeTools::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

	if ( bPlaceablesRefreshRequested || bPlaceablesFullRefreshRequested )
	{
		RefreshPlaceables();
		bPlaceablesRefreshRequested = false;
		bPlaceablesFullRefreshRequested = false;
	}

	if ( bVolumesRefreshRequested )
	{
		RefreshVolumes();
	}
}

FReply SPlacementModeTools::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	FReply Reply = FReply::Unhandled();

	if ( InKeyEvent.GetKey() == EKeys::Escape )
	{
		FPlacementMode* PlacementEditMode = (FPlacementMode*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Placement );
		PlacementEditMode->StopPlacing();
		Reply = FReply::Handled();
	}

	return Reply;
}

int32 SPlacementModeTools::GetSelectedPanel() const
{
	return SearchText.IsEmpty() ? 0 : 1;
}

void SPlacementModeTools::OnSearchChanged(const FText& InFilterText)
{
	// If the search text was previously we do a full rebuild of our cached widgets
	// for the placeable widgets.
	if ( SearchText.IsEmpty() )
	{
		bPlaceablesFullRefreshRequested = true;
	}
	else
	{
		bPlaceablesRefreshRequested = true;
	}

	SearchText = InFilterText;
}

FText SPlacementModeTools::GetHighlightText() const
{
	return SearchText;
}

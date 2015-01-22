// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorAsset.h"
#include "SPropertyEditorNewAsset.h"
#include "PropertyNode.h"
#include "PropertyEditor.h"
#include "AssetThumbnail.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "Runtime/AssetRegistry/Public/AssetData.h"
#include "PropertyHandleImpl.h"
#include "DelegateFilter.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "SAssetDropTarget.h"
#include "AssetRegistryModule.h"
#include "Particles/ParticleSystem.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

DECLARE_DELEGATE( FOnCopy );
DECLARE_DELEGATE( FOnPaste );

bool SPropertyEditorAsset::ShouldDisplayThumbnail( const FArguments& InArgs )
{
	// Decide whether we should display the thumbnail or not
	bool bDisplayThumbnailByDefault = false;
	if(PropertyEditor.IsValid())
	{
		UProperty* NodeProperty = PropertyEditor->GetPropertyNode()->GetProperty();
		UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>( NodeProperty );
		check(ObjectProperty);

		bDisplayThumbnailByDefault |= ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()) ||
									  ObjectProperty->PropertyClass->IsChildOf(UTexture::StaticClass()) ||
									  ObjectProperty->PropertyClass->IsChildOf(UStaticMesh::StaticClass()) ||
									  ObjectProperty->PropertyClass->IsChildOf(UStaticMeshComponent::StaticClass()) ||
									  ObjectProperty->PropertyClass->IsChildOf(USkeletalMesh::StaticClass()) ||
									  ObjectProperty->PropertyClass->IsChildOf(USkeletalMeshComponent::StaticClass()) ||
									  ObjectProperty->PropertyClass->IsChildOf(UParticleSystem::StaticClass());
	}

	bool bDisplayThumbnail = ( bDisplayThumbnailByDefault || InArgs._DisplayThumbnail ) && InArgs._ThumbnailPool.IsValid();

	if(PropertyEditor.IsValid())
	{
		// also check metadata for thumbnail & text display
		if(InArgs._ThumbnailPool.IsValid())
		{
			const UProperty* ArrayParent = PropertyEditorHelpers::GetArrayParent( *PropertyEditor->GetPropertyNode() );

			const UProperty* PropertyToCheck = PropertyEditor->GetProperty();
			if( ArrayParent != NULL )
			{
				// If the property is a child of an array property, the parent will have the display thumbnail metadata
				PropertyToCheck = ArrayParent;
			}

			FString DisplayThumbnailString = PropertyToCheck->GetMetaData(TEXT("DisplayThumbnail"));
			if(DisplayThumbnailString.Len() > 0)
			{
				bDisplayThumbnail = DisplayThumbnailString == TEXT("true");
			}
		}
	}

	return bDisplayThumbnail;
}

void SPropertyEditorAsset::Construct( const FArguments& InArgs, const TSharedPtr<FPropertyEditor>& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	PropertyHandle = InArgs._PropertyHandle;
	OnSetObject = InArgs._OnSetObject;

	if(PropertyEditor.IsValid())
	{
		UProperty* NodeProperty = PropertyEditor->GetPropertyNode()->GetProperty();
		UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>( NodeProperty );
		check(ObjectProperty);

		bAllowClear = !(NodeProperty->PropertyFlags & CPF_NoClear);
		ObjectClass = ObjectProperty->PropertyClass;
		bIsActor = ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() );

		FString ClassFilterString = NodeProperty->GetMetaData("AllowedClasses");
		if(ClassFilterString.IsEmpty())
		{
			CustomClassFilters.Add(ObjectClass);
		}
		else
		{
			TArray<FString> CustomClassFilterNames;
			ClassFilterString.ParseIntoArray(&CustomClassFilterNames, TEXT(","), true);

			for(auto It = CustomClassFilterNames.CreateConstIterator(); It; ++It)
			{
				const FString& ClassName = *It;

				UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);
				if(!Class)
				{
					Class = LoadObject<UClass>(nullptr, *ClassName);
				}
				if(Class)
				{
					CustomClassFilters.Add(Class);
				}
			}
		}
	}
	else
	{
		bAllowClear = InArgs._AllowClear;
		ObjectPath = InArgs._ObjectPath;
		ObjectClass = InArgs._Class;
		bIsActor = ObjectClass->IsChildOf( AActor::StaticClass() );

		CustomClassFilters.Add(ObjectClass);
	}
	OnShouldFilterAsset = InArgs._OnShouldFilterAsset;

	if (InArgs._NewAssetFactories.IsSet())
	{
		NewAssetFactories = InArgs._NewAssetFactories.GetValue();
	}
	else if (ObjectClass->GetClass() != UObject::StaticClass())
	{
		TArray<const UClass*> AllowedClasses;
		AllowedClasses.Add(ObjectClass);
		NewAssetFactories = PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses);
	}
	
	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	ChildSlot
	[
		SNew( SAssetDropTarget )
		.OnIsAssetAcceptableForDrop( this, &SPropertyEditorAsset::OnAssetDraggedOver )
		.OnAssetDropped( this, &SPropertyEditorAsset::OnAssetDropped )
		[
			SAssignNew( HorizontalBox, SHorizontalBox )			
		]
	];

	if(ShouldDisplayThumbnail(InArgs))
	{
		FObjectOrAssetData Value; 
		GetValue( Value );

		AssetThumbnail = MakeShareable( new FAssetThumbnail( Value.AssetData, InArgs._ThumbnailSize.X, InArgs._ThumbnailSize.Y, InArgs._ThumbnailPool ) );
		
		if( AssetThumbnail.IsValid()  )
		{
			HorizontalBox->AddSlot()
				.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
				.AutoWidth()
				[
					SAssignNew( ThumbnailBorder, SBorder )
					.Padding( 5.0f )
					.BorderImage( this, &SPropertyEditorAsset::GetThumbnailBorder )
					.OnMouseDoubleClick( this, &SPropertyEditorAsset::OnAssetThumbnailDoubleClick )
					[
						SNew( SBox )
						.ToolTipText( this, &SPropertyEditorAsset::OnGetToolTip )
						.WidthOverride( InArgs._ThumbnailSize.X ) 
						.HeightOverride( InArgs._ThumbnailSize.Y )
						[
							AssetThumbnail->MakeThumbnailWidget()
						]
					]
				];
		}
	}

	TSharedPtr<SHorizontalBox> ButtonBox;
	
	HorizontalBox->AddSlot()
	.FillWidth(1.0f)
	.Padding( 0.0f, 4.0f, 4.0f, 4.0f )
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign( VAlign_Center )
		[
			SAssignNew( ButtonBox, SHorizontalBox )
			.IsEnabled( this, &SPropertyEditorAsset::CanEdit )
			+ SHorizontalBox::Slot()
			[
				SAssignNew( AssetComboButton, SComboButton )
				.ToolTipText( this, &SPropertyEditorAsset::OnGetToolTip )
				.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent( this, &SPropertyEditorAsset::OnGetMenuContent )
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					// Show the name of the asset or actor
					SNew(STextBlock)
					.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
					.Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) )
					.Text(this,&SPropertyEditorAsset::OnGetAssetName)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( InArgs._CustomContentSlot.Widget == SNullWidget::NullWidget ? FMargin( 0.0f, 0.0f ) : FMargin( 0.0f, 2.0f ) )
		[
			InArgs._CustomContentSlot.Widget
		]
	];

	if(!bIsActor && InArgs._DisplayUseSelected)
	{
		ButtonBox->AddSlot()
		.VAlign(VAlign_Center)
		.Padding( 2.0f, 0.0f )
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton( FSimpleDelegate::CreateSP( this, &SPropertyEditorAsset::OnUse ) )
		];
	}

	if(InArgs._DisplayBrowse)
	{
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP( this, &SPropertyEditorAsset::OnBrowse ),
				TAttribute<FText>( this, &SPropertyEditorAsset::GetOnBrowseToolTip )
				)
		];
	}

	if(bIsActor)
	{
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeInteractiveActorPicker( FOnGetAllowedClasses::CreateSP(this, &SPropertyEditorAsset::OnGetAllowedClasses), FOnShouldFilterActor(), FOnActorSelected::CreateSP( this, &SPropertyEditorAsset::OnActorSelected ) )
		];
	}

	if( InArgs._ResetToDefaultSlot.Widget != SNullWidget::NullWidget )
	{
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign(VAlign_Center)
		[
			InArgs._ResetToDefaultSlot.Widget
		];
	}
}

void SPropertyEditorAsset::GetDesiredWidth( float& OutMinDesiredWidth, float &OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 250.0f;
	// No max width
	OutMaxDesiredWidth = 0.0f;
}

const FSlateBrush* SPropertyEditorAsset::GetThumbnailBorder() const
{
	if ( ThumbnailBorder->IsHovered() )
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	}
	else
	{
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
	}
}

void SPropertyEditorAsset::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if( AssetThumbnail.IsValid() )
	{
		// Ensure the thumbnail is up to date
		FObjectOrAssetData Value;
		GetValue( Value );

		// If the thumbnail is not the same as the object value set the thumbnail to the new value
		if( !(AssetThumbnail->GetAssetData() == Value.AssetData) )
		{
			AssetThumbnail->SetAsset( Value.AssetData );
		}
	}
}

bool SPropertyEditorAsset::Supports( const TSharedRef< FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	if(	PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInline) )
	{
		return false;
	}

	return Supports(PropertyNode->GetProperty());
}

bool SPropertyEditorAsset::Supports( const UProperty* NodeProperty )
{
	const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>( NodeProperty );
	if (ObjectProperty != NULL 
		&& !NodeProperty->IsA(UClassProperty::StaticClass()) 
		&& !NodeProperty->IsA(UAssetClassProperty::StaticClass()))
	{
		return true;
	}
	
	return false;
}

TSharedRef<SWidget> SPropertyEditorAsset::OnGetMenuContent()
{
	FObjectOrAssetData Value;
	GetValue(Value);

	if(bIsActor)
	{
		return PropertyCustomizationHelpers::MakeActorPickerWithMenu(Cast<AActor>(Value.Object),
																	 bAllowClear,
																	 FOnShouldFilterActor::CreateSP( this, &SPropertyEditorAsset::IsFilteredActor ),
																	 FOnActorSelected::CreateSP( this, &SPropertyEditorAsset::OnActorSelected),
																	 FSimpleDelegate::CreateSP( this, &SPropertyEditorAsset::CloseComboButton ),
																	 FSimpleDelegate::CreateSP( this, &SPropertyEditorAsset::OnUse ) );
	}
	else
	{
		return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(Value.AssetData,
																	 bAllowClear,
																	 CustomClassFilters,
																	 NewAssetFactories,
																	 OnShouldFilterAsset,
																	 FOnAssetSelected::CreateSP(this, &SPropertyEditorAsset::OnAssetSelected),
																	 FSimpleDelegate::CreateSP(this, &SPropertyEditorAsset::CloseComboButton));
	}
}

bool SPropertyEditorAsset::IsFilteredActor( const AActor* const Actor ) const
{
	return Actor->IsA( ObjectClass );
}

void SPropertyEditorAsset::CloseComboButton()
{
	AssetComboButton->SetIsOpen(false);
}

FString SPropertyEditorAsset::OnGetAssetName() const
{
	FObjectOrAssetData Value; 
	FPropertyAccess::Result Result = GetValue( Value );

	FString Name = LOCTEXT("None", "None").ToString();
	if( Result == FPropertyAccess::Success )
	{
		if(Value.Object != NULL)
		{
			if( bIsActor )
			{
				AActor* Actor = CastChecked<AActor>(Value.Object);
				Name = Actor->GetActorLabel();
			}
			else
			{
				Name = Value.Object->GetName();
			}
		}
		else if( Value.AssetData.IsValid() )
		{
			Name = FString( Value.AssetData.AssetName.ToString() );
		}
	}
	else if( Result == FPropertyAccess::MultipleValues )
	{
		Name = LOCTEXT("MultipleValues", "Multiple Values").ToString();
	}

	return Name;
}

FString SPropertyEditorAsset::OnGetAssetClassName() const
{
	UClass* Class = GetDisplayedClass();
	if(Class)
	{
		return Class->GetName();
	}
	return FString();
}

FString SPropertyEditorAsset::OnGetToolTip() const
{
	FObjectOrAssetData Value; 
	FPropertyAccess::Result Result = GetValue( Value );

	FString ToolTip;

	if( Result == FPropertyAccess::Success )
	{
		if(Value.Object != NULL && !bIsActor )
		{
			// Display the package name which is a valid path to the object without redundant information
			ToolTip = Value.Object->GetOutermost()->GetName();
		}
		else if( Value.AssetData.IsValid() )
		{
			ToolTip = Value.AssetData.PackageName.ToString();
		}
	}
	else if( Result == FPropertyAccess::MultipleValues )
	{
		ToolTip = LOCTEXT("MultipleValues", "Multiple Values").ToString();
	}


	if( ToolTip.IsEmpty() )
	{
		ToolTip = ObjectPath.Get();
	}

	return ToolTip;
}

void SPropertyEditorAsset::SetValue( const FAssetData& AssetData )
{
	AssetComboButton->SetIsOpen(false);
	if(PropertyEditor.IsValid())
	{
		PropertyEditor->GetPropertyHandle()->SetValue(AssetData);
	}

	OnSetObject.ExecuteIfBound(AssetData);
}

FPropertyAccess::Result SPropertyEditorAsset::GetValue( FObjectOrAssetData& OutValue ) const
{
	// Potentially accessing the value while garbage collecting or saving the package could trigger a crash.
	// so we fail to get the value when that is occuring.
	if ( GIsSavingPackage || GIsGarbageCollecting )
	{
		return FPropertyAccess::Fail;
	}

	FPropertyAccess::Result Result = FPropertyAccess::Fail;

	if( PropertyEditor.IsValid() && PropertyEditor->GetPropertyHandle()->IsValidHandle() )
	{
		UObject* Object = NULL;
		Result = PropertyEditor->GetPropertyHandle()->GetValue(Object);

		if (Object == NULL)
		{
			// Check to see if it's pointing to an unloaded object
			FString CurrentObjectPath;
			PropertyEditor->GetPropertyHandle()->GetValueAsFormattedString( CurrentObjectPath );

			if (CurrentObjectPath.Len() > 0 && CurrentObjectPath != TEXT("None"))
			{
				if( CurrentObjectPath != TEXT("None") && ( !CachedAssetData.IsValid() || CachedAssetData.ObjectPath.ToString() != CurrentObjectPath ) )
				{
					static FName AssetRegistryName("AssetRegistry");

					FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
					CachedAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( *CurrentObjectPath );
				}

				Result = FPropertyAccess::Success;
				OutValue = FObjectOrAssetData( CachedAssetData );

				return Result;
			}
		}

		OutValue = FObjectOrAssetData( Object );
	}
	else
	{
		const FString CurrentObjectPath = ObjectPath.Get();
		Result = FPropertyAccess::Success;

		if( CurrentObjectPath != TEXT("None") && ( !CachedAssetData.IsValid() || CachedAssetData.ObjectPath.ToString() != CurrentObjectPath ) )
		{
			static FName AssetRegistryName("AssetRegistry");

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
			CachedAssetData = AssetRegistryModule.Get().GetAssetByObjectPath( *CurrentObjectPath );

			if( PropertyHandle.IsValid() )
			{
				// No property editor was specified so check if multiple property values are associated with the property handle
				TArray<FString> ObjectValues;
				PropertyHandle->GetPerObjectValues( ObjectValues );

				if( ObjectValues.Num() > 1 )
				{
					for( int32 ObjectIndex = 1; ObjectIndex < ObjectValues.Num() && Result == FPropertyAccess::Success; ++ObjectIndex )
					{
						if( ObjectValues[ ObjectIndex ] != ObjectValues[ 0 ] )
						{
							Result = FPropertyAccess::MultipleValues;
						}
					}
				}
			}
		}
		else if( CurrentObjectPath == TEXT("None") )
		{
			CachedAssetData = FAssetData();
		}

		OutValue = FObjectOrAssetData( CachedAssetData );
	}

	return Result;
}

UClass* SPropertyEditorAsset::GetDisplayedClass() const
{
	FObjectOrAssetData Value;
	GetValue( Value );
	if(Value.Object != NULL)
	{
		return Value.Object->GetClass();
	}
	else
	{
		return ObjectClass;	
	}
}

void SPropertyEditorAsset::OnAssetSelected( const class FAssetData& AssetData )
{
	SetValue(AssetData);
}

void SPropertyEditorAsset::OnActorSelected( AActor* InActor )
{
	SetValue(InActor);
}

void SPropertyEditorAsset::OnGetAllowedClasses(TArray<const UClass*>& AllowedClasses)
{
	AllowedClasses.Append(CustomClassFilters);
}

void SPropertyEditorAsset::OnOpenAssetEditor()
{
	FObjectOrAssetData Value;
	GetValue( Value );

	UObject* ObjectToEdit = Value.AssetData.GetAsset();
	if( ObjectToEdit )
	{
		GEditor->EditObject( ObjectToEdit );
	}
}

void SPropertyEditorAsset::OnBrowse()
{
	FObjectOrAssetData Value;
	GetValue( Value );

	if(PropertyEditor.IsValid() && Value.Object)
	{
		// This code only works on loaded objects
		FPropertyEditor::SyncToObjectsInNode(PropertyEditor->GetPropertyNode());		
	}
	else if (auto* Asset = Value.AssetData.GetAsset())
	{
		TArray< UObject* > Objects;
		Objects.Add( Asset );
		GEditor->SyncBrowserToObjects( Objects );
	}
}

FText SPropertyEditorAsset::GetOnBrowseToolTip() const
{
	FObjectOrAssetData Value;
	GetValue( Value );

	if (Value.Object)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(Value.Object->GetName()));
		if (Value.Object->IsA(AActor::StaticClass()))
		{
			return FText::Format(LOCTEXT( "BrowseToAssetInViewport", "Select '{Asset}' in the viewport"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT( "BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);
		}
	}
	
	return LOCTEXT( "BrowseToAssetInContentBrowser", "Browse to Asset in Content Browser");
}

void SPropertyEditorAsset::OnUse()
{
	if(PropertyEditor.IsValid())
	{
		PropertyEditor->GetPropertyHandle()->SetObjectValueFromSelection();
	}
	else
	{
		// Load selected assets
		FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

		// try to get a selected object of our class
		UObject* Selection = NULL;
		if( ObjectClass && ObjectClass->IsChildOf( AActor::StaticClass() ) )
		{
			Selection = GEditor->GetSelectedActors()->GetTop( ObjectClass );
		}
		else if( ObjectClass )
		{
			// Get the first material selected
			Selection = GEditor->GetSelectedObjects()->GetTop( ObjectClass );
		}

		// Check against custom asset filter
		if (Selection != NULL
			&& OnShouldFilterAsset.IsBound()
			&& OnShouldFilterAsset.Execute(FAssetData(Selection)))
		{
			Selection = NULL;
		}

		if( Selection )
		{
			SetValue( Selection );
		}
	}
}

void SPropertyEditorAsset::OnClear()
{
	SetValue(NULL);
}

FSlateColor SPropertyEditorAsset::GetAssetClassColor()
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));	
	TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(GetDisplayedClass());
	if(AssetTypeActions.IsValid())
	{
		return FSlateColor(AssetTypeActions.Pin()->GetTypeColor());
	}

	return FSlateColor::UseForeground();
}

bool SPropertyEditorAsset::OnAssetDraggedOver( const UObject* InObject ) const
{
	if (CanEdit() && InObject != NULL && InObject->IsA(ObjectClass))
	{
		// Check against custom asset filter
		if (!OnShouldFilterAsset.IsBound()
			|| !OnShouldFilterAsset.Execute(FAssetData(InObject)))
		{
			return true;
		}
	}

	return false;
}

void SPropertyEditorAsset::OnAssetDropped( UObject* InObject )
{
	if( CanEdit() )
	{
		SetValue(InObject);
	}
}


void SPropertyEditorAsset::OnCopy()
{
	FObjectOrAssetData Value;
	GetValue( Value );

	if( Value.AssetData.IsValid() )
	{
		FPlatformMisc::ClipboardCopy(*Value.AssetData.GetExportTextName());
	}
}

void SPropertyEditorAsset::OnPaste()
{
	FString DestPath;
	FPlatformMisc::ClipboardPaste(DestPath);

	if(DestPath == TEXT("None"))
	{
		SetValue(NULL);
	}
	else
	{
		UObject* Object = LoadObject<UObject>(NULL, *DestPath);
		if(Object && Object->IsA(ObjectClass))
		{
			// Check against custom asset filter
			if (!OnShouldFilterAsset.IsBound()
				|| !OnShouldFilterAsset.Execute(FAssetData(Object)))
			{
				SetValue(Object);
			}
		}
	}
}

bool SPropertyEditorAsset::CanPaste()
{
	FString ClipboardText;
	FPlatformMisc::ClipboardPaste(ClipboardText);

	const FString PossibleObjectPath = FPackageName::ExportTextPathToObjectPath(ClipboardText);

	bool bCanPaste = false;

	if( CanEdit() )
	{
		if( PossibleObjectPath == TEXT("None") )
		{
			bCanPaste = true;
		}
		else
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			bCanPaste = PossibleObjectPath.Len() < NAME_SIZE && AssetRegistryModule.Get().GetAssetByObjectPath( *PossibleObjectPath ).IsValid();
		}
	}

	return bCanPaste;
}

FReply SPropertyEditorAsset::OnAssetThumbnailDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	OnOpenAssetEditor();
	return FReply::Handled();
}

bool SPropertyEditorAsset::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}

#undef LOCTEXT_NAMESPACE

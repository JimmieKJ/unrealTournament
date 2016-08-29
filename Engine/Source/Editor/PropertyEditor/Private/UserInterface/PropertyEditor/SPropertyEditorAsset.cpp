// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorAsset.h"
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
	OnShouldFilterAsset = InArgs._OnShouldFilterAsset;

	UProperty* Property = nullptr;
	if(PropertyEditor.IsValid())
	{
		Property = PropertyEditor->GetPropertyNode()->GetProperty();
		UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property);
		check(ObjectProperty);

		bAllowClear = !(Property->PropertyFlags & CPF_NoClear);
		ObjectClass = ObjectProperty->PropertyClass;
		bIsActor = ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() );
	}
	else
	{
		bAllowClear = InArgs._AllowClear;
		ObjectPath = InArgs._ObjectPath;
		ObjectClass = InArgs._Class;
		bIsActor = ObjectClass->IsChildOf( AActor::StaticClass() );

		if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
		{
			Property = PropertyHandle->GetProperty();
		}
		else
		{
			CustomClassFilters.Add(ObjectClass);
		}
	}

	// Account for the allowed classes specified in the property metadata
	if (Property)
	{
		FString ClassFilterString;
		if (UArrayProperty* ArrayParent = Cast<UArrayProperty>(Property->GetOuter()))
		{
			ClassFilterString = ArrayParent->GetMetaData("AllowedClasses");
		}
		else
		{
			ClassFilterString = Property->GetMetaData("AllowedClasses");
		}

		if (ClassFilterString.IsEmpty())
		{
			CustomClassFilters.Add(ObjectClass);
		}
		else
		{
			TArray<FString> CustomClassFilterNames;
			ClassFilterString.ParseIntoArray(CustomClassFilterNames, TEXT(","), true);

			for (auto It = CustomClassFilterNames.CreateIterator(); It; ++It)
			{
				FString& ClassName = *It;
				// User can potentially list class names with leading or trailing whitespace
				ClassName.Trim();
				ClassName.TrimTrailing();

				UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);

				if (!Class)
				{
					Class = LoadObject<UClass>(nullptr, *ClassName);
				}

				if (Class)
				{
					// If the class is an interface, expand it to be all classes in memory that implement the class.
					if (Class->HasAnyClassFlags(CLASS_Interface))
					{
						for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
						{
							UClass* const ClassWithInterface = (*ClassIt);
							if (ClassWithInterface->ImplementsInterface(Class))
							{
								CustomClassFilters.Add(ClassWithInterface);
							}
						}
					}
					else
					{
						CustomClassFilters.Add(Class);
					}
				}
			}
		}
	}

	if (InArgs._NewAssetFactories.IsSet())
	{
		NewAssetFactories = InArgs._NewAssetFactories.GetValue();
	}
	else if (CustomClassFilters.Num() > 1 || !CustomClassFilters.Contains(UObject::StaticClass()))
	{
		NewAssetFactories = PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(CustomClassFilters);
	}
	
	TSharedPtr<SHorizontalBox> ValueContentBox = NULL;
	ChildSlot
	[
		SNew( SAssetDropTarget )
		.OnIsAssetAcceptableForDrop( this, &SPropertyEditorAsset::OnAssetDraggedOver )
		.OnAssetDropped( this, &SPropertyEditorAsset::OnAssetDropped )
		[
			SAssignNew( ValueContentBox, SHorizontalBox )	
		]
	];

	TAttribute<bool> IsEnabledAttribute(this, &SPropertyEditorAsset::CanEdit);
	TAttribute<FText> TooltipAttribute(this, &SPropertyEditorAsset::OnGetToolTip);

	if (Property && Property->HasAllPropertyFlags(CPF_DisableEditOnTemplate))
	{
		// There are some cases where editing an Actor Property is not allowed, such as when it is contained within a struct or a CDO
		TArray<UObject*> ObjectList;
		PropertyEditor->GetPropertyHandle()->GetOuterObjects(ObjectList);

		// If there is no objects, that means we must have a struct asset managing this property
		if (ObjectList.Num() == 0)
		{
			IsEnabledAttribute.Set(false);
			TooltipAttribute.Set(LOCTEXT("VariableHasDisableEditOnTemplate", "Editing this value in structure's defaults is not allowed"));
		}
		else
		{
			// Go through all the found objects and see if any are a CDO, we can't set an actor in a CDO default.
			for (UObject* Obj : ObjectList)
			{
				if (Obj->HasAllFlags(RF_ClassDefaultObject))
				{
					IsEnabledAttribute.Set(false);
					TooltipAttribute.Set(LOCTEXT("VariableHasDisableEditOnTemplateTooltip", "Editing this value in a Class Default Object is not allowed"));
					break;
				}

			}
		}
	}

	AssetComboButton = SNew(SComboButton)
		.ToolTipText(TooltipAttribute)
		.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
		.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
		.OnGetMenuContent( this, &SPropertyEditorAsset::OnGetMenuContent )
		.OnMenuOpenChanged( this, &SPropertyEditorAsset::OnMenuOpenChanged )
		.IsEnabled( IsEnabledAttribute )
		.ContentPadding(2.0f)
		.ButtonContent()
		[
			// Show the name of the asset or actor
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
			.Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) )
			.Text(this,&SPropertyEditorAsset::OnGetAssetName)
		];

	
	TSharedRef<SHorizontalBox> ButtonBox = SNew( SHorizontalBox );
	
	TSharedPtr<SVerticalBox> CustomContentBox;

	if( ShouldDisplayThumbnail(InArgs) )
	{
		FObjectOrAssetData Value; 
		GetValue( Value );

		AssetThumbnail = MakeShareable( new FAssetThumbnail( Value.AssetData, InArgs._ThumbnailSize.X, InArgs._ThumbnailSize.Y, InArgs._ThumbnailPool ) );

		ValueContentBox->AddSlot()
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		.AutoWidth()
		[
			SAssignNew( ThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage( this, &SPropertyEditorAsset::GetThumbnailBorder )
			.OnMouseDoubleClick( this, &SPropertyEditorAsset::OnAssetThumbnailDoubleClick )
			[
				SNew( SBox )
				.ToolTipText(TooltipAttribute)
				.WidthOverride( InArgs._ThumbnailSize.X ) 
				.HeightOverride( InArgs._ThumbnailSize.Y )
				[
					AssetThumbnail->MakeThumbnailWidget()
				]
			]
		];


		ValueContentBox->AddSlot()
		[
			SNew( SBox )
			.VAlign( VAlign_Center )
			[
				SAssignNew( CustomContentBox, SVerticalBox )
				+ SVerticalBox::Slot()
				[
					AssetComboButton.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 0.0f, 2.0f, 4.0f, 2.0f )
				[
					ButtonBox
				]
			]
		];
	}
	else
	{
		ValueContentBox->AddSlot()
		[
			SAssignNew( CustomContentBox, SVerticalBox )
			+SVerticalBox::Slot()
			.VAlign( VAlign_Center )
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				[
					AssetComboButton.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 4.f, 0.f )
				[
					ButtonBox
				]
			]
		];
	}

	if( InArgs._CustomContentSlot.Widget != SNullWidget::NullWidget )
	{
		CustomContentBox->AddSlot()
		.VAlign( VAlign_Center )
		.Padding( FMargin( 0.0f, 2.0f ) )
		[
			InArgs._CustomContentSlot.Widget
		];
	}

	if( !bIsActor && InArgs._DisplayUseSelected )
	{
		ButtonBox->AddSlot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton( FSimpleDelegate::CreateSP( this, &SPropertyEditorAsset::OnUse ), FText(), IsEnabledAttribute )
		];
	}

	if( InArgs._DisplayBrowse )
	{
		ButtonBox->AddSlot()
		.Padding( 2.0f, 0.0f )
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP( this, &SPropertyEditorAsset::OnBrowse ),
				TAttribute<FText>( this, &SPropertyEditorAsset::GetOnBrowseToolTip )
				)
		];
	}

	if( bIsActor )
	{
		TSharedRef<SWidget> ActorPicker = PropertyCustomizationHelpers::MakeInteractiveActorPicker( FOnGetAllowedClasses::CreateSP(this, &SPropertyEditorAsset::OnGetAllowedClasses), FOnShouldFilterActor(), FOnActorSelected::CreateSP( this, &SPropertyEditorAsset::OnActorSelected ) );
		ActorPicker->SetEnabled( IsEnabledAttribute );

		ButtonBox->AddSlot()
		.Padding( 2.0f, 0.0f )
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			ActorPicker
		];
	}

	if( InArgs._ResetToDefaultSlot.Widget != SNullWidget::NullWidget )
	{
		TSharedRef<SWidget> ResetToDefaultWidget  = InArgs._ResetToDefaultSlot.Widget;
		ResetToDefaultWidget->SetEnabled( IsEnabledAttribute );

		ButtonBox->AddSlot()
		.Padding( 4.0f, 0.0f )
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			ResetToDefaultWidget
		];
	}
}

void SPropertyEditorAsset::GetDesiredWidth( float& OutMinDesiredWidth, float &OutMaxDesiredWidth )
{
	OutMinDesiredWidth = 250.f;
	// No max width
	OutMaxDesiredWidth = 350.f;
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

void SPropertyEditorAsset::OnMenuOpenChanged(bool bOpen)
{
	if ( bOpen == false )
	{
		AssetComboButton->SetMenuContent(SNullWidget::NullWidget);
	}
}

bool SPropertyEditorAsset::IsFilteredActor( const AActor* const Actor ) const
{
	return Actor->IsA( ObjectClass ) && !Actor->IsChildActor();
}

void SPropertyEditorAsset::CloseComboButton()
{
	AssetComboButton->SetIsOpen(false);
}

FText SPropertyEditorAsset::OnGetAssetName() const
{
	FObjectOrAssetData Value; 
	FPropertyAccess::Result Result = GetValue( Value );

	FText Name = LOCTEXT("None", "None");
	if( Result == FPropertyAccess::Success )
	{
		if(Value.Object != NULL)
		{
			if( bIsActor )
			{
				AActor* Actor = CastChecked<AActor>(Value.Object);
				Name = FText::FromString(Actor->GetActorLabel());
			}
			else
			{
				Name = FText::FromString(Value.Object->GetName());
			}
		}
		else if( Value.AssetData.IsValid() )
		{
			Name = FText::FromName(Value.AssetData.AssetName);
		}
	}
	else if( Result == FPropertyAccess::MultipleValues )
	{
		Name = LOCTEXT("MultipleValues", "Multiple Values");
	}

	return Name;
}

FText SPropertyEditorAsset::OnGetAssetClassName() const
{
	UClass* Class = GetDisplayedClass();
	if(Class)
	{
		return FText::FromString(Class->GetName());
	}
	return FText::GetEmpty();
}

FText SPropertyEditorAsset::OnGetToolTip() const
{
	FObjectOrAssetData Value; 
	FPropertyAccess::Result Result = GetValue( Value );

	FText ToolTipText = FText::GetEmpty();

	if( Result == FPropertyAccess::Success )
	{
		if(Value.Object != NULL && !bIsActor )
		{
			// Display the package name which is a valid path to the object without redundant information
			ToolTipText = FText::FromString(Value.Object->GetOutermost()->GetName());
		}
		else if( Value.AssetData.IsValid() )
		{
			ToolTipText = FText::FromName(Value.AssetData.PackageName);
		}
	}
	else if( Result == FPropertyAccess::MultipleValues )
	{
		ToolTipText = LOCTEXT("MultipleValues", "Multiple Values");
	}

	if( ToolTipText.IsEmpty() )
	{
		ToolTipText = FText::FromString(ObjectPath.Get());
	}

	return ToolTipText;
}

void SPropertyEditorAsset::SetValue( const FAssetData& AssetData )
{
	AssetComboButton->SetIsOpen(false);

	bool bAllowedToSetBasedOnFilter = CanSetBasedOnCustomClasses( AssetData );

	if( bAllowedToSetBasedOnFilter )
	{
		if(PropertyEditor.IsValid())
		{
			PropertyEditor->GetPropertyHandle()->SetValue(AssetData);
		}

		OnSetObject.ExecuteIfBound(AssetData);
	}
}

FPropertyAccess::Result SPropertyEditorAsset::GetValue( FObjectOrAssetData& OutValue ) const
{
	// Potentially accessing the value while garbage collecting or saving the package could trigger a crash.
	// so we fail to get the value when that is occurring.
	if ( GIsSavingPackage || IsGarbageCollecting() )
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
				if( !CachedAssetData.IsValid() || CachedAssetData.ObjectPath.ToString() != CurrentObjectPath )
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

#if !UE_BUILD_SHIPPING
		if (Object && !Object->IsValidLowLevel())
		{
			const UProperty* Property = PropertyEditor->GetProperty();
			UE_LOG(LogPropertyNode, Fatal, TEXT("Property \"%s\" (%s) contains invalid data."), *Property->GetName(), *Property->GetCPPType());
		}
#endif

		OutValue = FObjectOrAssetData( Object );
	}
	else
	{
		UObject* Object = NULL;
		if (PropertyHandle.IsValid())
		{
			Result = PropertyHandle->GetValue(Object);
		}

		if (Object != NULL)
		{
#if !UE_BUILD_SHIPPING
			if (!Object->IsValidLowLevel())
			{
				const UProperty* Property = PropertyEditor->GetProperty();
				UE_LOG(LogPropertyNode, Fatal, TEXT("Property \"%s\" (%s) contains invalid data."), *Property->GetName(), *Property->GetCPPType());
			}
#endif

			OutValue = FObjectOrAssetData(Object);
		}
		else
		{
			const FString CurrentObjectPath = ObjectPath.Get();
			Result = FPropertyAccess::Success;

			if (CurrentObjectPath != TEXT("None") && (!CachedAssetData.IsValid() || CachedAssetData.ObjectPath.ToString() != CurrentObjectPath))
			{
				static FName AssetRegistryName("AssetRegistry");

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(AssetRegistryName);
				CachedAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*CurrentObjectPath);

				if (PropertyHandle.IsValid())
				{
					// No property editor was specified so check if multiple property values are associated with the property handle
					TArray<FString> ObjectValues;
					PropertyHandle->GetPerObjectValues(ObjectValues);

					if (ObjectValues.Num() > 1)
					{
						for (int32 ObjectIndex = 1; ObjectIndex < ObjectValues.Num() && Result == FPropertyAccess::Success; ++ObjectIndex)
						{
							if (ObjectValues[ObjectIndex] != ObjectValues[0])
							{
								Result = FPropertyAccess::MultipleValues;
							}
						}
					}
				}
			}
			else if (CurrentObjectPath == TEXT("None"))
			{
				CachedAssetData = FAssetData();
			}

			OutValue = FObjectOrAssetData(CachedAssetData);
		}
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
	else
	{
		TArray<FAssetData> AssetDataList;
		AssetDataList.Add( Value.AssetData );
		GEditor->SyncBrowserToObjects( AssetDataList );
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
	// Use the property editor path if it is valid and there is no custom filtering required
	if(PropertyEditor.IsValid() && !OnShouldFilterAsset.IsBound() && CustomClassFilters.Num() == 0)
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
			return CanSetBasedOnCustomClasses( FAssetData(InObject) );
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

bool SPropertyEditorAsset::CanSetBasedOnCustomClasses( const FAssetData& InAssetData ) const
{
	bool bAllowedToSetBasedOnFilter = true;
	if( InAssetData.IsValid() && CustomClassFilters.Num() > 0 )
	{
		bAllowedToSetBasedOnFilter = false;
		UClass* AssetClass = InAssetData.GetClass();
		for( const UClass* AllowedClass : CustomClassFilters )
		{
			if( AssetClass->IsChildOf( AllowedClass ) )
			{
				bAllowedToSetBasedOnFilter = true;
				break;
			}
		}
	}

	return bAllowedToSetBasedOnFilter;
}

#undef LOCTEXT_NAMESPACE

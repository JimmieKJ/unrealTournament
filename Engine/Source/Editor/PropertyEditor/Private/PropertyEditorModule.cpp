// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorPrivatePCH.h"
#include "AssetSelection.h"
#include "AssetToolsModule.h"
#include "SPropertyTreeViewImpl.h"
#include "SlateBasics.h"
#include "PropertyNode.h"
#include "ItemPropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ObjectPropertyNode.h"
#include "PropertyNode.h"
#include "SDetailsView.h"
#include "SSingleProperty.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "PropertyChangeListener.h"
#include "PropertyEditorToolkit.h"

#include "PropertyTable.h"
#include "SPropertyTable.h"
#include "TextPropertyTableCellPresenter.h"

#include "PropertyTableConstants.h"
#include "SStructureDetailsView.h"
#include "SColorPicker.h"
#include "EngineUtils.h"
#include "Engine/UserDefinedStruct.h"


IMPLEMENT_MODULE( FPropertyEditorModule, PropertyEditor );

TSharedRef<IPropertyTypeCustomization> FPropertyTypeLayoutCallback::GetCustomizationInstance() const
{
	return PropertyTypeLayoutDelegate.Execute();
}

void FPropertyTypeLayoutCallbackList::Add( const FPropertyTypeLayoutCallback& NewCallback )
{
	if( !NewCallback.PropertyTypeIdentifier.IsValid() )
	{
		BaseCallback = NewCallback;
	}
	else
	{
		IdentifierList.Add( NewCallback );
	}
}

void FPropertyTypeLayoutCallbackList::Remove( const TSharedPtr<IPropertyTypeIdentifier>& InIdentifier )
{
	if( !InIdentifier.IsValid() )
	{
		BaseCallback = FPropertyTypeLayoutCallback();
	}
	else
	{
		IdentifierList.RemoveAllSwap( [&InIdentifier]( FPropertyTypeLayoutCallback& Callback) { return Callback.PropertyTypeIdentifier == InIdentifier; } );
	}
}

const FPropertyTypeLayoutCallback& FPropertyTypeLayoutCallbackList::Find( const IPropertyHandle& PropertyHandle )
{
	if( IdentifierList.Num() > 0 )
	{
		FPropertyTypeLayoutCallback* Callback =
			IdentifierList.FindByPredicate
			(
				[&]( const FPropertyTypeLayoutCallback& InCallback )
				{
					return InCallback.PropertyTypeIdentifier->IsPropertyTypeCustomized( PropertyHandle );
				}
			);

		if( Callback )
		{
			return *Callback;
		}
	}

	return BaseCallback;
}

void FPropertyEditorModule::StartupModule()
{
}

void FPropertyEditorModule::ShutdownModule()
{
	// NOTE: It's vital that we clean up everything created by this DLL here!  We need to make sure there
	//       are no outstanding references to objects as the compiled code for this module's class will
	//       literally be unloaded from memory after this function exits.  This even includes instantiated
	//       templates, such as delegate wrapper objects that are allocated by the module!
	DestroyColorPicker();

	AllDetailViews.Empty();
	AllSinglePropertyViews.Empty();
}

void FPropertyEditorModule::NotifyCustomizationModuleChanged()
{
	if (FSlateApplication::IsInitialized())
	{
		// The module was changed (loaded or unloaded), force a refresh.  Note it is assumed the module unregisters all customization delegates before this
		for (int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex)
		{
			if (AllDetailViews[ViewIndex].IsValid())
			{
				TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ViewIndex].Pin();

				DetailViewPin->ForceRefresh();
			}
		}
	}
}

static bool ShouldShowProperty(const FPropertyAndParent& PropertyAndParent, bool bHaveTemplate)
{
	const UProperty& Property = PropertyAndParent.Property;

	if ( bHaveTemplate )
	{
		const UClass* PropertyOwnerClass = Cast<const UClass>(Property.GetOuter());
		const bool bDisableEditOnTemplate = PropertyOwnerClass 
			&& PropertyOwnerClass->IsNative()
			&& Property.HasAnyPropertyFlags(CPF_DisableEditOnTemplate);
		if(bDisableEditOnTemplate)
		{
			return false;
		}
	}
	return true;
}

TSharedRef<SWindow> FPropertyEditorModule::CreateFloatingDetailsView( const TArray< UObject* >& InObjects, bool bIsLockable )
{

	TSharedRef<SWindow> NewSlateWindow = SNew(SWindow)
		.Title( NSLOCTEXT("PropertyEditor", "WindowTitle", "Property Editor") )
		.ClientSize( FVector2D(400, 550) );

	// If the main frame exists parent the window to it
	TSharedPtr< SWindow > ParentWindow;
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::GetModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	if( ParentWindow.IsValid() )
	{
		// Parent the window to the main frame 
		FSlateApplication::Get().AddWindowAsNativeChild( NewSlateWindow, ParentWindow.ToSharedRef() );
	}
	else
	{
		FSlateApplication::Get().AddWindow( NewSlateWindow );
	}
	
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bLockable = bIsLockable;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> DetailView = PropertyEditorModule.CreateDetailView( Args );

	bool bHaveTemplate = false;
	for(int32 i=0; i<InObjects.Num(); i++)
	{
		if(InObjects[i] != NULL && InObjects[i]->IsTemplate())
		{
			bHaveTemplate = true;
			break;
		}
	}

	DetailView->SetIsPropertyVisibleDelegate( FIsPropertyVisible::CreateStatic(&ShouldShowProperty, bHaveTemplate) );

	DetailView->SetObjects( InObjects );

	NewSlateWindow->SetContent(
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush(TEXT("PropertyWindow.WindowBorder")) )
		[
			DetailView
		]
	);

	return NewSlateWindow;
}

TSharedRef<SPropertyTreeViewImpl> FPropertyEditorModule::CreatePropertyView( 
	UObject* InObject,
	bool bAllowFavorites, 
	bool bIsLockable, 
	bool bHiddenPropertyVisibility, 
	bool bAllowSearch, 
	bool ShowTopLevelNodes,
	FNotifyHook* InNotifyHook, 
	float InNameColumnWidth,
	FOnPropertySelectionChanged OnPropertySelectionChanged,
	FOnPropertyClicked OnPropertyMiddleClicked,
	FConstructExternalColumnHeaders ConstructExternalColumnHeaders,
	FConstructExternalColumnCell ConstructExternalColumnCell)
{
	TSharedRef<SPropertyTreeViewImpl> PropertyView = 
		SNew( SPropertyTreeViewImpl )
		.IsLockable( bIsLockable )
		.AllowFavorites( bAllowFavorites )
		.HiddenPropertyVis( bHiddenPropertyVisibility )
		.NotifyHook( InNotifyHook )
		.AllowSearch( bAllowSearch )
		.ShowTopLevelNodes( ShowTopLevelNodes )
		.NameColumnWidth( InNameColumnWidth )
		.OnPropertySelectionChanged( OnPropertySelectionChanged )
		.OnPropertyMiddleClicked( OnPropertyMiddleClicked )
		.ConstructExternalColumnHeaders( ConstructExternalColumnHeaders )
		.ConstructExternalColumnCell( ConstructExternalColumnCell );

	if( InObject )
	{
		TArray< TWeakObjectPtr< UObject > > Objects;
		Objects.Add( InObject );
		PropertyView->SetObjectArray( Objects );
	}

	return PropertyView;
}

TSharedRef<IDetailsView> FPropertyEditorModule::CreateDetailView( const FDetailsViewArgs& DetailsViewArgs )
{
	// Compact the list of detail view instances
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if ( !AllDetailViews[ViewIndex].IsValid() )
		{
			AllDetailViews.RemoveAtSwap( ViewIndex );
			--ViewIndex;
		}
	}

	TSharedRef<SDetailsView> DetailView = 
		SNew( SDetailsView )
		.DetailsViewArgs( DetailsViewArgs );

	AllDetailViews.Add( DetailView );

	PropertyEditorOpened.Broadcast();
	return DetailView;
}

TSharedPtr<IDetailsView> FPropertyEditorModule::FindDetailView( const FName ViewIdentifier ) const
{
	if(ViewIdentifier.IsNone())
	{
		return nullptr;
	}

	for(auto It = AllDetailViews.CreateConstIterator(); It; ++It)
	{
		TSharedPtr<SDetailsView> DetailsView = It->Pin();
		if(DetailsView.IsValid() && DetailsView->GetIdentifier() == ViewIdentifier)
		{
			return DetailsView;
		}
	}

	return nullptr;
}

TSharedPtr<ISinglePropertyView> FPropertyEditorModule::CreateSingleProperty( UObject* InObject, FName InPropertyName, const FSinglePropertyParams& InitParams )
{
	// Compact the list of detail view instances
	for( int32 ViewIndex = 0; ViewIndex < AllSinglePropertyViews.Num(); ++ViewIndex )
	{
		if ( !AllSinglePropertyViews[ViewIndex].IsValid() )
		{
			AllSinglePropertyViews.RemoveAtSwap( ViewIndex );
			--ViewIndex;
		}
	}

	TSharedRef<SSingleProperty> Property = 
		SNew( SSingleProperty )
		.Object( InObject )
		.PropertyName( InPropertyName )
		.NamePlacement( InitParams.NamePlacement )
		.NameOverride( InitParams.NameOverride )
		.NotifyHook( InitParams.NotifyHook )
		.PropertyFont( InitParams.Font );

	if( Property->HasValidProperty() )
	{
		AllSinglePropertyViews.Add( Property );

		return Property;
	}

	return NULL;
}

TSharedRef< IPropertyTable > FPropertyEditorModule::CreatePropertyTable()
{
	return MakeShareable( new FPropertyTable() );
}

TSharedRef< SWidget > FPropertyEditorModule::CreatePropertyTableWidget( const TSharedRef< class IPropertyTable >& PropertyTable )
{
	return SNew( SPropertyTable, PropertyTable );
}

TSharedRef< SWidget > FPropertyEditorModule::CreatePropertyTableWidget( const TSharedRef< IPropertyTable >& PropertyTable, const TArray< TSharedRef< class IPropertyTableCustomColumn > >& Customizations )
{
	return SNew( SPropertyTable, PropertyTable )
		.ColumnCustomizations( Customizations );
}

TSharedRef< IPropertyTableWidgetHandle > FPropertyEditorModule::CreatePropertyTableWidgetHandle( const TSharedRef< IPropertyTable >& PropertyTable )
{
	TSharedRef< FPropertyTableWidgetHandle > FWidgetHandle = MakeShareable( new FPropertyTableWidgetHandle( SNew( SPropertyTable, PropertyTable ) ) );

	TSharedRef< IPropertyTableWidgetHandle > IWidgetHandle = StaticCastSharedRef<IPropertyTableWidgetHandle>(FWidgetHandle);

	 return IWidgetHandle;
}

TSharedRef< IPropertyTableWidgetHandle > FPropertyEditorModule::CreatePropertyTableWidgetHandle( const TSharedRef< IPropertyTable >& PropertyTable, const TArray< TSharedRef< class IPropertyTableCustomColumn > >& Customizations )
{
	TSharedRef< FPropertyTableWidgetHandle > FWidgetHandle = MakeShareable( new FPropertyTableWidgetHandle( SNew( SPropertyTable, PropertyTable )
		.ColumnCustomizations( Customizations )));

	TSharedRef< IPropertyTableWidgetHandle > IWidgetHandle = StaticCastSharedRef<IPropertyTableWidgetHandle>(FWidgetHandle);

	 return IWidgetHandle;
}

TSharedRef< IPropertyTableCellPresenter > FPropertyEditorModule::CreateTextPropertyCellPresenter(const TSharedRef< class FPropertyNode >& InPropertyNode, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities, 
																								 const FSlateFontInfo* InFontPtr /* = NULL */)
{
	FSlateFontInfo InFont;

	if (InFontPtr == NULL)
	{
		// Encapsulating reference to Private file PropertyTableConstants.h
		InFont = FEditorStyle::GetFontStyle( PropertyTableConstants::NormalFontStyle );
	}
	else
	{
		InFont = *InFontPtr;
	}

	TSharedRef< FPropertyEditor > PropertyEditor = FPropertyEditor::Create( InPropertyNode, InPropertyUtilities );
	return MakeShareable( new FTextPropertyTableCellPresenter( PropertyEditor, InPropertyUtilities, InFont) );
}

TSharedRef< FAssetEditorToolkit > FPropertyEditorModule::CreatePropertyEditorToolkit( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	return FPropertyEditorToolkit::CreateEditor( Mode, InitToolkitHost, ObjectToEdit );
}


TSharedRef< FAssetEditorToolkit > FPropertyEditorModule::CreatePropertyEditorToolkit( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< UObject* >& ObjectsToEdit )
{
	return FPropertyEditorToolkit::CreateEditor( Mode, InitToolkitHost, ObjectsToEdit );
}

TSharedRef< FAssetEditorToolkit > FPropertyEditorModule::CreatePropertyEditorToolkit( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< TWeakObjectPtr< UObject > >& ObjectsToEdit )
{
	TArray< UObject* > RawObjectsToEdit;
	for( auto ObjectIter = ObjectsToEdit.CreateConstIterator(); ObjectIter; ++ObjectIter )
	{
		RawObjectsToEdit.Add( ObjectIter->Get() );
	}

	return FPropertyEditorToolkit::CreateEditor( Mode, InitToolkitHost, RawObjectsToEdit );
}

TSharedRef<IPropertyChangeListener> FPropertyEditorModule::CreatePropertyChangeListener()
{
	return MakeShareable( new FPropertyChangeListener );
}

void FPropertyEditorModule::RegisterCustomClassLayout( FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate )
{
	if (ClassName != NAME_None)
	{
		FDetailLayoutCallback Callback;
		Callback.DetailLayoutDelegate = DetailLayoutDelegate;
		// @todo: DetailsView: Fix me: this specifies the order in which detail layouts should be queried
		Callback.Order = ClassNameToDetailLayoutNameMap.Num();

		ClassNameToDetailLayoutNameMap.Add(ClassName, Callback);
	}
}


void FPropertyEditorModule::UnregisterCustomClassLayout( FName ClassName )
{
	if (ClassName.IsValid() && (ClassName != NAME_None))
	{
		ClassNameToDetailLayoutNameMap.Remove(ClassName);
	}
}


void FPropertyEditorModule::RegisterCustomPropertyTypeLayout( FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier, TSharedPtr<IDetailsView> ForSpecificInstance )
{
	if( PropertyTypeName != NAME_None )
	{
		FPropertyTypeLayoutCallback Callback;
		Callback.PropertyTypeLayoutDelegate = PropertyTypeLayoutDelegate;
		Callback.PropertyTypeIdentifier = Identifier;

		if( ForSpecificInstance.IsValid() )
		{
			FCustomPropertyTypeLayoutMap& PropertyTypeToLayoutMap = InstancePropertyTypeLayoutMap.FindOrAdd( ForSpecificInstance );
			
			FPropertyTypeLayoutCallbackList* LayoutCallbacks = PropertyTypeToLayoutMap.Find( PropertyTypeName );
			if( LayoutCallbacks )
			{
				LayoutCallbacks->Add( Callback );
			}
			else
			{
				FPropertyTypeLayoutCallbackList NewLayoutCallbacks;
				NewLayoutCallbacks.Add( Callback );
				PropertyTypeToLayoutMap.Add( PropertyTypeName, NewLayoutCallbacks );
			}
		}
		else
		{
			FPropertyTypeLayoutCallbackList* LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find( PropertyTypeName );
			if( LayoutCallbacks )
			{
				LayoutCallbacks->Add( Callback );
			}
			else
			{
				FPropertyTypeLayoutCallbackList NewLayoutCallbacks;
				NewLayoutCallbacks.Add( Callback );
				GlobalPropertyTypeToLayoutMap.Add( PropertyTypeName, NewLayoutCallbacks );
			}
		}
	}
}

void FPropertyEditorModule::UnregisterCustomPropertyTypeLayout( FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier, TSharedPtr<IDetailsView> ForSpecificInstance )
{
	if (!PropertyTypeName.IsValid() || (PropertyTypeName == NAME_None))
	{
		return;
	}

	if (ForSpecificInstance.IsValid())
	{
		FCustomPropertyTypeLayoutMap* PropertyTypeToLayoutMap = InstancePropertyTypeLayoutMap.Find(ForSpecificInstance);
			
		if (PropertyTypeToLayoutMap)
		{
			FPropertyTypeLayoutCallbackList* LayoutCallbacks = PropertyTypeToLayoutMap->Find(PropertyTypeName);
			
			if (LayoutCallbacks)
			{
				LayoutCallbacks->Remove(Identifier);
			}
		}
	}
	else
	{
		FPropertyTypeLayoutCallbackList* LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(PropertyTypeName);

		if (LayoutCallbacks)
		{
			LayoutCallbacks->Remove(Identifier);
		}
	}
}

bool FPropertyEditorModule::HasUnlockedDetailViews() const
{
	uint32 NumUnlockedViews = 0;
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		const TWeakPtr<SDetailsView>& DetailView = AllDetailViews[ ViewIndex ];
		if( DetailView.IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = DetailView.Pin();
			if( DetailViewPin->IsUpdatable() &&  !DetailViewPin->IsLocked() )
			{
				return true;
			}
		}
	}

	return false;
}

/**
 * Refreshes property windows with a new list of objects to view
 * 
 * @param NewObjectList	The list of objects each property window should view
 */
void FPropertyEditorModule::UpdatePropertyViews( const TArray<UObject*>& NewObjectList )
{
	DestroyColorPicker();

	TSet<AActor*> ValidObjects;
	
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if( AllDetailViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ ViewIndex ].Pin();
			if( DetailViewPin->IsUpdatable() )
			{
				if( !DetailViewPin->IsLocked() )
				{
					DetailViewPin->SetObjects(NewObjectList, true);
				}
				else
				{
					DetailViewPin->RemoveInvalidObjects();
				}
			}
		}
	}
}

void FPropertyEditorModule::ReplaceViewedObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap )
{
	// Replace objects from detail views
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if( AllDetailViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ ViewIndex ].Pin();

			DetailViewPin->ReplaceObjects( OldToNewObjectMap );
		}
	}

	// Replace objects from single views
	for( int32 ViewIndex = 0; ViewIndex < AllSinglePropertyViews.Num(); ++ViewIndex )
	{
		if( AllSinglePropertyViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SSingleProperty> SinglePropPin = AllSinglePropertyViews[ ViewIndex ].Pin();

			SinglePropPin->ReplaceObjects( OldToNewObjectMap );
		}
	}
}

void FPropertyEditorModule::RemoveDeletedObjects( TArray<UObject*>& DeletedObjects )
{
	DestroyColorPicker();

	// remove deleted objects from detail views
	for( int32 ViewIndex = 0; ViewIndex < AllDetailViews.Num(); ++ViewIndex )
	{
		if( AllDetailViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SDetailsView> DetailViewPin = AllDetailViews[ ViewIndex ].Pin();

			DetailViewPin->RemoveDeletedObjects( DeletedObjects );
		}
	}

	// remove deleted object from single property views
	for( int32 ViewIndex = 0; ViewIndex < AllSinglePropertyViews.Num(); ++ViewIndex )
	{
		if( AllSinglePropertyViews[ ViewIndex ].IsValid() )
		{
			TSharedPtr<SSingleProperty> SinglePropPin = AllSinglePropertyViews[ ViewIndex ].Pin();

			SinglePropPin->RemoveDeletedObjects( DeletedObjects );
		}
	}
}

bool FPropertyEditorModule::IsCustomizedStruct(const UStruct* Struct, TSharedPtr<IDetailsView> DetailsViewInstance ) const
{
	bool bFound = false;
	if (Struct && !Struct->IsA<UUserDefinedStruct>())
	{
		const FCustomPropertyTypeLayoutMap* Map = InstancePropertyTypeLayoutMap.Find( DetailsViewInstance );

		if( Map )
		{
			bFound = Map->Contains( Struct->GetFName() );
		}
		
		if( !bFound )
		{
			bFound = GlobalPropertyTypeToLayoutMap.Contains( Struct->GetFName() );
		}
	}
	
	return bFound;
}

FPropertyTypeLayoutCallback FPropertyEditorModule::GetPropertyTypeCustomization(const UProperty* Property, const IPropertyHandle& PropertyHandle, TSharedPtr<IDetailsView> DetailsViewInstance )
{
	if( Property )
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(Property);
		bool bStructProperty = StructProperty && StructProperty->Struct;
		const bool bUserDefinedStruct = bStructProperty && StructProperty->Struct->IsA<UUserDefinedStruct>();
		bStructProperty &= !bUserDefinedStruct;

		const UByteProperty* ByteProperty = Cast<UByteProperty>(Property);
		bool bEnumProperty = ByteProperty && ByteProperty->Enum;
		const bool bUserDefinedEnum = bEnumProperty && ByteProperty->Enum->IsA<UUserDefinedEnum>();
		bEnumProperty &= !bUserDefinedEnum;

		const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
		const bool bObjectProperty = ObjectProperty != NULL && ObjectProperty->PropertyClass != NULL;

		FName PropertyTypeName;
		if( bStructProperty )
		{
			PropertyTypeName = StructProperty->Struct->GetFName();
		}
		else if( bEnumProperty )
		{
			PropertyTypeName = ByteProperty->Enum->GetFName();
		}
		else if ( bObjectProperty )
		{
			PropertyTypeName = ObjectProperty->PropertyClass->GetFName();
		}
		else
		{
			PropertyTypeName = Property->GetClass()->GetFName();
		}


		if (PropertyTypeName != NAME_None)
		{
			FCustomPropertyTypeLayoutMap* Map = InstancePropertyTypeLayoutMap.Find( DetailsViewInstance );
			
			FPropertyTypeLayoutCallbackList* LayoutCallbacks = nullptr;
			if( Map )
			{
				LayoutCallbacks = Map->Find( PropertyTypeName );
			}
				
			if( !LayoutCallbacks )
			{
				LayoutCallbacks = GlobalPropertyTypeToLayoutMap.Find(PropertyTypeName);
			}

			if ( LayoutCallbacks )
			{
				const FPropertyTypeLayoutCallback& Callback = LayoutCallbacks->Find(PropertyHandle);
				return Callback;
			}
		}
	}

	return FPropertyTypeLayoutCallback();
}

TSharedRef<class IStructureDetailsView> FPropertyEditorModule::CreateStructureDetailView(const FDetailsViewArgs& DetailsViewArgs, const FStructureDetailsViewArgs& StructureDetailsViewArgs, TSharedPtr<FStructOnScope> StructData, const FText& CustomName)
{
	TSharedRef<SStructureDetailsView> DetailView =
		SNew(SStructureDetailsView)
		.DetailsViewArgs(DetailsViewArgs)
		.CustomName(CustomName);

	struct FStructureDetailsViewFilter
	{
		static bool HasFilter( const FStructureDetailsViewArgs InStructureDetailsViewArgs )
		{
			const bool bShowEverything = InStructureDetailsViewArgs.bShowObjects 
				&& InStructureDetailsViewArgs.bShowAssets 
				&& InStructureDetailsViewArgs.bShowClasses 
				&& InStructureDetailsViewArgs.bShowInterfaces;
			return !bShowEverything;
		}

		static bool PassesFilter( const FPropertyAndParent& PropertyAndParent, const FStructureDetailsViewArgs InStructureDetailsViewArgs )
		{
			const auto ArrayProperty = Cast<UArrayProperty>(&PropertyAndParent.Property);
			const auto PropertyToTest = ArrayProperty ? ArrayProperty->Inner : &PropertyAndParent.Property;

			if( InStructureDetailsViewArgs.bShowClasses && (PropertyToTest->IsA<UClassProperty>() || PropertyToTest->IsA<UAssetClassProperty>()) )
			{
				return true;
			}

			if( InStructureDetailsViewArgs.bShowInterfaces && PropertyToTest->IsA<UInterfaceProperty>() )
			{
				return true;
			}

			const auto ObjectProperty = Cast<UObjectPropertyBase>(PropertyToTest);
			if( ObjectProperty )
			{
				if( InStructureDetailsViewArgs.bShowAssets )
				{
					// Is this an "asset" property?
					if( PropertyToTest->IsA<UAssetObjectProperty>())
					{
						return true;
					}

					// Not an "asset" property, but it may still be a property using an asset class type (such as a raw pointer)
					if( ObjectProperty->PropertyClass )
					{
						// We can use the asset tools module to see whether this type has asset actions (which likely means it's an asset class type)
						FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
						return AssetToolsModule.Get().GetAssetTypeActionsForClass(ObjectProperty->PropertyClass).IsValid();
					}
				}

				return InStructureDetailsViewArgs.bShowObjects;
			}

			return true;
		}
	};

	// Only add the filter if we need to exclude things
	if (FStructureDetailsViewFilter::HasFilter(StructureDetailsViewArgs))
	{
		DetailView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateStatic(&FStructureDetailsViewFilter::PassesFilter, StructureDetailsViewArgs));
	}
	DetailView->SetStructureData(StructData);

	return DetailView;
}
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DestructibleMeshEditorPrivatePCH.h"

#include "ModuleManager.h"
#include "DestructibleMeshEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "AssetToolsModule.h"

#include "ApexDestructibleAssetImport.h"
#include "Engine/DestructibleMesh.h"
#include "Engine/StaticMesh.h"

IMPLEMENT_MODULE( FDestructibleMeshEditorModule, DestructibleMeshEditor );

#define LOCTEXT_NAMESPACE "DestructibleMeshEditor"

const FName DestructibleMeshEditorAppIdentifier = FName(TEXT("DestructibleMeshEditorApp"));


void FDestructibleMeshEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
}


void FDestructibleMeshEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
}


TSharedRef<IDestructibleMeshEditor> FDestructibleMeshEditorModule::CreateDestructibleMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UDestructibleMesh* Table )
{
	TSharedRef< FDestructibleMeshEditor > NewDestructibleMeshEditor( new FDestructibleMeshEditor() );
	NewDestructibleMeshEditor->InitDestructibleMeshEditor( Mode, InitToolkitHost, Table );
	return NewDestructibleMeshEditor;
}

UDestructibleMesh* FDestructibleMeshEditorModule::CreateDestructibleMeshFromStaticMesh(UObject* InParent, UStaticMesh* StaticMesh, FName Name, EObjectFlags Flags, FText& OutErrorMsg)
{
	if (StaticMesh == NULL)
	{
		OutErrorMsg = LOCTEXT( "StaticMeshInvalid", "Static Mesh is Invalid!" );
		return NULL;
	}

	FString DestructibleName;
	
	if (Name == NAME_None)
	{
		DestructibleName = StaticMesh->GetName();
		DestructibleName += TEXT("_DM");
	}
	else
	{
		DestructibleName = *Name.ToString();
	}

	UDestructibleMesh* DestructibleMesh = FindObject<UDestructibleMesh>( InParent, *DestructibleName );

	// If there is an existing mesh, ask the user if they want to use the existing mesh, replace the existing mesh, or create a new mesh.
	if (DestructibleMesh != NULL)
	{
		enum ENameConflictDialogReturnType
		{
			NCD_None = 0,
			NCD_Use,
			NCD_Replace,
			NCD_Create
		};
#if 0	// BRG - removed SSimpleDialog, needs replacing
		FSimpleDialogModule& SimpleDialogModule = FModuleManager::LoadModuleChecked<FSimpleDialogModule>( TEXT("SimpleDialog") );
		SimpleDialogModule.CreateSimpleDialog(NSLOCTEXT("SimpleDialogModule", "DestructibleMeshAlreadyExistsTitle", "DestructibleMesh Already Exists").ToString(), NSLOCTEXT("SimpleDialogModule", "DestructibleMeshAlreadyExistsMessage", "A DestructibleMesh by the same name already exists.  Select an action.").ToString());
		SimpleDialogModule.AddButton(NCD_Use, NSLOCTEXT("SimpleDialogModule", "UseExistingDestructibleMesh", "Use").ToString(), NSLOCTEXT("SimpleDialogModule", "UseExistingDestructibleMeshTip", "Open the DestructibleMesh editor with the existing mesh").ToString());
		SimpleDialogModule.AddButton(NCD_Replace, NSLOCTEXT("SimpleDialogModule", "ReplaceExistingDestructibleMesh", "Replace").ToString(), NSLOCTEXT("SimpleDialogModule", "ReplaceExistingDestructibleMeshTip", "Replace the existing DestructibleMesh with a new one, and show in the DestructibleMesh editor").ToString());
		SimpleDialogModule.AddButton(NCD_Create, NSLOCTEXT("SimpleDialogModule", "CreateNewDestructibleMesh", "Create").ToString(), NSLOCTEXT("SimpleDialogModule", "CreateNewDestructibleMeshTip", "Create a new DestructibleMesh with a different name, and show in the DestructibleMesh editor").ToString());
		const uint32 UserResponse = SimpleDialogModule.ShowSimpleDialog();
#endif
		const uint32 UserResponse = NCD_None;
		switch (UserResponse)
		{
		default:	// Default to using the same mesh
		case NCD_Use:
			return DestructibleMesh;
		case NCD_Replace:
			break;
		case NCD_Create:
			Name = MakeUniqueObjectName(StaticMesh->GetOuter(), UDestructibleMesh::StaticClass(), Name);
			DestructibleName = *Name.ToString();
			DestructibleMesh = NULL;	// So that one will be created
			break;
		}
	}
	
	// Create the new UDestructibleMesh object if we still haven't found one after a possible resolution above
	if (DestructibleMesh == NULL)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		DestructibleMesh = Cast<UDestructibleMesh>(AssetToolsModule.Get().CreateAsset(DestructibleName, FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName()), UDestructibleMesh::StaticClass(), NULL));

		if(!DestructibleMesh)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Name"), FText::FromString( StaticMesh->GetName() ));

			OutErrorMsg = FText::Format( LOCTEXT( "DestructibleMeshFailed", "Failed to Create Destructible Mesh Asset from {Name}!" ), Arguments );
			return NULL;
		}
	}

	DestructibleMesh->BuildFromStaticMesh(*StaticMesh);
	
	return DestructibleMesh;
}

#undef LOCTEXT_NAMESPACE
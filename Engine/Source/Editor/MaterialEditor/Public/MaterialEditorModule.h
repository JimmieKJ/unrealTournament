// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __MaterialEditorModule_h__
#define __MaterialEditorModule_h__

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "IMaterialEditor.h"



extern const FName MaterialEditorAppIdentifier;
extern const FName MaterialInstanceEditorAppIdentifier;

/**
 * Material editor module interface
 */
class IMaterialEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/**
	 * Creates a new material editor, either for a material or a material function
	 */
	virtual TSharedRef<IMaterialEditor> CreateMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UMaterial* Material ) = 0;
	virtual TSharedRef<IMaterialEditor> CreateMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UMaterialFunction* MaterialFunction ) = 0;
	virtual TSharedRef<IMaterialEditor> CreateMaterialInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UMaterialInstance* MaterialInstance ) = 0;

	/**
	 * Retrieves all visible parameters within the material.
	 *
	 * @param	Material			The material to retrieve the parameters from.
	 * @param	MaterialInstance	The material instance that contains all parameter overrides.
	 * @param	VisisbleExpressions	The array that will contain the id's of the visible parameter expressions.
	 */
	virtual void GetVisibleMaterialParameters(const class UMaterial* Material, class UMaterialInstance* MaterialInstance, TArray<struct FGuid>& VisibleExpressions) = 0;

	/** Delegates to be called to extend the material menus */
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<FExtender>, FMaterialMenuExtender, const TSharedRef<FUICommandList>);
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<FExtender>, FMaterialMenuExtender_MaterialInterface, const UMaterialInterface*);
	virtual TArray<FMaterialMenuExtender>& GetAllMaterialCanvasMenuExtenders() {return MaterialCanvasMenuExtenders;}
	virtual TArray<FMaterialMenuExtender_MaterialInterface>& GetAllMaterialDragDropContextMenuExtenders() {return MaterialInheritanceMenuExtenders;}

private:
	/** All extender delegates for the material menus */
	TArray<FMaterialMenuExtender> MaterialCanvasMenuExtenders;
	TArray<FMaterialMenuExtender_MaterialInterface> MaterialInheritanceMenuExtenders;
};

#endif // __MaterialEditorModule_h__

//! @file SubstanceHelpers.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"

#include <UnrealEd.h>
#include <Factories.h>
#include <MainFrame.h>
#include <DesktopPlatformModule.h>

#include "SubstanceEditorHelpers.h"

#include "SubstanceCoreClasses.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceCorePreset.h"
#include "SubstanceFPackage.h"
#include "SubstanceFGraph.h"

#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionOneMinus.h"

using namespace Substance;

namespace SubstanceEditor
{
namespace Helpers
{


void CreateMaterialExpression(output_inst_t* OutputInst, UMaterial* UnrealMaterial);


UMaterialExpressionTextureSampleParameter2D* CreateSampler(UMaterial* UnrealMaterial, UTexture* UnrealTexture, output_desc_t* OutputDesc)
{
	UMaterialExpressionTextureSampleParameter2D* UnrealTextureExpression =
		NewObject<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial);

	UnrealTextureExpression->MaterialExpressionEditorX = -200;
	UnrealTextureExpression->MaterialExpressionEditorY = UnrealMaterial->Expressions.Num() * 180;
	UnrealTextureExpression->Texture = UnrealTexture;
	UnrealTextureExpression->ParameterName = *OutputDesc->Identifier;
	UnrealTextureExpression->SamplerType = UnrealTextureExpression->GetSamplerTypeForTexture(UnrealTexture);

	return UnrealTextureExpression;
}


//! @brief Create an Unreal Material for the given graph-instance
UMaterial* CreateMaterial(graph_inst_t* GraphInstance,
	const FString & MaterialName,
	UObject* Outer)
{
	// create an unreal material asset
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();

	UMaterial* UnrealMaterial = 
		(UMaterial*)MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Substance::Helpers::CreateObjectPackage(Outer, MaterialName),
			*MaterialName, 
			RF_Standalone|RF_Public, NULL, GWarn );

	// textures and properties
	for (auto ItOut = GraphInstance->Outputs.itfront(); ItOut; ++ItOut)
	{
		output_inst_t* OutputInst = &(*ItOut);
		output_desc_t* OutputDesc = OutputInst->GetOutputDesc();

		CreateMaterialExpression(
			OutputInst,
			UnrealMaterial);
	}

	// special case: emissive only materials
	TArray<FName> ParamNames;
	TArray<FGuid> ParamIds;
	UnrealMaterial->GetAllTextureParameterNames(ParamNames, ParamIds);
	if (ParamNames.Num() == 1)
	{
		if (ParamNames[0].ToString() == TEXT("emissive"))
		{
			UnrealMaterial->SetShadingModel(MSM_Unlit);
		}
	}

	// special case: no roughness but glossiness
	if (!UnrealMaterial->Roughness.IsConnected())
	{
		for (auto ItOut = GraphInstance->Outputs.itfront(); ItOut; ++ItOut)
		{
			output_inst_t* OutputInst = &(*ItOut);
			output_desc_t* OutputDesc = OutputInst->GetOutputDesc();

			UTexture* Texture = *OutputInst->Texture;

			if (OutputDesc->Channel == CHAN_Glossiness && Texture)
			{
				// and link it to the material
				UMaterialExpressionOneMinus* OneMinus = NewObject<UMaterialExpressionOneMinus>(UnrealMaterial);

				UMaterialExpressionTextureSampleParameter2D* UnrealTextureExpression =
					CreateSampler(UnrealMaterial, Texture, OutputDesc);

				UnrealTextureExpression->MaterialExpressionEditorX -= 200;
				OneMinus->MaterialExpressionEditorX = -200;
				OneMinus->MaterialExpressionEditorY = UnrealTextureExpression->MaterialExpressionEditorY;
				
				UnrealTextureExpression->ConnectExpression(OneMinus->GetInput(0), 0);
				UnrealMaterial->Roughness.Expression = OneMinus;
				
				UnrealMaterial->Expressions.Add(UnrealTextureExpression);
				UnrealMaterial->Expressions.Add(OneMinus);
			}
		}
	}

	// special case: no roughness and no metallic
	if (!UnrealMaterial->Roughness.IsConnected() && !UnrealMaterial->Metallic.IsConnected())
	{
		for (auto ItOut = GraphInstance->Outputs.itfront(); ItOut; ++ItOut)
		{
			output_inst_t* OutputInst = &(*ItOut);
			output_desc_t* OutputDesc = OutputInst->GetOutputDesc();

			UTexture* Texture = *OutputInst->Texture;

			if (OutputDesc->Channel == CHAN_Specular && Texture)
			{
				UMaterialExpressionTextureSampleParameter2D* UnrealTextureExpression =
					CreateSampler(UnrealMaterial, Texture, OutputDesc);

				UnrealMaterial->Specular.Expression = UnrealTextureExpression;

				UnrealMaterial->Expressions.Add(UnrealTextureExpression);
			}
		}
	}

	// let the material update itself if necessary
	UnrealMaterial->PreEditChange(NULL);
	UnrealMaterial->PostEditChange();
	
	return UnrealMaterial;
}


void CreateMaterialExpression(output_inst_t* OutputInst, UMaterial* UnrealMaterial)
{
	output_desc_t* OutputDesc = OutputInst->GetOutputDesc();
	FExpressionInput * MaterialInput = NULL;

	switch(OutputDesc->Channel)
	{
	case CHAN_BaseColor:
	case CHAN_Diffuse:
		MaterialInput = &UnrealMaterial->BaseColor;
		break;

	case CHAN_Metallic:
		MaterialInput = &UnrealMaterial->Metallic;
		break;

	case CHAN_SpecularLevel:
		MaterialInput = &UnrealMaterial->Specular;
		break;

	case CHAN_Roughness:
		MaterialInput = &UnrealMaterial->Roughness;
		break;

	case CHAN_Emissive:
		MaterialInput = &UnrealMaterial->EmissiveColor;
		break;

	case CHAN_Normal:
		MaterialInput = &UnrealMaterial->Normal;
		break;

	case CHAN_Opacity:
		MaterialInput = &UnrealMaterial->OpacityMask;
		UnrealMaterial->BlendMode = EBlendMode::BLEND_Masked;
		break;

	case CHAN_AmbientOcclusion:
		MaterialInput = &UnrealMaterial->AmbientOcclusion;
		break;

	default:
		// nothing relevant to plug, skip it
		return;
		break;
	}

	UTexture* UnrealTexture = *OutputInst->Texture;

	if (MaterialInput->IsConnected())
	{
		// slot already used by another output
		return;
	}

	if (UnrealTexture)
	{
		UMaterialExpressionTextureSampleParameter2D* UnrealTextureExpression = 
			CreateSampler(UnrealMaterial, UnrealTexture, OutputDesc);

		UnrealMaterial->Expressions.Add(UnrealTextureExpression);
		MaterialInput->Expression = UnrealTextureExpression;

		TArray<FExpressionOutput> Outputs;
		Outputs = MaterialInput->Expression->GetOutputs();

		FExpressionOutput* Output = &Outputs[0];
		MaterialInput->Mask = Output->Mask;
		MaterialInput->MaskR = Output->MaskR;
		MaterialInput->MaskG = Output->MaskG;
		MaterialInput->MaskB = Output->MaskB;
		MaterialInput->MaskA = Output->MaskA;
	}
}


static void* ChooseParentWindowHandle()
{
	void* ParentWindowWindowHandle = NULL;
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
	{
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}

	return ParentWindowWindowHandle;
}


/**** @param Title                  The title of the dialog
	* @param FileTypes              Filter for which file types are accepted and should be shown
	* @param InOutLastPath          Keep track of the last location from which the user attempted an import
	* @param DefaultFile            Default file name to use for saving.
	* @param OutOpenFilenames       The list of filenames that the user attempted to open
	*
	* @return true if the dialog opened successfully and the user accepted; false otherwise.
	*/
bool SaveFile( const FString& Title, const FString& FileTypes, FString& InOutLastPath, const FString& DefaultFile, FString& OutFilename )
{
	OutFilename = FString();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bFileChosen = false;
	TArray<FString> OutFilenames;
	if (DesktopPlatform)
	{
		bFileChosen = DesktopPlatform->SaveFileDialog(
			ChooseParentWindowHandle(),
			Title,
			InOutLastPath,
			DefaultFile,
			FileTypes,
			EFileDialogFlags::None,
			OutFilenames
		);
	}

	bFileChosen = (OutFilenames.Num() > 0);

	if (bFileChosen)
	{
		// User successfully chose a file; remember the path for the next time the dialog opens.
		InOutLastPath = OutFilenames[0];
		OutFilename = OutFilenames[0];
	}

	return bFileChosen;
}

/**** @param Title                  The title of the dialog
	* @param FileTypes              Filter for which file types are accepted and should be shown
	* @param InOutLastPath    Keep track of the last location from which the user attempted an import
	* @param DialogMode             Multiple items vs single item.
	* @param OutOpenFilenames       The list of filenames that the user attempted to open
	*
	* @return true if the dialog opened successfully and the user accepted; false otherwise.
	*/
bool OpenFiles( const FString& Title, const FString& FileTypes, FString& InOutLastPath, EFileDialogFlags::Type DialogMode, TArray<FString>& OutOpenFilenames ) 
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if ( DesktopPlatform )
	{
		bOpened = DesktopPlatform->OpenFileDialog(
			ChooseParentWindowHandle(),
			Title,
			InOutLastPath,
			TEXT(""),
			FileTypes,
			DialogMode,
			OutOpenFilenames
		);
	}

	bOpened = (OutOpenFilenames.Num() > 0);

	if ( bOpened )
	{
		// User successfully chose a file; remember the path for the next time the dialog opens.
		InOutLastPath = OutOpenFilenames[0];
	}

	return bOpened;
}


bool ExportPresetFromGraph(USubstanceGraphInstance* GraphInstance)
{
	check(GraphInstance != NULL);
	preset_t Preset;
	FString Data;
	
	Preset.ReadFrom(GraphInstance->Instance);
	WritePreset(Preset, Data);
	FString PresetFileName = SubstanceEditor::Helpers::ExportPresetFile(Preset.mLabel);
	if (PresetFileName.Len())
	{
		return FFileHelper::SaveStringToFile(Data, *PresetFileName);
	}

	return false;
}


bool ImportAndApplyPresetForGraph(USubstanceGraphInstance* GraphInstance)
{
	FString Data;
	FString PresetFileName;
	presets_t Presets;
	
	PresetFileName = SubstanceEditor::Helpers::ImportPresetFile();
	FFileHelper::LoadFileToString(Data, *PresetFileName);
	ParsePresets(Presets, Data);
	for (presets_t::TIterator ItPres = Presets.CreateIterator(); ItPres; ++ItPres)
	{
		preset_t Preset = *ItPres;

		// apply it [but which one ? for now that will be the last one]
		if (Preset.Apply(GraphInstance->Instance))
		{
			Substance::Helpers::RenderAsync(GraphInstance->Instance);
			return true;
		}
	}
	
	return false;
}


FString ExportPresetFile(FString SuggestedFilename)
{
        FString OutOpenFilename;
        FString InOutLastPath = ".";
        if ( SaveFile( TEXT("Export Preset"), TEXT("Export Types (*.sbsprs)|*.sbsprs;|All Files|*.*"),
		       InOutLastPath,
			   SuggestedFilename + ".sbsprs",
		       OutOpenFilename )
             )
                 return OutOpenFilename;
        else
                 return FString();
}

FString ImportPresetFile()
{
        TArray<FString> OutOpenFilenames;
        FString InOutLastPath = ".";
        if ( OpenFiles( TEXT("Import Preset"), TEXT("Import Types (*.sbsprs)|*.sbsprs;|All Files|*.*"),
                        InOutLastPath,
                        EFileDialogFlags::None, //single selection
                        OutOpenFilenames )
             )
                 return OutOpenFilenames[0];
        else
                 return FString();
}

} // namespace Helpers
} // namespace Substance

//! @file SubstanceEdXmlHelper.cpp
//! @brief Substance description XML parsing helper
//! @author Gonzalez Antoine - Allegorithmic
//! @date 20101229
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreXmlHelper.h"
#include "SubstanceFGraph.h"
#include "SubstanceFPackage.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceCorePreset.h"
#include "substance_public.h"

#include <memory>	//std::auto_ptr
#include <stdlib.h>	//for strtoul

#include "EngineSettings.h"


namespace Substance
{
namespace Helpers
{

#if WITH_EDITOR
//! @brief Parse an output node from the Substance xml desc file
//! @note Not reading the default size as we do not use it 
bool ParseOutput(const IXMLNode* OutputNode, graph_desc_t* ParentGraph)
{
	output_desc_t Output;

	FString Attribute = OutputNode->GetAttribute("uid");
	if (Attribute.Len() <= 0)
	{
		return false;
	}
	Output.Uid = appAtoul(*Attribute);

	Attribute = OutputNode->GetAttribute("identifier");
	if (Attribute.Len() <= 0)
	{
		return false;
	}
	Output.Identifier = Attribute;

	Attribute = OutputNode->GetAttribute("format");
	if (Attribute.Len() <= 0)
	{
		return false;
	}
	// Not every formats is supported, 
	// check it and force another is not supported
	SubstancePixelFormat initialFormat = (SubstancePixelFormat)FCString::Atoi(*Attribute);
	Output.Format = Helpers::ValidateFormat(initialFormat);

	// try to read the channel from the GUI info
	Output.Channel = CHAN_Undef;
	const IXMLNode* OutputGuiNode = OutputNode->FindChildNode("outputgui");

	if (OutputGuiNode)
	{
		Attribute = OutputGuiNode->GetAttribute("label");
		if (Attribute.Len() <= 0)
		{
			return false;
		}
		Output.Label = Attribute;

		const IXMLNode* ChannelsNode = OutputGuiNode->FindChildNode("channels");

		if (ChannelsNode)
		{
			const IXMLNode* ChannelNode = ChannelsNode->FindChildNode("channel");

			if (ChannelNode)
			{
				Attribute = ChannelNode->GetAttribute("names");

				if (FString(TEXT("diffuse")) == Attribute)
				{
					Output.Channel = CHAN_Diffuse;
				}
				else if (FString(TEXT("baseColor")) == Attribute)
				{
					Output.Channel = CHAN_BaseColor;
				}
				else if (FString(TEXT("normal")) == Attribute)
				{
					Output.Channel = CHAN_Normal;

					if (Output.Format != Substance_PF_DXT5)
					{
						FConfigFile EngineSettings;
						FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *FString(FPlatformProperties::PlatformName()));
						FString UseDXT5NormalMapsString;
						if (EngineSettings.GetString(TEXT("SystemSettings"), TEXT("Compat.UseDXT5NormalMaps"), UseDXT5NormalMapsString) &&
							FCString::ToBool(*UseDXT5NormalMapsString))
						{
							UE_LOG(LogSubstanceCore, Warning, TEXT("The current settings requires normal maps to be DXT5 with a *X*Y layout."));
						}
					}
				}
				else if (FString(TEXT("opacity")) == Attribute)
				{
					Output.Channel = CHAN_Opacity;
				}
				else if (FString(TEXT("emissive")) == Attribute)
				{
					Output.Channel = CHAN_Emissive;
				}
				else if (FString(TEXT("ambient")) == Attribute)
				{
					Output.Channel = CHAN_Ambient;
				}
				else if (FString(TEXT("ambientOcclusion")) == Attribute)
				{
					Output.Channel = CHAN_AmbientOcclusion;
				}
				else if (FString(TEXT("mask")) == Attribute)
				{
					Output.Channel = CHAN_Mask;
				}
				else if (FString(TEXT("bump")) == Attribute)
				{
					Output.Channel = CHAN_Bump;
				}
				else if (FString(TEXT("height")) == Attribute)
				{
					Output.Channel = CHAN_Height;
				}
				else if (FString(TEXT("displacement")) == Attribute)
				{
					Output.Channel = CHAN_Displacement;
				}
				else if (FString(TEXT("specular")) == Attribute)
				{
					Output.Channel = CHAN_Specular;
				}
				else if (FString(TEXT("specularLevel")) == Attribute)
				{
					Output.Channel = CHAN_SpecularLevel;
				}
				else if (FString(TEXT("specularColor")) == Attribute)
				{
					Output.Channel = CHAN_SpecularColor;
				}
				else if (FString(TEXT("glossiness")) == Attribute)
				{
					Output.Channel = CHAN_Glossiness;
				}
				else if (FString(TEXT("roughness")) == Attribute)
				{
					Output.Channel = CHAN_Roughness;
				}
				else if (FString(TEXT("anisotropyLevel")) == Attribute)
				{
					Output.Channel = CHAN_AnisotropyLevel;
				}
				else if (FString(TEXT("anisotropyAngle")) == Attribute)
				{
					Output.Channel = CHAN_AnisotropyAngle;
				}
				else if (FString(TEXT("transmissive")) == Attribute)
				{
					Output.Channel = CHAN_Transmissive;
				}
				else if (FString(TEXT("reflection")) == Attribute)
				{
					Output.Channel = CHAN_Reflection;
				}
				else if (FString(TEXT("refraction")) == Attribute)
				{
					Output.Channel = CHAN_Refraction;
				}
				else if (FString(TEXT("environment")) == Attribute)
				{
					Output.Channel = CHAN_Environment;
				}
				else if (FString(TEXT("IOR")) == Attribute)
				{
					Output.Channel = CHAN_IOR;
				}
				else if (FString(TEXT("scattering0")) == Attribute)
				{
					Output.Channel = CU_SCATTERING0;
				}
				else if (FString(TEXT("scattering1")) == Attribute)
				{
					Output.Channel = CU_SCATTERING1;
				}
				else if (FString(TEXT("scattering2")) == Attribute)
				{
					Output.Channel = CU_SCATTERING2;
				}
				else if (FString(TEXT("scattering3")) == Attribute)
				{
					Output.Channel = CU_SCATTERING3;
				}
				else if (FString(TEXT("metallic")) == Attribute)
				{
					Output.Channel = CHAN_Metallic;
				}
			}
		}
	}
	else
	{
		Output.Label = Output.Identifier;
	}

	// give the ownership to the parent graph
	ParentGraph->OutputDescs.push(Output);

	return true;
}


//! @brief Parse an input node from the substance xml companion file
bool ParseInput(const IXMLNode* InputNode, graph_desc_t* ParentGraph)
{
	TSharedPtr<input_desc_t> Input;

	FString Attribute = InputNode->GetAttribute("type");
	if (Attribute.Len() <= 0)
	{
		return false;
	}
	int32 Type = FCString::Atoi(*Attribute);

	switch((SubstanceInputType)Type)
	{
	case Substance_IType_Float:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<float>);
		break;
	case Substance_IType_Float2:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec2float_t>);
		break;
	case Substance_IType_Float3:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec3float_t>);
		break;
	case Substance_IType_Float4:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec4float_t>);
		break;
	case Substance_IType_Integer:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<int32>);
		break;
	case Substance_IType_Integer2:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec2int_t>);
		break;
	case Substance_IType_Integer3:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec3int_t>);
		break;
	case Substance_IType_Integer4:
		Input = TSharedPtr<input_desc_t>(new FNumericalInputDesc<vec4int_t>);
		break;
	case Substance_IType_Image:
		Input = TSharedPtr<input_desc_t>(new FImageInputDesc());
		break;
	default:
		return false;
		break;
	}
	Input->Type = Type;

	Input->Identifier = 
		InputNode->GetAttribute("identifier");
	if (Input->Identifier.Len()<=0)
	{
		return false;
	}

	// $outputsize, $pixelsize and $randomseed are heavyduty input
	if (InputNode->GetAttribute("identifier")[0] == '$')
	{
		Input->IsHeavyDuty = true;
	}
	else
	{
		Input->IsHeavyDuty = false;
	}

	Attribute = InputNode->GetAttribute("uid");
	if (Attribute.Len()<=0)
	{
		return false;
	}
	Input->Uid = appAtoul(*Attribute);

	//Parse alter outputs list and match uid with outputs
	FString AlteroutputsNode(InputNode->GetAttribute("alteroutputs"));
	TArray<FString> AlteredOutputsArray;
	AlteroutputsNode.ParseIntoArray(AlteredOutputsArray, TEXT(","), true); 

	for (int32 i = 0; i < AlteredOutputsArray.Num() ; ++i)
	{
		uint32 Uid = appAtoul(*AlteredOutputsArray[i]);
		
		//push the output in the list of outputs altered by the input
		Input->AlteredOutputUids.push(Uid);

		List<output_desc_t>::TIterator ItDesc(ParentGraph->OutputDescs.itfront());

		for (; ItDesc ; ++ItDesc)
		{
			if ((*ItDesc).Uid == Uid)
			{
				//and push the modifying uid in the output's list
				(*ItDesc).AlteringInputUids.push(Input->Uid);
				break;
			}
		}
	}

	Input->AlteredOutputUids.getArray().Sort();
	FString DefaultValue = InputNode->GetAttribute("default");

	if (DefaultValue.Len())
	{
		TArray<FString> DefaultValueArray;
		DefaultValue.ParseIntoArray(DefaultValueArray,TEXT(","),true);

		switch ((SubstanceInputType)Input->Type)
		{
		case Substance_IType_Float:
			((FNumericalInputDesc<float>*)Input.Get())->DefaultValue =
				FCString::Atof(*DefaultValueArray[0]);
			break;
		case Substance_IType_Float2:
			((FNumericalInputDesc<vec2float_t>*)Input.Get())->DefaultValue.X =
				FCString::Atof(*DefaultValueArray[0]);
			((FNumericalInputDesc<vec2float_t>*)Input.Get())->DefaultValue.Y =
				FCString::Atof(*DefaultValueArray[1]);
			break;
		case Substance_IType_Float3:
			((FNumericalInputDesc<vec3float_t>*)Input.Get())->DefaultValue.X =
				FCString::Atof(*DefaultValueArray[0]);
			((FNumericalInputDesc<vec3float_t>*)Input.Get())->DefaultValue.Y =
				FCString::Atof(*DefaultValueArray[1]);
			((FNumericalInputDesc<vec3float_t>*)Input.Get())->DefaultValue.Z =
				FCString::Atof(*DefaultValueArray[2]);
			break;
		case Substance_IType_Float4:
			((FNumericalInputDesc<vec4float_t>*)Input.Get())->DefaultValue.X =
				FCString::Atof(*DefaultValueArray[0]);
			((FNumericalInputDesc<vec4float_t>*)Input.Get())->DefaultValue.Y =
				FCString::Atof(*DefaultValueArray[1]);
			((FNumericalInputDesc<vec4float_t>*)Input.Get())->DefaultValue.Z =
				FCString::Atof(*DefaultValueArray[2]);
			((FNumericalInputDesc<vec4float_t>*)Input.Get())->DefaultValue.W =
				FCString::Atof(*DefaultValueArray[3]);
			break;
		case Substance_IType_Integer:
			((FNumericalInputDesc<int32>*)Input.Get())->DefaultValue =
				FCString::Atoi(*DefaultValueArray[0]);
			break;
		case Substance_IType_Integer2:
			((FNumericalInputDesc<vec2int_t>*)Input.Get())->DefaultValue.X =
				FCString::Atoi(*DefaultValueArray[0]);
			((FNumericalInputDesc<vec2int_t>*)Input.Get())->DefaultValue.Y =
				FCString::Atoi(*DefaultValueArray[1]);
			break;
		case Substance_IType_Integer3:
			((FNumericalInputDesc<vec3int_t>*)Input.Get())->DefaultValue.X =
				FCString::Atoi(*DefaultValueArray[0]);
			((FNumericalInputDesc<vec3int_t>*)Input.Get())->DefaultValue.Y =
				FCString::Atoi(*DefaultValueArray[1]);
			((FNumericalInputDesc<vec3int_t>*)Input.Get())->DefaultValue.Z =
				FCString::Atoi(*DefaultValueArray[2]);
			break;
		case Substance_IType_Integer4:
			((FNumericalInputDesc<vec4int_t>*)Input.Get())->DefaultValue.X =
				FCString::Atoi(*DefaultValueArray[0]);
			((FNumericalInputDesc<vec4int_t>*)Input.Get())->DefaultValue.Y =
				FCString::Atoi(*DefaultValueArray[1]);
			((FNumericalInputDesc<vec4int_t>*)Input.Get())->DefaultValue.Z =
				FCString::Atoi(*DefaultValueArray[2]);
			((FNumericalInputDesc<vec4int_t>*)Input.Get())->DefaultValue.W =
				FCString::Atoi(*DefaultValueArray[3]);
			break;
		case Substance_IType_Image:
			break;
		default:
			// should have returned in the previous switch-case
			check(0);
			break;
		}
	}

	// parse GUI specific information: min, max, prefered widget
	const IXMLNode* InputguiNode = InputNode->FindChildNode("inputgui");
	if (!InputguiNode)
	{
		((FInputDescBase*)Input.Get())->Widget =
			Substance::Input_NoWidget;
	}
	else
	{
		Input->Label = InputguiNode->GetAttribute("label");
		if (Input->Label.Len()<=0)
		{
			return false;
		}
		
		FString GroupAttribute(InputguiNode->GetAttribute("group"));
		if (GroupAttribute.Len())
		{
			Input->Group = GroupAttribute;
		}

		FString Widget = InputguiNode->GetAttribute("widget");

		// special case for old sbsar files where image input desc where not complete
		if (0 == Widget.Len() && (SubstanceInputType)Input->Type == Substance_IType_Image)
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Image;
			((FImageInputDesc*)Input.Get())->Label =
				InputguiNode->GetAttribute("label");
			((FImageInputDesc*)Input.Get())->Desc =
				FString(TEXT("No Description."));
		}
		else if (Widget == FString(TEXT("image")))
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Image;
			((FImageInputDesc*)Input.Get())->Label =
				InputguiNode->GetAttribute("label");
			((FImageInputDesc*)Input.Get())->Desc =
				InputguiNode->GetAttribute("description");
		}
		else if (Widget == FString(TEXT("combobox")))
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Combobox;

			TSharedPtr<input_desc_t> NewInput(
				new FNumericalInputDescComboBox(
					((FNumericalInputDesc<int32>*)Input.Get())));

			FNumericalInputDescComboBox* NewDesc = 
				(FNumericalInputDescComboBox*)NewInput.Get();

			// parse the combobox enum
			const IXMLNode* GuicomboboxNode =
				InputguiNode->FindChildNode("guicombobox");
			
			if (GuicomboboxNode)
			{
				const IXMLNode* GuicomboboxitemNode = 
					GuicomboboxNode->FindChildNode("guicomboboxitem");
				
				while (GuicomboboxitemNode)
				{
					FString item_text(
						GuicomboboxitemNode->GetAttribute("text"));
					
					FString item_value_attribute = 
							GuicomboboxitemNode->GetAttribute("value");
					if (item_value_attribute.Len() <= 0)
					{
						continue;
					}
					int32 item_value = FCString::Atoi(*item_value_attribute);

					NewDesc->ValueText.Add(item_value, item_text);

					GuicomboboxitemNode = 
						GuicomboboxitemNode->GetNextNode();
				}

				Input = NewInput;
			}
		}
		else if (Widget == FString(TEXT("togglebutton")))
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Togglebutton;
		}
		else if (Widget == FString(TEXT("color")))
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Color;
		}
		else if (Widget == FString(TEXT("angle")))
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Angle;
		}
		else // default to ("slider")
		{
			((FInputDescBase*)Input.Get())->Widget =
				Substance::Input_Slider;
		}

		const IXMLNode* InputsubguiNode = InputguiNode->GetFirstChildNode();
		if (InputsubguiNode)
		{
			TArray<FString> MinValueArray;
			MinValueArray.AddZeroed(4);
			FString MinValue(InputsubguiNode->GetAttribute("min"));

			if (MinValue.Len())
			{
				MinValue.ParseIntoArray(MinValueArray,TEXT(","),true);
				int32 IdxComp = 0;

				switch ((SubstanceInputType)Input->Type)
				{
				case Substance_IType_Float:
					((FNumericalInputDesc<float>*)Input.Get())->Min =
						FCString::Atof(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Float2:
					((FNumericalInputDesc<vec2float_t>*)Input.Get())->Min.X =
						FCString::Atof(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec2float_t>*)Input.Get())->Min.Y =
						FCString::Atof(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Float3:
					((FNumericalInputDesc<vec3float_t>*)Input.Get())->Min.X =
						FCString::Atof(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3float_t>*)Input.Get())->Min.Y =
						FCString::Atof(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3float_t>*)Input.Get())->Min.Z =
						FCString::Atof(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Float4:
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Min.X =
						FCString::Atof(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Min.Y =
						FCString::Atof(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Min.Z =
						FCString::Atof(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Min.W =
						FCString::Atof(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer:
					((FNumericalInputDesc<int32>*)Input.Get())->Min =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer2:
					((FNumericalInputDesc<vec2int_t>*)Input.Get())->Min.X =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec2int_t>*)Input.Get())->Min.Y =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer3:
					((FNumericalInputDesc<vec3int_t>*)Input.Get())->Min.X =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3int_t>*)Input.Get())->Min.Y =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3int_t>*)Input.Get())->Min.Z =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer4:
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Min.X =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Min.Y =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Min.Z =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Min.W =
						FCString::Atoi(*MinValueArray[IdxComp++]);
					break;
				default:
					// should have returned in the previous switch-case
					check(0);
					break;
				}
			}

			TArray<FString> MaxValueArray;
			MaxValueArray.AddZeroed(4);
			FString MaxValue(InputsubguiNode->GetAttribute("max"));

			if (MaxValue.Len())
			{
				MaxValue.ParseIntoArray(MaxValueArray,TEXT(","),true);
				int32 IdxComp = 0;

				switch ((SubstanceInputType)Input->Type)
				{
				case Substance_IType_Float:
					((FNumericalInputDesc<float>*)Input.Get())->Max =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Float2:
					((FNumericalInputDesc<vec2float_t>*)Input.Get())->Max.X =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec2float_t>*)Input.Get())->Max.Y =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Float3:
					((FNumericalInputDesc<vec3float_t>*)Input.Get())->Max.X =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3float_t>*)Input.Get())->Max.Y =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3float_t>*)Input.Get())->Max.Z =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Float4:
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Max.X =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Max.Y =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Max.Z =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4float_t>*)Input.Get())->Max.W =
						FCString::Atof(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer:
					((FNumericalInputDesc<int32>*)Input.Get())->Max =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer2:
					((FNumericalInputDesc<vec2int_t>*)Input.Get())->Max.X =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec2int_t>*)Input.Get())->Max.Y =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer3:
					((FNumericalInputDesc<vec3int_t>*)Input.Get())->Max.X =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3int_t>*)Input.Get())->Max.Y =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec3int_t>*)Input.Get())->Max.Z =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					break;
				case Substance_IType_Integer4:
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Max.X =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Max.Y =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Max.Z =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					((FNumericalInputDesc<vec4int_t>*)Input.Get())->Max.W =
						FCString::Atoi(*MaxValueArray[IdxComp++]);
					break;
				default:
					// should have returned in the previous switch-case
					check(0);
					break;
				}
			}
			
			FString ClampValue(InputsubguiNode->GetAttribute("clamp"));
			if (ClampValue.Len())
			{
				((FNumericalInputDesc<float>*)Input.Get())->IsClamped =
						ClampValue==FString(TEXT("on")) ? true : false;
			}
		}
	}
	
	if (0 == Input->Label.Len())
	{
		Input->Label = Input->Identifier;
	}

	if (Input->Label == "$outputsize")
	{
		((FNumericalInputDesc<vec2int_t>*)Input.Get())->Max.X = 11;
		((FNumericalInputDesc<vec2int_t>*)Input.Get())->Max.Y = 11;

		((FNumericalInputDesc<vec2int_t>*)Input.Get())->Min.X = 1;
		((FNumericalInputDesc<vec2int_t>*)Input.Get())->Min.Y = 1;
		((FNumericalInputDesc<vec2int_t>*)Input.Get())->IsClamped = true;
	}

	if (Input->Label == "$randomseed")
	{
		((FNumericalInputDesc<int32>*)Input.Get())->Min = 0;
		((FNumericalInputDesc<int32>*)Input.Get())->Max = INT_MAX - 100; // temp workaround
		((FNumericalInputDesc<int32>*)Input.Get())->IsClamped = true;
	}

	Input->Index = ParentGraph->InputDescs.Num();

	ParentGraph->InputDescs.push(Input);

	return true;
}
#endif // WITH_EDITOR


//! @brief Parse Xml companion file
//! @param XmlContent The string containing the xml data
//! @return true for success, false for failure
bool ParseSubstanceXml(
	const TArray<FString>& XmlContent,
	Substance::List<graph_desc_t*>& Graphs,
	TArray<uint32>& assembly_uid)
{
#if WITH_EDITOR
	if (XmlContent.Num() <= 0)
	{
		// abort if invalid
		UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, sbsar does not contain xml graph description."));

		return false;
	}

	TArray<FString>::TConstIterator ItXml(XmlContent);
	for (;ItXml;++ItXml)
	{
		AutoXMLFile Document(TCHAR_TO_UTF8(*(*ItXml)));

		if (!Document.IsValid())
		{
			UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description."));
			return false;
		}

		const IXMLNode* RootNode = Document.GetRootNode();
			
		if (!RootNode)
		{
			UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (sbsdescription)."));
			return false;
		}
		else
		{
			FString Attribute = RootNode->GetAttribute("asmuid");
			if (Attribute.Len() <= 0)
			{
				UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (asmuid)."));
				return false;
			}
			assembly_uid.Add(appAtoul(*Attribute));

			const IXMLNode* GlobalNode = RootNode->FindChildNode("global");

			const IXMLNode* normalformatinputnode = NULL;
			const IXMLNode* timeinputnode = NULL;

			if (GlobalNode!=NULL)
			{
				const IXMLNode* inputlistnode = 
					GlobalNode->FindChildNode("inputs");

				const IXMLNode* inputnode = 
					inputlistnode->FindChildNode("input");

				while(inputnode)
				{
					FString identifier(inputnode->GetAttribute("identifier"));
					
					if (identifier==TEXT("$time"))
					{
						timeinputnode = inputnode;
					}
					else if (identifier==TEXT("$normalformat"))
					{
						normalformatinputnode = inputnode;
					}
					else
					{
						//! @todo global not handled
					}

					inputnode = inputnode->GetNextNode();
				}
			}

			const IXMLNode* GraphListNode = RootNode->FindChildNode("graphs");
			if (!GraphListNode)
			{
				UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (graphs)."));
				return false;
			}

			FString FGraphCount(GraphListNode->GetAttribute("count"));
			int32 GraphCount = FCString::Atoi(*FGraphCount);

			if (0 == GraphCount)
			{
				UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (no graph)."));
				return false;
			}

			// Parse all the graphs
			for (const IXMLNode* GraphNode = GraphListNode->GetFirstChildNode(); GraphNode != NULL;GraphNode = GraphNode->GetNextNode())
			{
				// Create the graph object
				std::auto_ptr<graph_desc_t> OneGraph(new graph_desc_t);

				OneGraph->PackageUrl =
					GraphNode->GetAttribute("pkgurl");
				OneGraph->Label =
					GraphNode->GetAttribute("label");
				OneGraph->Description =
					GraphNode->GetAttribute("description");
				
				// Parse outputs
				{
					const IXMLNode* OutputListNode = GraphNode->FindChildNode("outputs");
		
					if (!OutputListNode)
					{
						UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (no outputs for %s)"), *OneGraph->Label);
						return false;
					}

					FString FCount(OutputListNode->GetAttribute("count"));
					int32 OutputCount = FCString::Atoi(*FCount);

					if (OutputCount<=0)
					{
						UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (no outputs for %s)"), *OneGraph->Label);
						
						// temp: no failure as long as the Ticket #1418 is not fixed.
						continue;
					}

					OneGraph->OutputDescs.Reserve(OutputCount);
					
					for (const IXMLNode* OutputNode = OutputListNode->GetFirstChildNode(); OutputNode != NULL;OutputNode = OutputNode->GetNextNode())
					{
						if (!ParseOutput(OutputNode,OneGraph.get()))
						{
							UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (invalid output)"));
							return false;
						}
					}
				}

				// Parse inputs
				{
					const IXMLNode* InputListNode = GraphNode->FindChildNode("inputs");

					if (!InputListNode)
					{
						UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (no inputs)"));
						return false;
					}
					FString FCount(InputListNode->GetAttribute("count"));
					int32 InputCount = FCString::Atoi(*FCount);

					if	(InputCount >= 0)
					{
						OneGraph->InputDescs.Reserve(InputCount);

						for (const IXMLNode* InputNode = InputListNode->GetFirstChildNode(); InputNode != NULL;InputNode = InputNode->GetNextNode())
						{
							// Parse input
							if ( !ParseInput(InputNode, OneGraph.get()))
							{
								UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (invalid input)"));
								return false;
							}
						}

						if (normalformatinputnode)
						{
							// Parse input
							if ( !ParseInput(normalformatinputnode, OneGraph.get()))
							{
								UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (invalid input)"));
								return false;
							}
						}

						if (timeinputnode)
						{
							// Parse input
							if ( !ParseInput(timeinputnode, OneGraph.get()))
							{
								UE_LOG(LogSubstanceCore, Error, TEXT("Cannot import, invalid xml graph description (invalid input)"));
								return false;
							}
						}
					}
				}

				// Add the graph to his package
				Graphs.push(OneGraph.release());
			}

			Substance::List<graph_desc_t*>::TIterator itGraph(Graphs.itfront());
			for (;itGraph;++itGraph)
			{
				(*itGraph)->OutputDescs[0].AlteringInputUids.getArray().Sort();
			}
		}
	}
	return true;
#endif // WITH_EDITOR

	return false;
}

bool ParseSubstanceXmlPreset(
	presets_t& presets,
	const FString& XmlContent,
	const graph_desc_t* graphDesc)
{
#if WITH_EDITOR
	AutoXMLFile XmlDocument(TCHAR_TO_UTF8(*XmlContent));

	if (false == XmlDocument.IsValid())
	{
		UE_LOG(LogSubstanceCore, Error, TEXT("Invalid preset file (xml parsing error)."));
		return false;
	}

	const IXMLNode* RootNode = XmlDocument.GetRootNode();
	const IXMLNode* sbspresetnode = RootNode->FindChildNode("sbspreset");

	// Reserve presets array
	{
		SIZE_T presetsCount = 0;
		while (sbspresetnode)
		{
			sbspresetnode = 
				sbspresetnode->GetNextNode();
			++presetsCount;
		}
		presets.AddZeroed(presetsCount+presets.Num());
	}

	presets_t::TIterator ItPres(presets);

	for (const IXMLNode* child = RootNode->GetFirstChildNode(); child != NULL;child = child->GetNextNode())
	{
		FPreset &preset = *ItPres;

		// Parse preset
		preset.mPackageUrl = 
			child->GetAttribute("pkgurl");
		preset.mPackageUrl = graphDesc!=NULL ?
			graphDesc->PackageUrl :
			preset.mPackageUrl;
		preset.mLabel = 
			child->GetAttribute("label");
		preset.mDescription = 
			child->GetAttribute("description");

		
		// Loop on preset inputs
		for (const IXMLNode* presetinputnode = child->GetFirstChildNode(); presetinputnode != NULL;presetinputnode = presetinputnode->GetNextNode())
		{
			int32 Idx = preset.mInputValues.AddZeroed(1);
			FPreset::FInputValue& inpvalue = preset.mInputValues[Idx];

			inpvalue.mUid = 0;

			FString Attribute = 
				presetinputnode->GetAttribute("uid");
			if (Attribute.Len() <= 0)
			{
				return false;
			}
			inpvalue.mUid = appAtoul(*Attribute);

			inpvalue.mValue = 
				presetinputnode->GetAttribute("value");

			int type = 0xFFFFu;

			if (graphDesc)
			{
				// Internal preset
				TSharedPtr<input_desc_t> ite = FindInput(
					graphDesc->InputDescs,
					inpvalue.mUid);

				if (ite.Get())
				{
					type = ite->Type;
					inpvalue.mIdentifier = ite->Identifier;
				}
				else
				{
					// TODO Warning: cannot match internal preset
					inpvalue.mType = (SubstanceInputType)0xFFFF;
				}
			}
			else
			{
				// External preset
				FString FType(presetinputnode->GetAttribute("type"));
				type = FCString::Atoi(*FType);

				inpvalue.mIdentifier = 
						presetinputnode->GetAttribute("identifier");
			}

			inpvalue.mType = (SubstanceInputType)type;
		}
		
		++ItPres;
	}

	return true;
#endif // WITH_EDITOR

	return false;
}


void WriteSubstanceXmlPreset(
	preset_t& preset,
	FString& XmlContent)
{
	XmlContent += TEXT("<sbspresets formatversion=\"1.0\" count=\"1\">");
	XmlContent += TEXT("<sbspreset pkgurl=\"");
	XmlContent += preset.mPackageUrl;
	XmlContent += TEXT("\" label=\"");
	XmlContent += preset.mLabel;
	XmlContent += TEXT("\" >\n");

	TArray<Substance::FPreset::FInputValue>::TIterator 
		ItV(preset.mInputValues.itfront());

	for (; ItV ; ++ItV)
	{
		XmlContent += TEXT("<presetinput identifier=\"");
		XmlContent += (*ItV).mIdentifier;
		XmlContent += TEXT("\" uid=\"");
		XmlContent += FString::Printf(TEXT("%u"),(*ItV).mUid);
		XmlContent += TEXT("\" type=\"");
		XmlContent += FString::Printf(TEXT("%d"),(int32)(*ItV).mType);
		XmlContent += TEXT("\" value=\"");
		XmlContent += (*ItV).mValue;
		XmlContent += TEXT("\" />\n");
	}

	XmlContent += TEXT("</sbspreset>\n");
	XmlContent += TEXT("</sbspresets>");
}

} // namespace Substance
} // namespace Helpers

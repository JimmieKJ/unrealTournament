// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "WidgetBlueprintGeneratedClass.h"
#include "WidgetAnimation.h"
#include "WidgetTree.h"

void FBackendHelperUMG::WidgetFunctionsInHeader(FEmitterLocalContext& Context)
{
	if (Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s(TArray<FName>& SlotNames) const override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s(const class ITargetPlatform* TargetPlatform) override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, PreSave)));
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s() override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, CustomNativeInitilize)));
	}
}

void FBackendHelperUMG::AdditionalHeaderIncludeForWidget(FEmitterLocalContext& Context)
{
	if (Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(TEXT("#include \"Runtime/UMG/Public/UMG.h\""));
	}
}

void FBackendHelperUMG::CreateClassSubobjects(FEmitterLocalContext& Context, bool bCreate, bool bInitialize)
{
	if (auto WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		if (WidgetClass->WidgetTree)
		{
			ensure(WidgetClass->WidgetTree->GetOuter() == Context.GetCurrentlyGeneratedClass());
			FEmitDefaultValueHelper::HandleClassSubobject(Context, WidgetClass->WidgetTree, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, bCreate, bInitialize);
		}
		for (auto Anim : WidgetClass->Animations)
		{
			ensure(Anim->GetOuter() == Context.GetCurrentlyGeneratedClass());
			FEmitDefaultValueHelper::HandleClassSubobject(Context, Anim, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, bCreate, bInitialize);
		}
	}
}

void FBackendHelperUMG::EmitWidgetInitializationFunctions(FEmitterLocalContext& Context)
{
	if (auto WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.ResetPropertiesForInaccessibleStructs();

		const FString CppClassName = FEmitHelper::GetCppName(WidgetClass);

		auto GenerateLocalProperty = [](FEmitterLocalContext& InContext, UProperty* InProperty, const uint8* DataPtr) -> FString
		{
			check(InProperty && DataPtr);
			const FString NativeName = InContext.GenerateUniqueLocalName();
			
			const uint32 CppTemplateTypeFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst | EPropertyExportCPPFlags::CPPF_NoRef;
			const FString Target = InContext.ExportCppDeclaration(InProperty, EExportedDeclaration::Local, CppTemplateTypeFlags, true);

			InContext.AddLine(FString::Printf(TEXT("%s %s;"), *Target, *NativeName));
			FEmitDefaultValueHelper::InnerGenerate(InContext, InProperty, NativeName, DataPtr, nullptr, true);
			return NativeName;
		};

		{	// GetSlotNames
			Context.AddLine(FString::Printf(TEXT("void %s::%s(TArray<FName>& SlotNames) const"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();

			const FString LocalNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("NamedSlots")), reinterpret_cast<const uint8*>(&WidgetClass->NamedSlots));
			Context.AddLine(FString::Printf(TEXT("SlotNames.Append(%s);"), *LocalNativeName));

			Context.DecreaseIndent();
			Context.AddLine(TEXT("}"));
		}

		{	// CustomNativeInitilize
			Context.AddLine(FString::Printf(TEXT("void %s::%s()"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, CustomNativeInitilize)));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();

			const FString WidgetTreeStr = Context.FindGloballyMappedObject(WidgetClass->WidgetTree, UWidgetTree::StaticClass(), true);
			ensure(!WidgetTreeStr.IsEmpty());
			const FString AnimationsArrayNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("Animations")), reinterpret_cast<const uint8*>(&WidgetClass->Animations));
			const FString BindingsArrayNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("Bindings")), reinterpret_cast<const uint8*>(&WidgetClass->Bindings));
					
			Context.AddLine(FString::Printf(TEXT("UWidgetBlueprintGeneratedClass::%s(this, GetClass(), %s, %s, %s, %s, %s);")
				, GET_FUNCTION_NAME_STRING_CHECKED(UWidgetBlueprintGeneratedClass, InitializeWidgetStatic)
				, WidgetClass->bCanEverTick ? TEXT("true") : TEXT("false")
				, WidgetClass->bCanEverPaint ? TEXT("true") : TEXT("false")
				, *WidgetTreeStr
				, *AnimationsArrayNativeName
				, *BindingsArrayNativeName));

			Context.DecreaseIndent();
			Context.AddLine(TEXT("}"));
		}

		// PreSave
		Context.AddLine(FString::Printf(TEXT("void %s::%s(const class ITargetPlatform* TargetPlatform)"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, PreSave)));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("Super::%s(TargetPlatform);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, PreSave)));
		Context.AddLine(TEXT("TArray<FName> LocalNamedSlots;"));
		Context.AddLine(FString::Printf(TEXT("%s(LocalNamedSlots);"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
		Context.AddLine(TEXT("RemoveObsoleteBindings(LocalNamedSlots);")); //RemoveObsoleteBindings is protected - no check
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
}


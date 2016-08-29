// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "Runtime/Core/Public/Math/Box2D.h"

void FEmitDefaultValueHelper::OuterGenerate(FEmitterLocalContext& Context
	, const UProperty* Property
	, const FString& OuterPath
	, const uint8* DataContainer
	, const uint8* OptionalDefaultDataContainer
	, EPropertyAccessOperator AccessOperator
	, bool bAllowProtected)
{
	// Determine if the given property contains an instanced default subobject reference. We only get here if the values are not identical.
	auto IsInstancedSubobjectLambda = [&](int32 ArrayIndex) -> bool
	{
		if (auto ObjectProperty = Cast<UObjectProperty>(Property))
		{
			check(DataContainer);
			check(OptionalDefaultDataContainer);

			auto ObjectPropertyValue = ObjectProperty->GetObjectPropertyValue_InContainer(DataContainer, ArrayIndex);
			auto DefaultObjectPropertyValue = ObjectProperty->GetObjectPropertyValue_InContainer(OptionalDefaultDataContainer, ArrayIndex);
			if (ObjectPropertyValue && ObjectPropertyValue->IsDefaultSubobject() && DefaultObjectPropertyValue && DefaultObjectPropertyValue->IsDefaultSubobject() && ObjectPropertyValue->GetFName() == DefaultObjectPropertyValue->GetFName())
			{
				return true;
			}
		}

		return false;
	};
	check(Property);
	if (Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("FEmitDefaultValueHelper Skip EditorOnly or Transient property: %s"), *Property->GetPathName());
		return;
	}

	if (Property->IsA<UDelegateProperty>() || Property->IsA<UMulticastDelegateProperty>())
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("FEmitDefaultValueHelper delegate property: %s"), *Property->GetPathName());
		return;
	}

	for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
	{
		if (!OptionalDefaultDataContainer
			|| ((Property->PropertyFlags & CPF_Config) != 0) 
			|| (!Property->Identical_InContainer(DataContainer, OptionalDefaultDataContainer, ArrayIndex) && !IsInstancedSubobjectLambda(ArrayIndex)))
		{
			FString PathToMember;
			UBlueprintGeneratedClass* PropertyOwnerAsBPGC = Cast<UBlueprintGeneratedClass>(Property->GetOwnerClass());
			UScriptStruct* PropertyOwnerAsScriptStruct = Cast<UScriptStruct>(Property->GetOwnerStruct());
			const bool bNoExportProperty = PropertyOwnerAsScriptStruct
				&& PropertyOwnerAsScriptStruct->IsNative()
				&& (PropertyOwnerAsScriptStruct->StructFlags & STRUCT_NoExport)
				// && !PropertyOwnerAsScriptStruct->GetBoolMetaData(TEXT("BlueprintType"))
				&& ensure(EPropertyAccessOperator::Dot == AccessOperator);
			if (PropertyOwnerAsBPGC && !Context.Dependencies.WillClassBeConverted(PropertyOwnerAsBPGC))
			{
				ensure(EPropertyAccessOperator::None != AccessOperator);
				const FString OperatorStr = (EPropertyAccessOperator::Dot == AccessOperator) ? TEXT("&") : TEXT("");
				const FString ContainerStr = (EPropertyAccessOperator::None == AccessOperator) ? TEXT("this") : FString::Printf(TEXT("%s(%s)"), *OperatorStr, *OuterPath);

				PathToMember = FString::Printf(TEXT("FUnconvertedWrapper__%s(%s).GetRef__%s()"), *FEmitHelper::GetCppName(PropertyOwnerAsBPGC), *ContainerStr
					, *UnicodeToCPPIdentifier(Property->GetName(), false, nullptr));
			}
			else if (bNoExportProperty || Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate) || (!bAllowProtected && Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)))
			{
				const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(Property);
				const bool bBietfield = BoolProperty && !BoolProperty->IsNativeBool();
				const FString OperatorStr = (EPropertyAccessOperator::Dot == AccessOperator) ? TEXT("&") : TEXT("");
				const FString ContainerStr = (EPropertyAccessOperator::None == AccessOperator) ? TEXT("this") : OuterPath;
				if (bBietfield)
				{
					const FString PropertyLocalName = FEmitHelper::GenerateGetPropertyByName(Context, Property);
					const FString ValueStr = Context.ExportTextItem(Property, Property->ContainerPtrToValuePtr<uint8>(DataContainer, ArrayIndex));
					Context.AddLine(FString::Printf(TEXT("(((UBoolProperty*)%s)->%s(%s(%s), %s, %d));")
						, *PropertyLocalName
						, GET_FUNCTION_NAME_STRING_CHECKED(UBoolProperty, SetPropertyValue_InContainer)
						, *OperatorStr
						, *ContainerStr
						, *ValueStr
						, ArrayIndex));
					continue;
				}
				const FString GetPtrStr = bNoExportProperty
					? FEmitHelper::AccessInaccessiblePropertyUsingOffset(Context, Property, ContainerStr, OperatorStr, ArrayIndex)
					: FEmitHelper::AccessInaccessibleProperty(Context, Property, ContainerStr, OperatorStr, ArrayIndex, ENativizedTermUsage::UnspecifiedOrReference, nullptr);
				PathToMember = Context.GenerateUniqueLocalName();
				Context.AddLine(FString::Printf(TEXT("auto& %s = %s;"), *PathToMember, *GetPtrStr));
			}
			else
			{
				const FString AccessOperatorStr = (EPropertyAccessOperator::None == AccessOperator) ? TEXT("")
					: ((EPropertyAccessOperator::Pointer == AccessOperator) ? TEXT("->") : TEXT("."));
				const bool bStaticArray = (Property->ArrayDim > 1);
				const FString ArrayPost = bStaticArray ? FString::Printf(TEXT("[%d]"), ArrayIndex) : TEXT("");
				PathToMember = FString::Printf(TEXT("%s%s%s%s"), *OuterPath, *AccessOperatorStr, *FEmitHelper::GetCppName(Property), *ArrayPost);
			}

			{
				const uint8* ValuePtr = Property->ContainerPtrToValuePtr<uint8>(DataContainer, ArrayIndex);
				const uint8* DefaultValuePtr = OptionalDefaultDataContainer ? Property->ContainerPtrToValuePtr<uint8>(OptionalDefaultDataContainer, ArrayIndex) : nullptr;
				InnerGenerate(Context, Property, PathToMember, ValuePtr, DefaultValuePtr);
			}
		}
	}
}

void FEmitDefaultValueHelper::GenerateGetDefaultValue(const UUserDefinedStruct* Struct, FEmitterLocalContext& Context)
{
	check(Struct);
	const FString StructName = FEmitHelper::GetCppName(Struct);
	Context.Header.AddLine(FString::Printf(TEXT("static %s GetDefaultValue()"), *StructName));
	Context.Header.AddLine(TEXT("{"));

	Context.Header.IncreaseIndent();
	Context.Header.AddLine(FString::Printf(TEXT("FStructOnScope StructOnScope(%s::StaticStruct());"), *StructName));
	Context.Header.AddLine(FString::Printf(TEXT("%s& DefaultData__ = *((%s*)StructOnScope.GetStructMemory());"), *StructName, *StructName));
	{
		TGuardValue<FCodeText*> OriginalDefaultTarget(Context.DefaultTarget, &Context.Header);
		FStructOnScope StructData(Struct);
		FStructureEditorUtils::Fill_MakeStructureDefaultValue(Struct, StructData.GetStructMemory());
		FStructOnScope RawDefaultStructOnScope(Struct);
		for (auto Property : TFieldRange<const UProperty>(Struct))
		{
			OuterGenerate(Context, Property, TEXT("DefaultData__"), StructData.GetStructMemory(), RawDefaultStructOnScope.GetStructMemory(), EPropertyAccessOperator::Dot);
		}
	}
	Context.Header.AddLine(TEXT("return DefaultData__;"));
	Context.Header.DecreaseIndent();

	Context.Header.AddLine(TEXT("}"));
}

void FEmitDefaultValueHelper::InnerGenerate(FEmitterLocalContext& Context, const UProperty* Property, const FString& PathToMember, const uint8* ValuePtr, const uint8* DefaultValuePtr, bool bWithoutFirstConstructionLine)
{
	auto OneLineConstruction = [](FEmitterLocalContext& LocalContext, const UProperty* LocalProperty, const uint8* LocalValuePtr, FString& OutSingleLine, bool bGenerateEmptyStructConstructor) -> bool
	{
		bool bComplete = true;
		FString ValueStr = HandleSpecialTypes(LocalContext, LocalProperty, LocalValuePtr);
		if (ValueStr.IsEmpty())
		{
			ValueStr = LocalContext.ExportTextItem(LocalProperty, LocalValuePtr);
			auto StructProperty = Cast<const UStructProperty>(LocalProperty);
			if (ValueStr.IsEmpty() && StructProperty)
			{
				check(StructProperty->Struct);
				if (bGenerateEmptyStructConstructor)
				{
					ValueStr = FString::Printf(TEXT("%s%s"), *FEmitHelper::GetCppName(StructProperty->Struct), FEmitHelper::EmptyDefaultConstructor(StructProperty->Struct)); //don;t override existing values
				}
				bComplete = false;
			}
			else if (ValueStr.IsEmpty())
			{
				UE_LOG(LogK2Compiler, Error, TEXT("FEmitDefaultValueHelper Cannot generate initilization: %s"), *LocalProperty->GetPathName());
			}
		}
		OutSingleLine += ValueStr;
		return bComplete;
	};

	auto StructProperty = Cast<const UStructProperty>(Property);
	check(!StructProperty || StructProperty->Struct);
	auto ArrayProperty = Cast<const UArrayProperty>(Property);
	check(!ArrayProperty || ArrayProperty->Inner);

	if (!bWithoutFirstConstructionLine)
	{
		FString ValueStr;
		const bool bComplete = OneLineConstruction(Context, Property, ValuePtr, ValueStr, false);
		if (!ValueStr.IsEmpty())
		{
			Context.AddLine(FString::Printf(TEXT("%s = %s;"), *PathToMember, *ValueStr));
		}
		// array initialization "array_var = TArray<..>()" is complete, but it still needs items.
		if (bComplete && !ArrayProperty)
		{
			return;
		}
	}

	if (StructProperty)
	{
		// Create default struct instance, only when DefaultValuePtr is null.
		FStructOnScope DefaultStructOnScope(DefaultValuePtr ? nullptr : StructProperty->Struct);

		for (auto LocalProperty : TFieldRange<const UProperty>(StructProperty->Struct))
		{
			OuterGenerate(Context, LocalProperty, PathToMember, ValuePtr
				, (DefaultValuePtr ? DefaultValuePtr : DefaultStructOnScope.GetStructMemory())
				, EPropertyAccessOperator::Dot);
		}
	}
	
	if (ArrayProperty)
	{
		FScriptArrayHelper ScriptArrayHelper(ArrayProperty, ValuePtr);
		UStructProperty* InnerStructProperty = Cast<UStructProperty>(ArrayProperty->Inner);
		
		const bool bNoExportStructure = InnerStructProperty && InnerStructProperty->Struct
			&& InnerStructProperty->Struct->IsNative()
			&& (InnerStructProperty->Struct->StructFlags & STRUCT_NoExport);
		const bool bInitializeWithoutScriptStruct = bNoExportStructure;

		UScriptStruct* RegularInnerStruct = nullptr;
		if (!bInitializeWithoutScriptStruct)
		{
			if (InnerStructProperty && !FEmitDefaultValueHelper::SpecialStructureConstructor(InnerStructProperty->Struct, nullptr, nullptr))
			{
				RegularInnerStruct = InnerStructProperty->Struct;
			}
		}

		if (ScriptArrayHelper.Num())
		{
			const TCHAR* ArrayReserveFunctionName = RegularInnerStruct ? TEXT("AddUninitialized") : TEXT("Reserve");
			Context.AddLine(FString::Printf(TEXT("%s.%s(%d);"), *PathToMember, ArrayReserveFunctionName, ScriptArrayHelper.Num()));

			if (RegularInnerStruct)
			{
				const FString InnerStructStr = Context.FindGloballyMappedObject(RegularInnerStruct, UScriptStruct::StaticClass());
				Context.AddLine(FString::Printf(TEXT("%s->%s(%s.GetData(), %d);")
					, *InnerStructStr
					, GET_FUNCTION_NAME_STRING_CHECKED(UStruct, InitializeStruct)
					, *PathToMember
					, ScriptArrayHelper.Num()));
			}
		}

		const FStructOnScope DefaultStruct(RegularInnerStruct);

		for (int32 Index = 0; Index < ScriptArrayHelper.Num(); ++Index)
		{
			const uint8* LocalValuePtr = ScriptArrayHelper.GetRawPtr(Index);

			bool bComplete = false;
			if (!RegularInnerStruct)
			{
				FString ValueStr;
				bComplete = OneLineConstruction(Context, ArrayProperty->Inner, LocalValuePtr, ValueStr, bInitializeWithoutScriptStruct);
				ensure(bComplete || bInitializeWithoutScriptStruct);
				Context.AddLine(FString::Printf(TEXT("%s.Add(%s);"), *PathToMember, *ValueStr));
			}
			
			if (RegularInnerStruct || (bInitializeWithoutScriptStruct && !bComplete))
			{
				FString LocalPathToMember = FString::Printf(TEXT("%s[%d]"), *PathToMember, Index);
				if (RegularInnerStruct)
				{
					const FString ArrayElementRefName = Context.GenerateUniqueLocalName();
					Context.AddLine(FString::Printf(TEXT("auto& %s = %s;"), *ArrayElementRefName, *LocalPathToMember));
					LocalPathToMember = ArrayElementRefName;
				}
				InnerGenerate(Context, ArrayProperty->Inner, LocalPathToMember, LocalValuePtr, DefaultStruct.GetStructMemory(), true);
			}
		}
	}
}

bool FEmitDefaultValueHelper::SpecialStructureConstructor(const UStruct* Struct, const uint8* ValuePtr, /*out*/ FString* OutResult)
{
	check(ValuePtr || !OutResult);

	if (FLatentActionInfo::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FLatentActionInfo* LatentActionInfo = reinterpret_cast<const FLatentActionInfo*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FLatentActionInfo(%d, %d, TEXT(\"%s\"), this)")
				, LatentActionInfo->Linkage
				, LatentActionInfo->UUID
				, *LatentActionInfo->ExecutionFunction.ToString().ReplaceCharWithEscapedChar());
		}
		return true;
	}

	if (TBaseStructure<FTransform>::Get() == Struct)
	{
		if (OutResult)
		{
			const FTransform* Transform = reinterpret_cast<const FTransform*>(ValuePtr);
			const auto Rotation = Transform->GetRotation();
			const auto Translation = Transform->GetTranslation();
			const auto Scale = Transform->GetScale3D();
			*OutResult = FString::Printf(TEXT("FTransform( FQuat(%s,%s,%s,%s), FVector(%s,%s,%s), FVector(%s,%s,%s) )"),
				*FEmitHelper::FloatToString(Rotation.X), *FEmitHelper::FloatToString(Rotation.Y), *FEmitHelper::FloatToString(Rotation.Z), *FEmitHelper::FloatToString(Rotation.W),
				*FEmitHelper::FloatToString(Translation.X), *FEmitHelper::FloatToString(Translation.Y), *FEmitHelper::FloatToString(Translation.Z),
				*FEmitHelper::FloatToString(Scale.X), *FEmitHelper::FloatToString(Scale.Y), *FEmitHelper::FloatToString(Scale.Z));
		}
		return true;
	}

	if (TBaseStructure<FVector>::Get() == Struct)
	{
		if (OutResult)
		{
			const FVector* Vector = reinterpret_cast<const FVector*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FVector(%s, %s, %s)"), *FEmitHelper::FloatToString(Vector->X), *FEmitHelper::FloatToString(Vector->Y), *FEmitHelper::FloatToString(Vector->Z));
		}
		return true;
	}

	if (TBaseStructure<FGuid>::Get() == Struct)
	{
		if (OutResult)
		{
			const FGuid* Guid = reinterpret_cast<const FGuid*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FGuid(0x%08X, 0x%08X, 0x%08X, 0x%08X)"), Guid->A, Guid->B, Guid->C, Guid->D);
		}
		return true;
	}

	if (TBaseStructure<FRotator>::Get() == Struct)
	{
		if (OutResult)
		{
			const FRotator* Rotator = reinterpret_cast<const FRotator*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FRotator(%s, %s, %s)"), *FEmitHelper::FloatToString(Rotator->Pitch), *FEmitHelper::FloatToString(Rotator->Yaw), *FEmitHelper::FloatToString(Rotator->Roll));
		}
		return true;
	}

	if (TBaseStructure<FLinearColor>::Get() == Struct)
	{
		if (OutResult)
		{
			const FLinearColor* LinearColor = reinterpret_cast<const FLinearColor*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FLinearColor(%s, %s, %s, %s)"), *FEmitHelper::FloatToString(LinearColor->R), *FEmitHelper::FloatToString(LinearColor->G), *FEmitHelper::FloatToString(LinearColor->B), *FEmitHelper::FloatToString(LinearColor->A));
		}
		return true;
	}

	if (TBaseStructure<FColor>::Get() == Struct)
	{
		if (OutResult)
		{
			const FColor* Color = reinterpret_cast<const FColor*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FColor(%d, %d, %d, %d)"), Color->R, Color->G, Color->B, Color->A);
		}
		return true;
	}

	if (TBaseStructure<FVector2D>::Get() == Struct)
	{
		if (OutResult)
		{
			const FVector2D* Vector2D = reinterpret_cast<const FVector2D*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FVector2D(%s, %s)"), *FEmitHelper::FloatToString(Vector2D->X), *FEmitHelper::FloatToString(Vector2D->Y));
		}
		return true;
	}

	if (TBaseStructure<FBox2D>::Get() == Struct)
	{
		if (OutResult)
		{
			const FBox2D* Box2D = reinterpret_cast<const FBox2D*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("CreateFBox2D(FVector2D(%s, %s), FVector2D(%s, %s), %s)")
				, *FEmitHelper::FloatToString(Box2D->Min.X)
				, *FEmitHelper::FloatToString(Box2D->Min.Y)
				, *FEmitHelper::FloatToString(Box2D->Max.X)
				, *FEmitHelper::FloatToString(Box2D->Max.Y)
				, Box2D->bIsValid ? TEXT("true") : TEXT("false"));
		}
		return true;
	}

	if (TBaseStructure<FFloatRangeBound>::Get() == Struct)
	{
		if (OutResult)
		{
			const FFloatRangeBound* FloatRangeBound = reinterpret_cast<const FFloatRangeBound*>(ValuePtr);
			if (FloatRangeBound->IsExclusive())
			{
				*OutResult = FString::Printf(TEXT("FFloatRangeBound::%s(%s)"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Exclusive), *FEmitHelper::FloatToString(FloatRangeBound->GetValue()));
			}
			if (FloatRangeBound->IsInclusive())
			{
				*OutResult = FString::Printf(TEXT("FFloatRangeBound::%s(%s)"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Inclusive), *FEmitHelper::FloatToString(FloatRangeBound->GetValue()));
			}
			if (FloatRangeBound->IsOpen())
			{
				*OutResult = FString::Printf(TEXT("FFloatRangeBound::%s()"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Open));
			}
		}
		return true;
	}

	if (TBaseStructure<FFloatRange>::Get() == Struct)
	{
		if (OutResult)
		{
			const FFloatRange* FloatRangeBound = reinterpret_cast<const FFloatRange*>(ValuePtr);

			FString LowerBoundStr;
			FFloatRangeBound LowerBound = FloatRangeBound->GetLowerBound();
			SpecialStructureConstructor(TBaseStructure<FFloatRangeBound>::Get(), (uint8*)&LowerBound, &LowerBoundStr);

			FString UpperBoundStr;
			FFloatRangeBound UpperBound = FloatRangeBound->GetUpperBound();
			SpecialStructureConstructor(TBaseStructure<FFloatRangeBound>::Get(), (uint8*)&UpperBound, &UpperBoundStr);

			*OutResult = FString::Printf(TEXT("FFloatRange(%s, %s)"), *LowerBoundStr, *UpperBoundStr);
		}
		return true;
	}

	if (TBaseStructure<FInt32RangeBound>::Get() == Struct)
	{
		if (OutResult)
		{
			const FInt32RangeBound* RangeBound = reinterpret_cast<const FInt32RangeBound*>(ValuePtr);
			if (RangeBound->IsExclusive())
			{
				*OutResult = FString::Printf(TEXT("FInt32RangeBound::%s(%d)"), GET_FUNCTION_NAME_STRING_CHECKED(FInt32RangeBound, Exclusive), RangeBound->GetValue());
			}
			if (RangeBound->IsInclusive())
			{
				*OutResult = FString::Printf(TEXT("FInt32RangeBound::%s(%d)"), GET_FUNCTION_NAME_STRING_CHECKED(FInt32RangeBound, Exclusive), RangeBound->GetValue());
			}
			if (RangeBound->IsOpen())
			{
				*OutResult = FString::Printf(TEXT("FInt32RangeBound::%s()"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Open));
			}
		}
		return true;
	}

	if (TBaseStructure<FInt32Range>::Get() == Struct)
	{
		if (OutResult)
		{
			const FInt32Range* RangeBound = reinterpret_cast<const FInt32Range*>(ValuePtr);

			FString LowerBoundStr;
			FInt32RangeBound LowerBound = RangeBound->GetLowerBound();
			SpecialStructureConstructor(TBaseStructure<FInt32RangeBound>::Get(), (uint8*)&LowerBound, &LowerBoundStr);

			FString UpperBoundStr;
			FInt32RangeBound UpperBound = RangeBound->GetUpperBound();
			SpecialStructureConstructor(TBaseStructure<FInt32RangeBound>::Get(), (uint8*)&UpperBound, &UpperBoundStr);

			*OutResult = FString::Printf(TEXT("FInt32Range(%s, %s)"), *LowerBoundStr, *UpperBoundStr);
		}
		return true;
	}

	if (TBaseStructure<FFloatInterval>::Get() == Struct)
	{
		if (OutResult)
		{
			const FFloatInterval* Interval = reinterpret_cast<const FFloatInterval*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FFloatInterval(%s, %s)"), *FEmitHelper::FloatToString(Interval->Min), *FEmitHelper::FloatToString(Interval->Max));
		}
		return true;
	}

	if (TBaseStructure<FInt32Interval>::Get() == Struct)
	{
		if (OutResult)
		{
			const FInt32Interval* Interval = reinterpret_cast<const FInt32Interval*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FFloatInterval(%d, %d)"), Interval->Min, Interval->Max);
		}
		return true;
	}

	return false;
}

FString FEmitDefaultValueHelper::HandleSpecialTypes(FEmitterLocalContext& Context, const UProperty* Property, const uint8* ValuePtr)
{
	//TODO: Use Path maps for Objects
	if (auto ObjectProperty = Cast<UObjectProperty>(Property))
	{
		UObject* Object = ObjectProperty->GetPropertyValue(ValuePtr);
		if (Object)
		{
			{
				UClass* ObjectClassToUse = Context.GetFirstNativeOrConvertedClass(ObjectProperty->PropertyClass);
				const FString MappedObject = Context.FindGloballyMappedObject(Object, ObjectClassToUse);
				if (!MappedObject.IsEmpty())
				{
					return MappedObject;
				}
			}

			const bool bCreatingSubObjectsOfClass = (Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass);
			{
				auto BPGC = Context.GetCurrentlyGeneratedClass();
				auto CDO = BPGC ? BPGC->GetDefaultObject(false) : nullptr;
				if (BPGC && Object && CDO && Object->IsIn(BPGC) && !Object->IsIn(CDO) && bCreatingSubObjectsOfClass)
				{
					return HandleClassSubobject(Context, Object, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, true, true);
				}
			}

			if (!bCreatingSubObjectsOfClass && Property->HasAnyPropertyFlags(CPF_InstancedReference))
			{
				const FString CreateAsInstancedSubobject = HandleInstancedSubobject(Context, Object, Object->HasAnyFlags(RF_ArchetypeObject));
				if (!CreateAsInstancedSubobject.IsEmpty())
				{
					return CreateAsInstancedSubobject;
				}
			}
		}
		else if (ObjectProperty->HasMetaData(FBlueprintMetadata::MD_LatentCallbackTarget))
		{
			return TEXT("this");
		}
	}

	if (auto StructProperty = Cast<UStructProperty>(Property))
	{
		FString StructConstructor;
		if (SpecialStructureConstructor(StructProperty->Struct, ValuePtr, &StructConstructor))
		{
			return StructConstructor;
		}
	}

	return FString();
}

struct FNonativeComponentData
{
	FString NativeVariablePropertyName;
	UActorComponent* ComponentTemplate;
	UObject* ObjectToCompare;

	////
	FString ParentVariableName;
	bool bSetNativeCreationMethod;
	/** Socket/Bone that Component might attach to */
	FName AttachToName;
	bool bIsRoot;

	FNonativeComponentData()
		: ComponentTemplate(nullptr)
		, ObjectToCompare(nullptr)
		, bSetNativeCreationMethod(false)
		, bIsRoot(false)
	{
	}

	bool HandledAsSpecialProperty(FEmitterLocalContext& Context, const UProperty* Property)
	{
		// skip relative location and rotation. THey are ignored for root components created from scs (and they probably should be reset by scs editor).
		if (bIsRoot && (Property->GetOuter() == USceneComponent::StaticClass()))
		{
			UProperty* RelativeLocationProperty = USceneComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation));
			UProperty* RelativeRotationProperty = USceneComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation));
			if ((Property == RelativeLocationProperty) || (Property == RelativeRotationProperty))
			{
				return true;
			}
		}

		return false;
	}

	void EmitProperties(FEmitterLocalContext& Context)
	{
		ensure(!NativeVariablePropertyName.IsEmpty());
		if (bSetNativeCreationMethod)
		{
			Context.AddLine(FString::Printf(TEXT("%s->%s = EComponentCreationMethod::Native;"), *NativeVariablePropertyName, GET_MEMBER_NAME_STRING_CHECKED(UActorComponent, CreationMethod)));
		}

		if (!ParentVariableName.IsEmpty())
		{
			const FString SocketName = (AttachToName == NAME_None) ? FString() : FString::Printf(TEXT(", TEXT(\"%s\")"), *AttachToName.ToString());
			Context.AddLine(FString::Printf(TEXT("%s->%s(%s, FAttachmentTransformRules::KeepRelativeTransform %s);")
				, *NativeVariablePropertyName
				, GET_FUNCTION_NAME_STRING_CHECKED(USceneComponent, AttachToComponent)
				, *ParentVariableName, *SocketName));
			// AttachTo is called first in case some properties will be overridden.
		}

		bool bBodyInstanceIsAlreadyHandled = false;
		UProperty* BodyInstanceProperty = UPrimitiveComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, BodyInstance));
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ComponentTemplate);
		if(PrimitiveComponent)
		{
			const FName CollisionProfileName = PrimitiveComponent->BodyInstance.GetCollisionProfileName();
			UPrimitiveComponent* ComponentArchetype = Cast<UPrimitiveComponent>(ObjectToCompare);
			const FName ComponentArchetypeCollisionProfileName = ComponentArchetype ? ComponentArchetype->BodyInstance.GetCollisionProfileName() : NAME_None;
			if (CollisionProfileName != ComponentArchetypeCollisionProfileName)
			{
				FStructOnScope BodyInstanceToCompare(FBodyInstance::StaticStruct());
				if (ComponentArchetype)
				{
					FBodyInstance::StaticStruct()->CopyScriptStruct(BodyInstanceToCompare.GetStructMemory(), &ComponentArchetype->BodyInstance);
				}
				((FBodyInstance*)BodyInstanceToCompare.GetStructMemory())->SetCollisionProfileName(CollisionProfileName);

				const FString PathToMember = FString::Printf(TEXT("%s->BodyInstance"), *NativeVariablePropertyName);
				Context.AddLine(FString::Printf(TEXT("%s.SetCollisionProfileName(FName(TEXT(\"%s\")));"), *PathToMember, *CollisionProfileName.ToString().ReplaceCharWithEscapedChar()));
				FEmitDefaultValueHelper::InnerGenerate(Context, BodyInstanceProperty, PathToMember, (const uint8*)&PrimitiveComponent->BodyInstance, BodyInstanceToCompare.GetStructMemory());
				bBodyInstanceIsAlreadyHandled = true;
			}
		}

		UClass* ComponentClass = ComponentTemplate->GetClass();
		for (auto Property : TFieldRange<const UProperty>(ComponentClass))
		{
			if (bBodyInstanceIsAlreadyHandled && (Property == BodyInstanceProperty))
			{
				continue;
			}
			if (HandledAsSpecialProperty(Context, Property))
			{
				continue;
			}
			FEmitDefaultValueHelper::OuterGenerate(Context, Property, NativeVariablePropertyName
				, reinterpret_cast<const uint8*>(ComponentTemplate)
				, reinterpret_cast<const uint8*>(ObjectToCompare)
				, FEmitDefaultValueHelper::EPropertyAccessOperator::Pointer);
		}
	}

	void EmitForcedPostLoad(FEmitterLocalContext& Context)
	{
		Context.AddLine(FString::Printf(TEXT("if(%s && !%s->%s())"), *NativeVariablePropertyName, *NativeVariablePropertyName, GET_FUNCTION_NAME_STRING_CHECKED(UActorComponent, IsTemplate)));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("%s->%s(RF_NeedPostLoad |RF_NeedPostLoadSubobjects);"), *NativeVariablePropertyName, GET_FUNCTION_NAME_STRING_CHECKED(UActorComponent, SetFlags)));
		Context.AddLine(FString::Printf(TEXT("%s->%s();"), *NativeVariablePropertyName, GET_FUNCTION_NAME_STRING_CHECKED(UActorComponent, ConditionalPostLoad)));
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
};

FString FEmitDefaultValueHelper::HandleNonNativeComponent(FEmitterLocalContext& Context, const USCS_Node* Node, TSet<const UProperty*>& OutHandledProperties, TArray<FString>& NativeCreatedComponentProperties, const USCS_Node* ParentNode, TArray<FNonativeComponentData>& ComponenntsToInit)
{
	check(Node);
	check(Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::CommonConstructor);

	FString NativeVariablePropertyName;
	UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	if (UActorComponent* ComponentTemplate = Node->GetActualComponentTemplate(BPGC))
	{
		const FString VariableCleanName = Node->VariableName.ToString();

		const UObjectProperty* VariableProperty = FindField<UObjectProperty>(BPGC, *VariableCleanName);
		if (VariableProperty)
		{
			NativeVariablePropertyName = FEmitHelper::GetCppName(VariableProperty);
			OutHandledProperties.Add(VariableProperty);
		}
		else
		{
			NativeVariablePropertyName = VariableCleanName;
		}

		Context.AddCommonSubObject_InConstructor(ComponentTemplate, NativeVariablePropertyName);

		if (ComponentTemplate->GetOuter() == BPGC)
		{
			FNonativeComponentData NonativeComponentData;
			NonativeComponentData.NativeVariablePropertyName = NativeVariablePropertyName;
			NonativeComponentData.ComponentTemplate = ComponentTemplate;
			USCS_Node* RootComponentNode = nullptr;
			Node->GetSCS()->GetSceneRootComponentTemplate(&RootComponentNode);
			NonativeComponentData.bIsRoot = RootComponentNode == Node;
			UClass* ComponentClass = ComponentTemplate->GetClass();
			check(ComponentClass != nullptr);

			UObject* ObjectToCompare = ComponentClass->GetDefaultObject(false);

			if (ComponentTemplate->HasAnyFlags(RF_InheritableComponentTemplate))
			{
				ObjectToCompare = Node->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass()));
			}
			else
			{
				Context.AddLine(FString::Printf(TEXT("%s%s = CreateDefaultSubobject<%s>(TEXT(\"%s\"));")
					, (VariableProperty == nullptr) ? TEXT("auto ") : TEXT("")
					, *NativeVariablePropertyName
					, *FEmitHelper::GetCppName(ComponentClass)
					, *VariableCleanName));

				NonativeComponentData.bSetNativeCreationMethod = true;
				NativeCreatedComponentProperties.Add(NativeVariablePropertyName);

				FString ParentVariableName;
				if (ParentNode)
				{
					const FString CleanParentVariableName = ParentNode->VariableName.ToString();
					const UObjectProperty* ParentVariableProperty = FindField<UObjectProperty>(BPGC, *CleanParentVariableName);
					ParentVariableName = ParentVariableProperty ? FEmitHelper::GetCppName(ParentVariableProperty) : CleanParentVariableName;
				}
				else if (USceneComponent* ParentComponentTemplate = Node->GetParentComponentTemplate(CastChecked<UBlueprint>(BPGC->ClassGeneratedBy)))
				{
					ParentVariableName = Context.FindGloballyMappedObject(ParentComponentTemplate, USceneComponent::StaticClass());
				}
				NonativeComponentData.ParentVariableName = ParentVariableName;
				NonativeComponentData.AttachToName = Node->AttachToName;
			}
			NonativeComponentData.ObjectToCompare = ObjectToCompare;
			ComponenntsToInit.Add(NonativeComponentData);
		}
	}

	// Recursively handle child nodes.
	for (auto ChildNode : Node->ChildNodes)
	{
		HandleNonNativeComponent(Context, ChildNode, OutHandledProperties, NativeCreatedComponentProperties, Node, ComponenntsToInit);
	}

	return NativeVariablePropertyName;
}

struct FDependenciesHelper
{
public:
	// Keep sync with FTypeSingletonCache::GenerateSingletonName
	static FString GenerateZConstructor(UField* Item)
	{
		FString Result;
		if (!ensure(Item))
		{
			return Result;
		}

		for (UObject* Outer = Item; Outer; Outer = Outer->GetOuter())
		{
			if (!Result.IsEmpty())
			{
				Result = TEXT("_") + Result;
			}

			if (Cast<UClass>(Outer) || Cast<UScriptStruct>(Outer))
			{
				FString OuterName = FEmitHelper::GetCppName(CastChecked<UField>(Outer), true);
				Result = OuterName + Result;

				// Structs can also have UPackage outer.
				if (Cast<UClass>(Outer) || Cast<UPackage>(Outer->GetOuter()))
				{
					break;
				}
			}
			else
			{
				Result = Outer->GetName() + Result;
			}
		}

		// Can't use long package names in function names.
		if (Result.StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive))
		{
			Result = FPackageName::GetShortName(Result);
		}

		const FString ClassString = Item->IsA<UClass>() ? TEXT("UClass") : TEXT("UScriptStruct");
		return FString(TEXT("Z_Construct_")) + ClassString + TEXT("_") + Result + TEXT("()");
	}
};

void FEmitDefaultValueHelper::AddStaticFunctionsForDependencies(FEmitterLocalContext& Context, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies)
{


	// 0.ALL ASSETS TO LIST
	TSet<const UObject*> StaticDependenciesAssets;
	{
		for (UObject* LocAsset : Context.Dependencies.Assets)
		{
			if (ParentDependencies.IsValid() && ParentDependencies->Assets.Contains(LocAsset))
			{
				continue;
			}
			StaticDependenciesAssets.Add(LocAsset);
		}
		for (UBlueprintGeneratedClass* LocAsset : Context.Dependencies.ConvertedClasses)
		{
			if (ParentDependencies.IsValid() && (ParentDependencies->ConvertedClasses.Contains(LocAsset) || ParentDependencies->Assets.Contains(LocAsset)))
			{
				continue;
			}
			StaticDependenciesAssets.Add(LocAsset);
		}
	}

	// 1. USED ASSETS LIST
	auto SourceClass = Context.GetCurrentlyGeneratedClass();
	auto OriginalClass = Context.Dependencies.FindOriginalClass(SourceClass);
	const FString CppClassName = FEmitHelper::GetCppName(OriginalClass);

	auto CreateAssetToLoadString = [&](const UObject* AssetObj, const FString& LongPathVariable) -> FString
	{
		UClass* AssetType = AssetObj->GetClass();
		if (AssetType->IsChildOf<UUserDefinedEnum>())
		{
			AssetType = UEnum::StaticClass();
		}
		else if (AssetType->IsChildOf<UUserDefinedStruct>())
		{
			AssetType = UScriptStruct::StaticClass();
		}
		else if (AssetType->IsChildOf<UBlueprintGeneratedClass>() && Context.Dependencies.WillClassBeConverted(CastChecked<UBlueprintGeneratedClass>(AssetObj)))
		{
			AssetType = UDynamicClass::StaticClass();
		}

		return FString::Printf(TEXT("FBlueprintDependencyData(%s, TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\")),")
			, *LongPathVariable
			, *FPackageName::GetShortName(AssetObj->GetOutermost()->GetPathName())
			, *AssetObj->GetName()
			, *AssetType->GetOutermost()->GetPathName()
			, *AssetType->GetName());
	};

	auto AddAssetArray = [&](TArray<const UObject*>& Assets)
	{
		TMap<FString, FString> PrefixToLocalVariable;
		for (const UObject* LocAsset : Assets)
		{
			FString LongPackagePath = FPackageName::GetLongPackagePath(LocAsset->GetOutermost()->GetPathName());
			if (!PrefixToLocalVariable.Find(LongPackagePath))
			{
				FString VariableName = Context.GenerateUniqueLocalName();
				Context.AddLine(FString::Printf(TEXT("const TCHAR* %s = TEXT(\"%s\");"), *VariableName, *LongPackagePath));
				PrefixToLocalVariable.Add(LongPackagePath, VariableName);
			}
		}

		if (Assets.Num())
		{
			Context.AddLine(TEXT("FBlueprintDependencyData LocAssets[] ="));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();
		}

		for (const UObject* LocAsset : Assets)
		{
			FString* LongPathVar = PrefixToLocalVariable.Find(FPackageName::GetLongPackagePath(LocAsset->GetOutermost()->GetPathName()));
			check(LongPathVar);
			Context.AddLine(CreateAssetToLoadString(LocAsset, *LongPathVar));
		}

		if (Assets.Num())
		{
			Context.DecreaseIndent();
			Context.AddLine(TEXT("};"));
			Context.AddLine(TEXT("for(auto& LocAsset : LocAssets) { AssetsToLoad.Add(LocAsset); } "));
		}
	};

	int32 UsedAssetIndex = 0;
	// 1. COMMON DEPENDENCIES ASSETS - USED BOTH BY __StaticDependenciesAssets and __StaticDependencies_DirectlyUsedAssets
	{
		Context.AddLine(FString::Printf(TEXT("void %s::__StaticDependencies_CommonAssets(TArray<FBlueprintDependencyData>& AssetsToLoad)"), *CppClassName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();

		TArray<const UObject*> AssetsToAdd;
		for (;UsedAssetIndex < Context.UsedObjectInCurrentClass.Num(); ++UsedAssetIndex)
		{
			const UObject* LocAsset = Context.UsedObjectInCurrentClass[UsedAssetIndex];
			if (!StaticDependenciesAssets.Contains(LocAsset))
			{
				break;
			}
			StaticDependenciesAssets.Remove(LocAsset);
			AssetsToAdd.Add(LocAsset);
		}
		AddAssetArray(AssetsToAdd);

		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}

	//
	{
		// __StaticDependencies_DirectlyUsedAssets
		Context.AddLine(FString::Printf(TEXT("void %s::__StaticDependencies_DirectlyUsedAssets(TArray<FBlueprintDependencyData>& AssetsToLoad)"), *CppClassName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();

		Context.AddLine(FString(TEXT("__StaticDependencies_CommonAssets(AssetsToLoad);")));

		TArray<const UObject*> AssetsToAdd;
		for (; UsedAssetIndex < Context.UsedObjectInCurrentClass.Num(); ++UsedAssetIndex)
		{
			const UObject* LocAsset = Context.UsedObjectInCurrentClass[UsedAssetIndex];
			ensure(!StaticDependenciesAssets.Contains(LocAsset));
			AssetsToAdd.Add(LocAsset);
		}
		AddAssetArray(AssetsToAdd);

		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}

	// 2. REMAINING DEPENDENCIES
	// __StaticDependenciesAssets
	{
		Context.AddLine(FString::Printf(TEXT("void %s::__StaticDependenciesAssets(TArray<FBlueprintDependencyData>& AssetsToLoad)"), *CppClassName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();

		UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(SourceClass->GetSuperClass());
		ensure(ParentDependencies.IsValid() == !!ParentBPGC);
		if (ParentBPGC)
		{
			Context.AddLine(FString::Printf(TEXT("%s::__StaticDependenciesAssets(AssetsToLoad);"), *FEmitHelper::GetCppName(ParentBPGC)));
		}
		Context.AddLine(FString(TEXT("__StaticDependencies_CommonAssets(AssetsToLoad);")));

		TArray<const UObject*> AssetsToAdd;
		for (const UObject* LocAsset : StaticDependenciesAssets)
		{
			AssetsToAdd.Add(LocAsset);
		}
		AddAssetArray(AssetsToAdd);

		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
}

void FEmitDefaultValueHelper::AddRegisterHelper(FEmitterLocalContext& Context)
{
	auto SourceClass = Context.GetCurrentlyGeneratedClass();
	auto OriginalClass = Context.Dependencies.FindOriginalClass(SourceClass);
	const FString CppClassName = FEmitHelper::GetCppName(OriginalClass);

	const FString RegisterHelperName = FString::Printf(TEXT("FRegisterHelper__%s"), *CppClassName);
	Context.AddLine(FString::Printf(TEXT("struct %s"), *RegisterHelperName));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();

	Context.AddLine(FString::Printf(TEXT("%s()"), *RegisterHelperName));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();

	Context.AddLine(FString::Printf(
		TEXT("FConvertedBlueprintsDependencies::Get().RegisterClass(TEXT(\"%s\"), &%s::__StaticDependenciesAssets);")
		, *OriginalClass->GetOutermost()->GetPathName()
		, *CppClassName));

	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));

	Context.AddLine(FString::Printf(TEXT("static %s Instance;"), *RegisterHelperName));

	Context.DecreaseIndent();
	Context.AddLine(TEXT("};"));

	Context.AddLine(FString::Printf(TEXT("%s %s::Instance;"), *RegisterHelperName, *RegisterHelperName));
}


void FEmitDefaultValueHelper::FillCommonUsedAssets(FEmitterLocalContext& Context, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies)
{
	ensure(0 == Context.UsedObjectInCurrentClass.Num());
	for (auto LocAsset : Context.Dependencies.Assets)
	{
		if (!ParentDependencies.IsValid() || !ParentDependencies->Assets.Contains(LocAsset))
		{
			Context.UsedObjectInCurrentClass.Add(LocAsset);
		}
	}
}

void FEmitDefaultValueHelper::GenerateCustomDynamicClassInitialization(FEmitterLocalContext& Context, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies)
{
	auto BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	const FString CppClassName = FEmitHelper::GetCppName(BPGC);

	Context.AddLine(FString::Printf(TEXT("void %s::__CustomDynamicClassInitialization(UDynamicClass* InDynamicClass)"), *CppClassName));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();
	Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields)));
	Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, MiscConvertedSubobjects)));
	Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, DynamicBindingObjects)));
	Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ComponentTemplates)));
	Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, Timelines)));
	Context.AddLine(FString::Printf(TEXT("ensure(nullptr == InDynamicClass->%s);"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, AnimClassImplementation)));
	Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s();"), GET_FUNCTION_NAME_STRING_CHECKED(UDynamicClass, AssembleReferenceTokenStream)));

	Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass;
	Context.ResetPropertiesForInaccessibleStructs();

	if (Context.Dependencies.ConvertedEnum.Num())
	{
		Context.AddLine(TEXT("// List of all referenced converted enums"));
	}
	for (auto LocEnum : Context.Dependencies.ConvertedEnum)
	{
		Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(LoadObject<UEnum>(nullptr, TEXT(\"%s\")));"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields), *(LocEnum->GetPathName().ReplaceCharWithEscapedChar())));
		Context.EnumsInCurrentClass.Add(LocEnum);
	}

	if (Context.Dependencies.ConvertedClasses.Num())
	{
		Context.AddLine(TEXT("// List of all referenced converted classes"));
	}
	for (auto LocStruct : Context.Dependencies.ConvertedClasses)
	{
		UClass* ClassToLoad = Context.Dependencies.FindOriginalClass(LocStruct);
		if (ensure(ClassToLoad))
		{
			if (ParentDependencies.IsValid() && ParentDependencies->ConvertedClasses.Contains(LocStruct))
			{
				continue;
			}

			const FString ClassConstructor = FDependenciesHelper::GenerateZConstructor(ClassToLoad);
			Context.AddLine(FString::Printf(TEXT("extern UClass* %s;"), *ClassConstructor));
			Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(%s);"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields), *ClassConstructor));

			//Context.AddLine(FString::Printf(TEXT("InDynamicClass->ReferencedConvertedFields.Add(LoadObject<UClass>(nullptr, TEXT(\"%s\")));")
			//	, *(ClassToLoad->GetPathName().ReplaceCharWithEscapedChar())));
		}
	}

	if (Context.Dependencies.ConvertedStructs.Num())
	{
		Context.AddLine(TEXT("// List of all referenced converted structures"));
	}
	for (auto LocStruct : Context.Dependencies.ConvertedStructs)
	{
		if (ParentDependencies.IsValid() && ParentDependencies->ConvertedStructs.Contains(LocStruct))
		{
			continue;
		}
		const FString StructConstructor = FDependenciesHelper::GenerateZConstructor(LocStruct);
		Context.AddLine(FString::Printf(TEXT("extern UScriptStruct* %s;"), *StructConstructor));
		Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(%s);"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields), *StructConstructor));
	}

	TArray<UActorComponent*> ActorComponentTempatesOwnedByClass = BPGC->ComponentTemplates;
	// Gather all CT from SCS and IH, the remaining ones are generated for class..
	if (auto SCS = BPGC->SimpleConstructionScript)
	{
		for (auto Node : SCS->GetAllNodes())
		{
			ActorComponentTempatesOwnedByClass.RemoveSwap(Node->ComponentTemplate);
		}
	}
	if (auto IH = BPGC->GetInheritableComponentHandler())
	{
		TArray<UActorComponent*> AllTemplates;
		IH->GetAllTemplates(AllTemplates);
		ActorComponentTempatesOwnedByClass.RemoveAllSwap([&](UActorComponent* Component) -> bool
		{
			return AllTemplates.Contains(Component);
		});
	}

	Context.AddLine(TEXT("FConvertedBlueprintsDependencies::FillUsedAssetsInDynamicClass(InDynamicClass, &__StaticDependencies_DirectlyUsedAssets);"));

	auto CreateAndInitializeClassSubobjects = [&](bool bCreate, bool bInitilize)
	{
		for (auto ComponentTemplate : ActorComponentTempatesOwnedByClass)
		{
			if (ComponentTemplate)
			{
				HandleClassSubobject(Context, ComponentTemplate, FEmitterLocalContext::EClassSubobjectList::ComponentTemplates, bCreate, bInitilize);
			}
		}

		for (auto TimelineTemplate : BPGC->Timelines)
		{
			if (TimelineTemplate)
			{
				HandleClassSubobject(Context, TimelineTemplate, FEmitterLocalContext::EClassSubobjectList::Timelines, bCreate, bInitilize);
			}
		}

		for (auto DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (DynamicBindingObject)
			{
				HandleClassSubobject(Context, DynamicBindingObject, FEmitterLocalContext::EClassSubobjectList::DynamicBindingObjects, bCreate, bInitilize);
			}
		}
		FBackendHelperUMG::CreateClassSubobjects(Context, bCreate, bInitilize);
	};
	CreateAndInitializeClassSubobjects(true, false);
	CreateAndInitializeClassSubobjects(false, true);

	FBackendHelperAnim::CreateAnimClassData(Context);

	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));

	Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::Regular;
	Context.ResetPropertiesForInaccessibleStructs();

	FBackendHelperUMG::EmitWidgetInitializationFunctions(Context);
}

void FEmitDefaultValueHelper::GenerateConstructor(FEmitterLocalContext& Context)
{
	UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	const FString CppClassName = FEmitHelper::GetCppName(BPGC);

	UClass* SuperClass = BPGC->GetSuperClass();
	const bool bSuperHasObjectInitializerConstructor = SuperClass && SuperClass->HasMetaData(TEXT("ObjectInitializerConstructorDeclared"));

	Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::CommonConstructor;
	Context.ResetPropertiesForInaccessibleStructs();
	Context.AddLine(FString::Printf(TEXT("%s::%s(const FObjectInitializer& ObjectInitializer) : Super(%s)")
		, *CppClassName
		, *CppClassName
		, bSuperHasObjectInitializerConstructor ? TEXT("ObjectInitializer") : TEXT("")));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();

	// Call CustomDynamicClassInitialization
	Context.AddLine(FString::Printf(TEXT("if(HasAnyFlags(RF_ClassDefaultObject) && (%s::StaticClass() == GetClass()))"), *CppClassName));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();
	Context.AddLine(FString::Printf(TEXT("%s::__CustomDynamicClassInitialization(CastChecked<UDynamicClass>(GetClass()));"), *CppClassName));
	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));

	// Components that must be fixed after serialization
	TArray<FString> NativeCreatedComponentProperties;
	TArray<FNonativeComponentData> ComponentsToInit;
	{
		UObject* CDO = BPGC->GetDefaultObject(false);

		UObject* ParentCDO = BPGC->GetSuperClass()->GetDefaultObject(false);
		check(CDO && ParentCDO);
		Context.AddLine(TEXT(""));

		FString NativeRootComponentFallback;
		TSet<const UProperty*> HandledProperties;

		// Generate ctor init code for native class default subobjects that are always instanced (e.g. components).
		// @TODO (pkavan) - We can probably make this faster by generating code to index through the DSO array instead (i.e. in place of HandleInstancedSubobject which will generate a lookup call per DSO).
		TArray<UObject*> NativeDefaultObjectSubobjects;
		BPGC->GetDefaultObjectSubobjects(NativeDefaultObjectSubobjects);
		for (auto DSO : NativeDefaultObjectSubobjects)
		{
			if (DSO && DSO->GetClass()->HasAnyClassFlags(CLASS_DefaultToInstanced))
			{
				// Determine if this is an editor-only subobject.
				bool bIsEditorOnlySubobject = false;
				if (const UActorComponent* ActorComponent = Cast<UActorComponent>(DSO))
				{
					bIsEditorOnlySubobject = ActorComponent->IsEditorOnly();
				}

				// Skip ctor code gen for editor-only subobjects, since they won't be used by the runtime. Any dependencies on editor-only subobjects will be handled later (see HandleInstancedSubobject).
				if (!bIsEditorOnlySubobject)
				{
					const FString VariableName = HandleInstancedSubobject(Context, DSO, false, true);

					// Keep track of which component can be used as a root, in case it's not explicitly set.
					if (NativeRootComponentFallback.IsEmpty())
					{
						USceneComponent* SceneComponent = Cast<USceneComponent>(DSO);
						if (SceneComponent && !SceneComponent->GetAttachParent() && SceneComponent->CreationMethod == EComponentCreationMethod::Native)
						{
							NativeRootComponentFallback = VariableName;
						}
					}
				}
			}
		}

		// Check for a valid RootComponent property value; mark it as handled if already set in the defaults.
		bool bNeedsRootComponentAssignment = false;
		static const FName RootComponentPropertyName(TEXT("RootComponent"));
		const UObjectProperty* RootComponentProperty = FindField<UObjectProperty>(BPGC, RootComponentPropertyName);
		if (RootComponentProperty)
		{
			if (RootComponentProperty->GetObjectPropertyValue_InContainer(CDO))
			{
				HandledProperties.Add(RootComponentProperty);
			}
			else if (!NativeRootComponentFallback.IsEmpty())
			{
				Context.AddLine(FString::Printf(TEXT("RootComponent = %s;"), *NativeRootComponentFallback));
				HandledProperties.Add(RootComponentProperty);
			}
			else
			{
				bNeedsRootComponentAssignment = true;
			}
		}

		// Generate ctor init code for the SCS node hierarchy (i.e. non-native components). SCS nodes may have dependencies on native DSOs, but not vice-versa.
		TArray<const UBlueprintGeneratedClass*> BPGCStack;
		const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(BPGC, BPGCStack);
		if (bErrorFree)
		{
			// Start at the base of the hierarchy so that dependencies are handled first.
			for (int32 i = BPGCStack.Num() - 1; i >= 0; --i)
			{
				if (BPGCStack[i]->SimpleConstructionScript)
				{
					for (auto Node : BPGCStack[i]->SimpleConstructionScript->GetRootNodes())
					{
						if (Node)
						{
							const FString NativeVariablePropertyName = HandleNonNativeComponent(Context, Node, HandledProperties, NativeCreatedComponentProperties, nullptr, ComponentsToInit);

							if (bNeedsRootComponentAssignment && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>() && !NativeVariablePropertyName.IsEmpty())
							{
								// Only emit the explicit root component assignment statement if we're looking at the child BPGC that we're generating ctor code
								// for. In all other cases, the root component will already be set up by a chained parent ctor call, so we avoid stomping it here.
								if (i == 0)
								{
									Context.AddLine(FString::Printf(TEXT("RootComponent = %s;"), *NativeVariablePropertyName));
									HandledProperties.Add(RootComponentProperty);
								}

								bNeedsRootComponentAssignment = false;
							}
						}
					}
				}
			}

			for (auto& ComponentToInit : ComponentsToInit)
			{
				ComponentToInit.EmitProperties(Context);

				if (Cast<UPrimitiveComponent>(ComponentToInit.ComponentTemplate))
				{
					Context.AddLine(FString::Printf(TEXT("if(!%s->%s())"), *ComponentToInit.NativeVariablePropertyName, GET_FUNCTION_NAME_STRING_CHECKED(UPrimitiveComponent, IsTemplate)));
					Context.AddLine(TEXT("{"));
					Context.IncreaseIndent();
					Context.AddLine(FString::Printf(TEXT("%s->%s.%s(%s);")
						, *ComponentToInit.NativeVariablePropertyName
						, GET_MEMBER_NAME_STRING_CHECKED(UPrimitiveComponent, BodyInstance)
						, GET_FUNCTION_NAME_STRING_CHECKED(FBodyInstance, FixupData)
						, *ComponentToInit.NativeVariablePropertyName));
					Context.DecreaseIndent();
					Context.AddLine(TEXT("}"));
				}
			}
		}

		// Generate ctor init code for generated Blueprint class property values that may differ from parent class defaults (or that otherwise belong to the generated Blueprint class).
		for (auto Property : TFieldRange<const UProperty>(BPGC))
		{
			if (!HandledProperties.Contains(Property))
			{
				const bool bNewProperty = Property->GetOwnerStruct() == BPGC;
				OuterGenerate(Context, Property, TEXT(""), reinterpret_cast<const uint8*>(CDO), bNewProperty ? nullptr : reinterpret_cast<const uint8*>(ParentCDO), EPropertyAccessOperator::None, true);
			}
		}
	}
	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));

	// TODO: this mechanism could be required by other instanced subobjects.
	Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::Regular;
	Context.ResetPropertiesForInaccessibleStructs();

	Context.ResetPropertiesForInaccessibleStructs();
	Context.AddLine(FString::Printf(TEXT("void %s::%s(FObjectInstancingGraph* OuterInstanceGraph)"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UObject, PostLoadSubobjects)));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();
	Context.AddLine(FString::Printf(TEXT("Super::%s(OuterInstanceGraph);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, PostLoadSubobjects)));
	for (auto& ComponentToFix : NativeCreatedComponentProperties)
	{
		Context.AddLine(FString::Printf(TEXT("if(%s)"), *ComponentToFix));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("%s->%s = EComponentCreationMethod::Native;"), *ComponentToFix, GET_MEMBER_NAME_STRING_CHECKED(UActorComponent, CreationMethod)));
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));
}

FString FEmitDefaultValueHelper::HandleClassSubobject(FEmitterLocalContext& Context, UObject* Object, FEmitterLocalContext::EClassSubobjectList ListOfSubobjectsType, bool bCreate, bool bInitilize)
{
	ensure(Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass);

	FString LocalNativeName;
	if (bCreate)
	{
		FString OuterStr = Context.FindGloballyMappedObject(Object->GetOuter());
		if (OuterStr.IsEmpty())
		{
			OuterStr = HandleClassSubobject(Context, Object->GetOuter(), ListOfSubobjectsType, bCreate, bInitilize);
			if (OuterStr.IsEmpty())
			{
				return FString();
			}
			const FString AlreadyCreatedObject = Context.FindGloballyMappedObject(Object);
			if (!AlreadyCreatedObject.IsEmpty())
			{
				return AlreadyCreatedObject;
			}
		}

		const bool bAddAsSubobjectOfClass = Object->GetOuter() == Context.GetCurrentlyGeneratedClass();
		LocalNativeName = Context.GenerateUniqueLocalName();
		Context.AddClassSubObject_InConstructor(Object, LocalNativeName);
		UClass* ObjectClass = Object->GetClass();
		const FString ActualClass = Context.FindGloballyMappedObject(ObjectClass, UClass::StaticClass());
		const FString NativeType = FEmitHelper::GetCppName(Context.GetFirstNativeOrConvertedClass(ObjectClass));
		Context.AddLine(FString::Printf(
			TEXT("auto %s = NewObject<%s>(%s, %s, TEXT(\"%s\"));")
			, *LocalNativeName
			, *NativeType
			, bAddAsSubobjectOfClass ? TEXT("InDynamicClass") : *OuterStr
			, *ActualClass
			, *Object->GetName().ReplaceCharWithEscapedChar()));
		if (bAddAsSubobjectOfClass)
		{
			Context.RegisterClassSubobject(Object, ListOfSubobjectsType);
			Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(%s);")
				, Context.ClassSubobjectListName(ListOfSubobjectsType)
				, *LocalNativeName));
		}
	}

	if (bInitilize)
	{
		if (LocalNativeName.IsEmpty())
		{
			LocalNativeName = Context.FindGloballyMappedObject(Object);
		}
		ensure(!LocalNativeName.IsEmpty());
		auto CDO = Object->GetClass()->GetDefaultObject(false);
		for (auto Property : TFieldRange<const UProperty>(Object->GetClass()))
		{
			OuterGenerate(Context, Property, LocalNativeName
				, reinterpret_cast<const uint8*>(Object)
				, reinterpret_cast<const uint8*>(CDO)
				, EPropertyAccessOperator::Pointer);
		}
	}
	return LocalNativeName;
}

FString FEmitDefaultValueHelper::HandleInstancedSubobject(FEmitterLocalContext& Context, UObject* Object, bool bCreateInstance, bool bSkipEditorOnlyCheck)
{
	check(Object);

	// Make sure we don't emit initialization code for the same object more than once.
	FString LocalNativeName = Context.FindGloballyMappedObject(Object);
	if (!LocalNativeName.IsEmpty())
	{
		return LocalNativeName;
	}
	else
	{
		LocalNativeName = Context.GenerateUniqueLocalName();
	}

	if (Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass)
	{
		Context.AddClassSubObject_InConstructor(Object, LocalNativeName);
	}
	else if (Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::CommonConstructor)
	{
		Context.AddCommonSubObject_InConstructor(Object, LocalNativeName);
	}

	UClass* ObjectClass = Object->GetClass();

	// Determine if this is an editor-only subobject. When handling as a dependency, we'll create a "dummy" object in its place (below).
	bool bIsEditorOnlySubobject = false;
	if (!bSkipEditorOnlyCheck)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
		{
			bIsEditorOnlySubobject = ActorComponent->IsEditorOnly();
			if (bIsEditorOnlySubobject)
			{
				// Replace the potentially editor-only class with a base actor/scene component class that's available to the runtime. We'll create a "dummy" object of this type to stand in for the editor-only subobject below.
				ObjectClass = ObjectClass->IsChildOf<USceneComponent>() ? USceneComponent::StaticClass() : UActorComponent::StaticClass();
			}
		}
	}

	auto BPGC = Context.GetCurrentlyGeneratedClass();
	auto CDO = BPGC ? BPGC->GetDefaultObject(false) : nullptr;
	if (!bIsEditorOnlySubobject && ensure(CDO) && (CDO == Object->GetOuter()))
	{
		if (bCreateInstance)
		{
			Context.AddLine(FString::Printf(TEXT("auto %s = CreateDefaultSubobject<%s>(TEXT(\"%s\"));")
				, *LocalNativeName, *FEmitHelper::GetCppName(ObjectClass), *Object->GetName()));
		}
		else
		{
			Context.AddLine(FString::Printf(TEXT("auto %s = CastChecked<%s>(%s(TEXT(\"%s\")));")
				, *LocalNativeName
				, *FEmitHelper::GetCppName(ObjectClass)
				, GET_FUNCTION_NAME_STRING_CHECKED(UObject, GetDefaultSubobjectByName)
				, *Object->GetName()));
		}

		const UObject* ObjectArchetype = Object->GetArchetype();
		for (auto Property : TFieldRange<const UProperty>(ObjectClass))
		{
			OuterGenerate(Context, Property, LocalNativeName
				, reinterpret_cast<const uint8*>(Object)
				, reinterpret_cast<const uint8*>(ObjectArchetype)
				, EPropertyAccessOperator::Pointer);
		}
	}
	else
	{
		const FString OuterStr = Context.FindGloballyMappedObject(Object);
		if (OuterStr.IsEmpty())
		{
			ensure(false);
			return FString();
		}

		const FString ActualClass = Context.FindGloballyMappedObject(ObjectClass, UClass::StaticClass());
		const FString NativeType = FEmitHelper::GetCppName(Context.GetFirstNativeOrConvertedClass(ObjectClass));
		Context.AddLine(FString::Printf(
			TEXT("auto %s = NewObject<%s>(%s, %s, TEXT(\"%s\"));")
			, *LocalNativeName
			, *NativeType
			, *OuterStr
			, *ActualClass
			, *Object->GetName().ReplaceCharWithEscapedChar()));
	}

	return LocalNativeName;
}

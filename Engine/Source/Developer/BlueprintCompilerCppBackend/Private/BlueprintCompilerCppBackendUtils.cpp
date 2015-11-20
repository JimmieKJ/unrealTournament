// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "EdGraphSchema_K2.h"
#include "IBlueprintCompilerCppBackendModule.h" // for OnPCHFilenameQuery()

FString FEmitterLocalContext::GenerateUniqueLocalName()
{
	const FString UniqueNameBase = TEXT("__Local__");
	const FString UniqueName = FString::Printf(TEXT("%s%d"), *UniqueNameBase, LocalNameIndexMax);
	++LocalNameIndexMax;
	return UniqueName;
}

FString FEmitterLocalContext::FindGloballyMappedObject(const UObject* Object, const UClass* ExpectedClass, bool bLoadIfNotFound, bool bTryUsedAssetsList)
{
	UClass* ActualClass = Cast<UClass>(Dependencies.GetOriginalStruct());
	if (ActualClass && Object && Object->IsIn(ActualClass))
	{
		if (const FString* NamePtr = (CurrentCodeType == EGeneratedCodeType::SubobjectsOfClass) ? ClassSubobjectsMap.Find(Object) : nullptr)
		{
			return *NamePtr;
		}

		if (const FString* NamePtr = (CurrentCodeType == EGeneratedCodeType::CommonConstructor) ? CommonSubobjectsMap.Find(Object) : nullptr)
		{
			return *NamePtr;
		}

		const UClass* ObjectClassToUse = ExpectedClass ? ExpectedClass : GetFirstNativeOrConvertedClass(Object->GetClass());
		const FString ClassString = FEmitHelper::GetCppName(ObjectClassToUse);

		int32 ObjectsCreatedPerClassIdx = MiscConvertedSubobjects.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->MiscConvertedSubobjects[%d])")
				, *ClassString, *FEmitHelper::GetCppName(ActualClass), ObjectsCreatedPerClassIdx);
		}

		ObjectsCreatedPerClassIdx = ObjectsCreatedPerClassIdx = DynamicBindingObjects.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->DynamicBindingObjects[%d])")
				, *ClassString, *FEmitHelper::GetCppName(ActualClass), ObjectsCreatedPerClassIdx);
		}

		ObjectsCreatedPerClassIdx = ComponentTemplates.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->ComponentTemplates[%d])")
				, *ClassString, *FEmitHelper::GetCppName(ActualClass), ObjectsCreatedPerClassIdx);
		}

		ObjectsCreatedPerClassIdx = Timelines.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->Timelines[%d])")
				, *ClassString, *FEmitHelper::GetCppName(ActualClass), ObjectsCreatedPerClassIdx);
		}

		if ((CurrentCodeType == EGeneratedCodeType::SubobjectsOfClass) || (CurrentCodeType == EGeneratedCodeType::CommonConstructor))
		{
			if (Object == ActualClass->GetDefaultObject(false))
			{
				return TEXT("this");
			}
		}
	}

	if (ActualClass && (Object == ActualClass))
	{
		return TEXT("GetClass()");
	}

	if (auto ObjClass = Cast<UClass>(Object))
	{
		auto BPGC = Cast<UBlueprintGeneratedClass>(ObjClass);
		if (ObjClass->HasAnyClassFlags(CLASS_Native) || (BPGC && Dependencies.WillClassBeConverted(BPGC)))
		{
			return FString::Printf(TEXT("%s::StaticClass()"), *FEmitHelper::GetCppName(ObjClass, true));
		}
	}

	// TODO Handle native structires, and special cases..

	if (auto UDS = Cast<UScriptStruct>(Object))
	{
		// Check if  
		// TODO: check if supported 
		return FString::Printf(TEXT("%s::StaticStruct()"), *FEmitHelper::GetCppName(UDS));
	}

	if (auto UDE = Cast<UEnum>(Object))
	{
		// TODO:
		// TODO: check if supported 
		//return FString::Printf(TEXT("%s_StaticEnum()"), *UDE->GetName());
		return FString::Printf(TEXT("FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT(\"%s\"))"), *FEmitHelper::GetCppName(UDE));
	}

	// TODO: handle subobjects
	ensure(!bLoadIfNotFound || Object);
	if (Object && (bLoadIfNotFound || bTryUsedAssetsList))
	{
		const UClass* ObjectClassToUse = ExpectedClass ? ExpectedClass : GetFirstNativeOrConvertedClass(Object->GetClass());
		const FString ClassString = FEmitHelper::GetCppName(ObjectClassToUse);

		if (bTryUsedAssetsList)
		{
			const int32 AssetIndex = Dependencies.Assets.IndexOfByKey(Object);
			if (INDEX_NONE != AssetIndex)
			{
				return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->UsedAssets[%d])")
					, *ClassString
					, *FEmitHelper::GetCppName(ActualClass)
					, AssetIndex);
			}
		}

		if (bLoadIfNotFound)
		{
			return FString::Printf(TEXT("LoadObject<%s>(nullptr, TEXT(\"%s\"))")
				, *ClassString
				, *(Object->GetPathName().ReplaceCharWithEscapedChar()));
		}
	}

	return FString{};
}

FString FEmitterLocalContext::ExportTextItem(const UProperty* Property, const void* PropertyValue) const
{
	if (auto ArrayProperty = Cast<const UArrayProperty>(Property))
	{
		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
		const FString TypeText = ExportCppDeclaration(ArrayProperty, EExportedDeclaration::Parameter, LocalExportCPPFlags, true);
		return FString::Printf(TEXT("%s()"), *TypeText);
	}

	auto ObjectPropertyBase = Cast<const UObjectPropertyBase>(Property);
	if (ObjectPropertyBase && Property->IsA<UAssetObjectProperty>())
	{
		if (UObject* Object = ObjectPropertyBase->GetObjectPropertyValue(PropertyValue))
		{
			const UClass* ActualClass = GetFirstNativeOrConvertedClass(ObjectPropertyBase->PropertyClass);
			return FString::Printf(TEXT("LoadObject<%s>(nullptr, TEXT(\"%s\"))")
				, *FEmitHelper::GetCppName(ActualClass)
				, *(Object->GetPathName().ReplaceCharWithEscapedChar()));
		}
		return TEXT("nullptr");
	}

	FString ValueStr;
	Property->ExportTextItem(ValueStr, PropertyValue, PropertyValue, nullptr, EPropertyPortFlags::PPF_ExportCpp);
	return ValueStr;
}

FString FEmitterLocalContext::ExportCppDeclaration(const UProperty* Property, EExportedDeclaration::Type DeclarationType, uint32 InExportCPPFlags, bool bSkipParameterName) const
{
	FString ActualCppType;
	FString* ActualCppTypePtr = nullptr;
	FString ActualExtendedType;
	FString* ActualExtendedTypePtr = nullptr;
	uint32 ExportCPPFlags = InExportCPPFlags;

	auto GetActualNameCPP = [&](const UObjectPropertyBase* ObjectPropertyBase, UClass* InActualClass)
	{
		auto BPGC = Cast<UBlueprintGeneratedClass>(InActualClass);
		const bool bConverted = BPGC && Dependencies.WillClassBeConverted(BPGC);
		if (BPGC )
		{
			if (!bConverted)
			{
				const bool bIsParameter = (DeclarationType == EExportedDeclaration::Parameter) || (DeclarationType == EExportedDeclaration::MacroParameter);
				const uint32 LocalExportCPPFlags = ExportCPPFlags | (bIsParameter ? CPPF_ArgumentOrReturnValue : 0);
				UClass* NativeType = GetFirstNativeOrConvertedClass(BPGC);
				ActualCppType = ObjectPropertyBase->GetCPPTypeCustom(&ActualExtendedType, LocalExportCPPFlags, NativeType);
				ActualCppTypePtr = &ActualCppType;
				if (!ActualExtendedType.IsEmpty())
				{
					ActualExtendedTypePtr = &ActualExtendedType;
				}
			}
			else
			{
				ActualCppType = FEmitHelper::GetCppName(ObjectPropertyBase->PropertyClass, false) + TEXT("*");
				ActualCppTypePtr = &ActualCppType;
			}
		}
	};

	auto ArrayProperty = Cast<const UArrayProperty>(Property);
	if (ArrayProperty)
	{
		Property = ArrayProperty->Inner;
		ExportCPPFlags = (ExportCPPFlags & ~CPPF_ArgumentOrReturnValue);
	}

	if (auto ClassProperty = Cast<const UClassProperty>(Property))
	{
		GetActualNameCPP(ClassProperty, ClassProperty->MetaClass);
	}
	else if (auto AssetClassProperty = Cast<const UAssetClassProperty>(Property))
	{
		GetActualNameCPP(AssetClassProperty, AssetClassProperty->MetaClass);
	}
	else if (auto ObjectProperty = Cast<const UObjectPropertyBase>(Property))
	{
		GetActualNameCPP(ObjectProperty, ObjectProperty->PropertyClass);
	}
	else if (auto StructProperty = Cast<const UStructProperty>(Property))
	{
		ActualCppType = FEmitHelper::GetCppName(StructProperty->Struct, false);
		ActualCppTypePtr = &ActualCppType;
	}

	if (ArrayProperty)
	{
		Property = ArrayProperty;
		if (ActualCppTypePtr)
		{
			const FString LocalActualCppType = ActualCppType;
			ActualCppType.Empty();
			const FString LocalActualExtendedType = ActualExtendedType;
			ActualExtendedType.Empty();
			ActualExtendedTypePtr = nullptr;

			const bool bIsParameter = (DeclarationType == EExportedDeclaration::Parameter) || (DeclarationType == EExportedDeclaration::MacroParameter);
			const uint32 LocalExportCPPFlags = InExportCPPFlags | (bIsParameter ? CPPF_ArgumentOrReturnValue : 0);

			ActualCppType = ArrayProperty->GetCPPTypeCustom(&ActualExtendedType, LocalExportCPPFlags, LocalActualCppType, LocalActualExtendedType);
			if (!ActualExtendedType.IsEmpty())
			{
				ActualExtendedTypePtr = &ActualExtendedType;
			}
		}
	}

	FStringOutputDevice Out;
	Property->ExportCppDeclaration(Out, DeclarationType, nullptr, InExportCPPFlags, bSkipParameterName, ActualCppTypePtr, ActualExtendedTypePtr);
	return FString(Out);

}

FString FEmitHelper::GetCppName(const UField* Field, bool bUInterface)
{
	check(Field);
	auto AsClass = Cast<UClass>(Field);
	auto AsScriptStruct = Cast<UScriptStruct>(Field);
	if (AsClass || AsScriptStruct)
	{
		if (AsClass && AsClass->HasAnyClassFlags(CLASS_Interface))
		{
			ensure(AsClass->IsChildOf<UInterface>());
			return FString::Printf(TEXT("%s%s")
				, bUInterface ? TEXT("U") : TEXT("I")
				, *AsClass->GetName());
		}
		auto AsStruct = CastChecked<UStruct>(Field);
		if (AsStruct->HasAnyFlags(RF_Native))
		{
			return FString::Printf(TEXT("%s%s"), AsStruct->GetPrefixCPP(), *AsStruct->GetName());
		}
		else
		{
			return ::UnicodeToCPPIdentifier(*AsStruct->GetName(), false, AsStruct->GetPrefixCPP());
		}
	}
	else if (auto AsProperty = Cast<UProperty>(Field))
	{
		if (const UStruct* Owner = AsProperty->GetOwnerStruct())
		{
			if ((Cast<UBlueprintGeneratedClass>(Owner) ||
				!Owner->HasAnyFlags(RF_Native)))
			{
				return ::UnicodeToCPPIdentifier(AsProperty->GetName(), AsProperty->HasAnyPropertyFlags(CPF_Deprecated), TEXT("bpv__"));
			}
		}
		return AsProperty->GetNameCPP();
	}

	if (!Field->HasAnyFlags(RF_Native) && !Field->IsA<UUserDefinedEnum>())
	{
		return ::UnicodeToCPPIdentifier(Field->GetName(), false, TEXT("bpf__"));
	}
	return Field->GetName();
}

void FEmitHelper::ArrayToString(const TArray<FString>& Array, FString& OutString, const TCHAR* Separator)
{
	if (Array.Num())
	{
		OutString += Array[0];
	}
	for (int32 i = 1; i < Array.Num(); ++i)
	{
		OutString += Separator;
		OutString += Array[i];
	}
}

bool FEmitHelper::HasAllFlags(uint64 Flags, uint64 FlagsToCheck)
{
	return FlagsToCheck == (Flags & FlagsToCheck);
}

FString FEmitHelper::HandleRepNotifyFunc(const UProperty* Property)
{
	check(Property);
	if (HasAllFlags(Property->PropertyFlags, CPF_Net | CPF_RepNotify))
	{
		return FString::Printf(TEXT("ReplicatedUsing=%s"), *Property->RepNotifyFunc.ToString());
	}
	else if (HasAllFlags(Property->PropertyFlags, CPF_Net))
	{
		return TEXT("Replicated");
	}
	return FString();
}

bool FEmitHelper::MetaDataCanBeNative(const FName MetaDataName)
{
	if (MetaDataName == TEXT("ModuleRelativePath"))
	{
		return false;
	}
	return true;
}

FString FEmitHelper::HandleMetaData(const UField* Field, bool AddCategory, TArray<FString>* AdditinalMetaData)
{
	FString MetaDataStr;

	UPackage* Package = Field ? Field->GetOutermost() : nullptr;
	const UMetaData* MetaData = Package ? Package->GetMetaData() : nullptr;
	const TMap<FName, FString>* ValuesMap = MetaData ? MetaData->ObjectMetaDataMap.Find(Field) : nullptr;
	TArray<FString> MetaDataStrings;
	if (ValuesMap && ValuesMap->Num())
	{
		for (auto& Pair : *ValuesMap)
		{
			if (!MetaDataCanBeNative(Pair.Key))
			{
				continue;
			}
			if (!Pair.Value.IsEmpty())
			{
				FString Value = Pair.Value.Replace(TEXT("\n"), TEXT(""));
				MetaDataStrings.Emplace(FString::Printf(TEXT("%s=\"%s\""), *Pair.Key.ToString(), *Value));
			}
			else
			{
				MetaDataStrings.Emplace(Pair.Key.ToString());
			}
		}
	}
	if (AddCategory && (!ValuesMap || !ValuesMap->Find(TEXT("Category"))))
	{
		MetaDataStrings.Emplace(TEXT("Category"));
	}
	if (AdditinalMetaData)
	{
		MetaDataStrings.Append(*AdditinalMetaData);
	}
	if (Field)
	{
		MetaDataStrings.Emplace(FString::Printf(TEXT("OverrideNativeName=\"%s\""), *Field->GetName()));
	}
	MetaDataStrings.Remove(FString());
	if (MetaDataStrings.Num())
	{
		MetaDataStr += TEXT("meta=(");
		ArrayToString(MetaDataStrings, MetaDataStr, TEXT(", "));
		MetaDataStr += TEXT(")");
	}
	return MetaDataStr;
}

#ifdef HANDLE_CPF_TAG
	static_assert(false, "Macro HANDLE_CPF_TAG redefinition.");
#endif
#define HANDLE_CPF_TAG(TagName, CheckedFlags) if (HasAllFlags(Flags, (CheckedFlags))) { Tags.Emplace(TagName); }

TArray<FString> FEmitHelper::ProperyFlagsToTags(uint64 Flags, bool bIsClassProperty)
{
	TArray<FString> Tags;

	// EDIT FLAGS
	if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst | CPF_DisableEditOnInstance)))
	{
		Tags.Emplace(TEXT("VisibleDefaultsOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst | CPF_DisableEditOnTemplate)))
	{
		Tags.Emplace(TEXT("VisibleInstanceOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst)))
	{
		Tags.Emplace(TEXT("VisibleAnywhere"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_DisableEditOnInstance)))
	{
		Tags.Emplace(TEXT("EditDefaultsOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_DisableEditOnTemplate)))
	{
		Tags.Emplace(TEXT("EditInstanceOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit)))
	{
		Tags.Emplace(TEXT("EditAnywhere"));
	}

	// BLUEPRINT EDIT
	if (HasAllFlags(Flags, (CPF_BlueprintVisible | CPF_BlueprintReadOnly)))
	{
		Tags.Emplace(TEXT("BlueprintReadOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_BlueprintVisible)))
	{
		Tags.Emplace(TEXT("BlueprintReadWrite"));
	}

	// CONFIG
	if (HasAllFlags(Flags, (CPF_GlobalConfig | CPF_Config)))
	{
		Tags.Emplace(TEXT("GlobalConfig"));
	}
	else if (HasAllFlags(Flags, (CPF_Config)))
	{
		Tags.Emplace(TEXT("Config"));
	}

	// OTHER
	HANDLE_CPF_TAG(TEXT("Transient"), CPF_Transient)
	HANDLE_CPF_TAG(TEXT("DuplicateTransient"), CPF_DuplicateTransient)
	HANDLE_CPF_TAG(TEXT("TextExportTransient"), CPF_TextExportTransient)
	HANDLE_CPF_TAG(TEXT("NonPIEDuplicateTransient"), CPF_NonPIEDuplicateTransient)
	HANDLE_CPF_TAG(TEXT("Export"), CPF_ExportObject)
	HANDLE_CPF_TAG(TEXT("NoClear"), CPF_NoClear)
	HANDLE_CPF_TAG(TEXT("EditFixedSize"), CPF_EditFixedSize)
	if (!bIsClassProperty)
	{
		HANDLE_CPF_TAG(TEXT("NotReplicated"), CPF_RepSkip)
	}

	HANDLE_CPF_TAG(TEXT("Interp"), CPF_Edit | CPF_BlueprintVisible | CPF_Interp)
	HANDLE_CPF_TAG(TEXT("NonTransactional"), CPF_NonTransactional)
	HANDLE_CPF_TAG(TEXT("BlueprintAssignable"), CPF_BlueprintAssignable)
	HANDLE_CPF_TAG(TEXT("BlueprintCallable"), CPF_BlueprintCallable)
	HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), CPF_BlueprintAuthorityOnly)
	HANDLE_CPF_TAG(TEXT("AssetRegistrySearchable"), CPF_AssetRegistrySearchable)
	HANDLE_CPF_TAG(TEXT("SimpleDisplay"), CPF_SimpleDisplay)
	HANDLE_CPF_TAG(TEXT("AdvancedDisplay"), CPF_AdvancedDisplay)
	HANDLE_CPF_TAG(TEXT("SaveGame"), CPF_SaveGame)

	//TODO:
	//HANDLE_CPF_TAG(TEXT("Instanced"), CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference)

	return Tags;
}

TArray<FString> FEmitHelper::FunctionFlagsToTags(uint64 Flags)
{
	TArray<FString> Tags;

	//Pointless: BlueprintNativeEvent, BlueprintImplementableEvent
	//Pointless: CustomThunk

	//TODO: SealedEvent
	//TODO: Unreliable
	//TODO: ServiceRequest, ServiceResponse

	HANDLE_CPF_TAG(TEXT("Exec"), FUNC_Exec)
	HANDLE_CPF_TAG(TEXT("Server"), FUNC_Net | FUNC_NetServer)
	HANDLE_CPF_TAG(TEXT("Client"), FUNC_Net | FUNC_NetClient)
	HANDLE_CPF_TAG(TEXT("NetMulticast"), FUNC_Net | FUNC_NetMulticast)
	HANDLE_CPF_TAG(TEXT("Reliable"), FUNC_NetReliable)
	HANDLE_CPF_TAG(TEXT("BlueprintCallable"), FUNC_BlueprintCallable)
	HANDLE_CPF_TAG(TEXT("BlueprintPure"), FUNC_BlueprintCallable | FUNC_BlueprintPure)
	HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), FUNC_BlueprintAuthorityOnly)
	HANDLE_CPF_TAG(TEXT("BlueprintCosmetic"), FUNC_BlueprintCosmetic)
	HANDLE_CPF_TAG(TEXT("WithValidation"), FUNC_NetValidate)

	return Tags;
}

#undef HANDLE_CPF_TAG

bool FEmitHelper::IsBlueprintNativeEvent(uint64 FunctionFlags)
{
	return HasAllFlags(FunctionFlags, FUNC_Event | FUNC_BlueprintEvent | FUNC_Native);
}

bool FEmitHelper::IsBlueprintImplementableEvent(uint64 FunctionFlags)
{
	return HasAllFlags(FunctionFlags, FUNC_Event | FUNC_BlueprintEvent) && !HasAllFlags(FunctionFlags, FUNC_Native);
}

FString FEmitHelper::GenerateReplaceConvertedMD(UObject* Obj)
{
	FString Result;
	if (Obj)
	{
		Result = TEXT("ReplaceConverted=\"");

		// 1. Current object
		Result += Obj->GetPathName();

		// 2. Loaded Redirectors
		{
			struct FLocalHelper
			{
				static UObject* FindFinalObject(UObjectRedirector* Redirector)
				{
					auto DestObj = Redirector ? Redirector->DestinationObject : nullptr;
					auto InnerRedir = Cast<UObjectRedirector>(DestObj);
					return InnerRedir ? FindFinalObject(InnerRedir) : DestObj;
				}
			};

			TArray<UObject*> AllObjects;
			GetObjectsOfClass(UObjectRedirector::StaticClass(), AllObjects);
			for (auto LocalObj : AllObjects)
			{
				auto Redirector = CastChecked<UObjectRedirector>(LocalObj);
				if (Obj == FLocalHelper::FindFinalObject(Redirector))
				{
					Result += TEXT(",");
					Result += Redirector->GetPathName();
				}
			}
		}

		// 3. Unloaded Redirectors
		// TODO: It would be better to load all redirectors before compiling. Than checking here AssetRegistry.

		Result += TEXT("\"");

		// 4. Add overridden name:
		Result += TEXT(", OverrideNativeName=\"");
		Result += Obj->GetName();
		Result += TEXT("\"");
	}

	return Result;
}

FString FEmitHelper::GetBaseFilename(const UObject* AssetObj)
{
	FString PackagePath = AssetObj->GetOutermost()->GetPathName();
	// We have to sanitize the package path because UHT is going to generate header guards (preprocessor symbols)
	// based on the filename. I'm also not interested in exploring the depth of unicode filename support in UHT,
	// UBT, and our various c++ toolchains, so this logic is pretty aggressive:
	FString Postfix = TEXT("__pf");
	for (auto& Char : PackagePath)
	{
		if (Char == '/' || Char == '\\')
		{
			continue;
		}

		if (!IsValidCPPIdentifierChar(Char))
		{
			// deterministically map char to a valid ascii character, we have 63 characters available (aA-zZ, 0-9, and _)
			// so the optimal encoding would be base 63:
			Postfix.Append(ToValidCPPIdentifierChars(Char));
			Char = TCHAR('x');
		}
	}

	return FPackageName::GetLongPackageAssetName(PackagePath+Postfix);
}

FString FEmitHelper::GetPCHFilename()
{
	FString PCHFilename;
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();

	auto& PchFilenameQuery = BackEndModule.OnPCHFilenameQuery();
	if (PchFilenameQuery.IsBound())
	{
		PCHFilename = PchFilenameQuery.Execute();
	}

	if (PCHFilename.IsEmpty())
	{
		PCHFilename  = FApp::GetGameName();
		PCHFilename += TEXT(".h");
	}
	return PCHFilename;
}

FString FEmitHelper::EmitUFuntion(UFunction* Function, TArray<FString>* AdditinalMetaData)
{
	TArray<FString> Tags = FEmitHelper::FunctionFlagsToTags(Function->FunctionFlags);

	auto FunctionOwnerClass = Function->GetOwnerClass();
	if (FunctionOwnerClass->IsChildOf<UInterface>())
	{
		Tags.Emplace(TEXT("BlueprintNativeEvent"));
	}

	const bool bMustHaveCategory = (Function->FunctionFlags & (FUNC_BlueprintCallable | FUNC_BlueprintPure)) != 0;
	Tags.Emplace(FEmitHelper::HandleMetaData(Function, bMustHaveCategory, AdditinalMetaData));
	Tags.Remove(FString());

	FString AllTags;
	FEmitHelper::ArrayToString(Tags, AllTags, TEXT(", "));

	return FString::Printf(TEXT("UFUNCTION(%s)"), *AllTags);
}

int32 FEmitHelper::ParseDelegateDetails(FEmitterLocalContext& EmitterContext, UFunction* Signature, FString& OutParametersMacro, FString& OutParamNumberStr)
{
	int32 ParameterNum = 0;
	FStringOutputDevice Parameters;
	for (TFieldIterator<UProperty> PropIt(Signature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		Parameters += ", ";
		Parameters += EmitterContext.ExportCppDeclaration(*PropIt, EExportedDeclaration::MacroParameter, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend);
		++ParameterNum;
	}

	FString ParamNumberStr;
	switch (ParameterNum)
	{
	case 0: ParamNumberStr = TEXT("");				break;
	case 1: ParamNumberStr = TEXT("_OneParam");		break;
	case 2: ParamNumberStr = TEXT("_TwoParams");	break;
	case 3: ParamNumberStr = TEXT("_ThreeParams");	break;
	case 4: ParamNumberStr = TEXT("_FourParams");	break;
	case 5: ParamNumberStr = TEXT("_FiveParams");	break;
	case 6: ParamNumberStr = TEXT("_SixParams");	break;
	case 7: ParamNumberStr = TEXT("_SevenParams");	break;
	case 8: ParamNumberStr = TEXT("_EightParams");	break;
	default: ParamNumberStr = TEXT("_TooMany");		break;
	}

	OutParametersMacro = Parameters;
	OutParamNumberStr = ParamNumberStr;
	return ParameterNum;
}

void FEmitHelper::EmitSinglecastDelegateDeclarations(FEmitterLocalContext& EmitterContext, const TArray<UDelegateProperty*>& Delegates)
{
	for (auto It : Delegates)
	{
		check(It);
		auto Signature = It->SignatureFunction;
		check(Signature);

		FString ParamNumberStr, Parameters;
		ParseDelegateDetails(EmitterContext, Signature, Parameters, ParamNumberStr);

		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
		const FString TypeName = EmitterContext.ExportCppDeclaration(It, EExportedDeclaration::Parameter, LocalExportCPPFlags, true);
		EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_DELEGATE%s(%s%s);"), *ParamNumberStr, *TypeName, *Parameters));
	}
}

void FEmitHelper::EmitMulticastDelegateDeclarations(FEmitterLocalContext& EmitterContext)
{
	for (TFieldIterator<UMulticastDelegateProperty> It(EmitterContext.GetCurrentlyGeneratedClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		auto Signature = It->SignatureFunction;
		check(Signature);

		FString ParamNumberStr, Parameters;
		ParseDelegateDetails(EmitterContext, Signature, Parameters, ParamNumberStr);

		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
		const FString TypeName = EmitterContext.ExportCppDeclaration(*It, EExportedDeclaration::Parameter, LocalExportCPPFlags, true);
		EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE%s(%s%s);"), *ParamNumberStr, *TypeName, *Parameters));
	}
}

void FEmitHelper::EmitLifetimeReplicatedPropsImpl(FEmitterLocalContext& EmitterContext)
{
	auto SourceClass = EmitterContext.GetCurrentlyGeneratedClass();
	auto CppClassName = FEmitHelper::GetCppName(SourceClass);
	bool bFunctionInitilzed = false;
	for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if ((It->PropertyFlags & CPF_Net) != 0)
		{
			if (!bFunctionInitilzed)
			{
				EmitterContext.AddLine(FString::Printf(TEXT("void %s::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const"), *CppClassName));
				EmitterContext.AddLine(TEXT("{"));
				EmitterContext.IncreaseIndent();
				EmitterContext.AddLine(TEXT("Super::GetLifetimeReplicatedProps(OutLifetimeProps);"));
				bFunctionInitilzed = true;
			}
			EmitterContext.AddLine(FString::Printf(TEXT("DOREPLIFETIME(%s, %s);"), *CppClassName, *FEmitHelper::GetCppName(*It)));
		}
	}
	if (bFunctionInitilzed)
	{
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));
	}
}

FString FEmitHelper::LiteralTerm(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& Type, const FString& CustomValue, UObject* LiteralObject)
{
	auto Schema = GetDefault<UEdGraphSchema_K2>();

	if (UEdGraphSchema_K2::PC_String == Type.PinCategory)
	{
		return FString::Printf(TEXT("TEXT(\"%s\")"), *CustomValue.ReplaceCharWithEscapedChar());
	}
	else if (UEdGraphSchema_K2::PC_Text == Type.PinCategory)
	{
		return FString::Printf(TEXT("FText::FromString(TEXT(\"%s\"))"), *CustomValue.ReplaceCharWithEscapedChar());
	}
	else if (UEdGraphSchema_K2::PC_Float == Type.PinCategory)
	{
		float Value = CustomValue.IsEmpty() ? 0.0f : FCString::Atof(*CustomValue);
		return FString::Printf(TEXT("%f"), Value);
	}
	else if (UEdGraphSchema_K2::PC_Int == Type.PinCategory)
	{
		int32 Value = CustomValue.IsEmpty() ? 0 : FCString::Atoi(*CustomValue);
		return FString::Printf(TEXT("%d"), Value);
	}
	else if ((UEdGraphSchema_K2::PC_Byte == Type.PinCategory) || (UEdGraphSchema_K2::PC_Enum == Type.PinCategory))
	{
		auto TypeEnum = Cast<UEnum>(Type.PinSubCategoryObject.Get());
		if (TypeEnum)
		{
			// @note: We have to default to the zeroth entry because there may not be a symbol associated with the 
			// last element (UHT generates a MAX entry for UEnums, even if the user did not declare them, but UHT 
			// does not generate a symbol for said entry.
			return FString::Printf(TEXT("%s::%s"), *FEmitHelper::GetCppName(TypeEnum), CustomValue.IsEmpty()
				? *TypeEnum->GetEnumName(0)
				: *CustomValue);
		}
		else
		{
			uint8 Value = CustomValue.IsEmpty() ? 0 : FCString::Atoi(*CustomValue);
			return FString::Printf(TEXT("%u"), Value);
		}
	}
	else if (UEdGraphSchema_K2::PC_Boolean == Type.PinCategory)
	{
		const bool bValue = CustomValue.ToBool();
		return bValue ? TEXT("true") : TEXT("false");
	}
	else if (UEdGraphSchema_K2::PC_Name == Type.PinCategory)
	{
		return CustomValue.IsEmpty() 
			? TEXT("FName()") 
			: FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *(FName(*CustomValue).ToString().ReplaceCharWithEscapedChar()));
	}
	else if (UEdGraphSchema_K2::PC_Struct == Type.PinCategory)
	{
		auto StructType = Cast<UScriptStruct>(Type.PinSubCategoryObject.Get());
		ensure(StructType);
		if (StructType == TBaseStructure<FVector>::Get())
		{
			FVector Vect = FVector::ZeroVector;
			FDefaultValueHelper::ParseVector(CustomValue, /*out*/ Vect);
			return FString::Printf(TEXT("FVector(%f,%f,%f)"), Vect.X, Vect.Y, Vect.Z);
		}
		else if (StructType == TBaseStructure<FRotator>::Get())
		{
			FRotator Rot = FRotator::ZeroRotator;
			FDefaultValueHelper::ParseRotator(CustomValue, /*out*/ Rot);
			return FString::Printf(TEXT("FRotator(%f,%f,%f)"), Rot.Pitch, Rot.Yaw, Rot.Roll);
		}
		else if (StructType == TBaseStructure<FTransform>::Get())
		{
			FTransform Trans = FTransform::Identity;
			Trans.InitFromString(CustomValue);
			const FQuat Rot = Trans.GetRotation();
			const FVector Translation = Trans.GetTranslation();
			const FVector Scale = Trans.GetScale3D();
			return FString::Printf(TEXT("FTransform( FQuat(%f,%f,%f,%f), FVector(%f,%f,%f), FVector(%f,%f,%f) )"),
				Rot.X, Rot.Y, Rot.Z, Rot.W, Translation.X, Translation.Y, Translation.Z, Scale.X, Scale.Y, Scale.Z);
		}
		else if (StructType == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor LinearColor;
			LinearColor.InitFromString(CustomValue);
			return FString::Printf(TEXT("FLinearColor(%f,%f,%f,%f)"), LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A);
		}
		else if (StructType == TBaseStructure<FColor>::Get())
		{
			FColor Color;
			Color.InitFromString(CustomValue);
			return FString::Printf(TEXT("FColor(%d,%d,%d,%d)"), Color.R, Color.G, Color.B, Color.A);
		}
		if (StructType == TBaseStructure<FVector2D>::Get())
		{
			FVector2D Vect = FVector2D::ZeroVector;
			Vect.InitFromString(CustomValue);
			return FString::Printf(TEXT("FVector2D(%f,%f)"), Vect.X, Vect.Y);
		}
		else
		{
			//@todo:  This needs to be more robust, since import text isn't really proper for struct construction.
			const bool bEmptyCustomValue = CustomValue.IsEmpty() || (CustomValue == TEXT("()"));
			const FString StructName = *FEmitHelper::GetCppName(StructType);
			if (bEmptyCustomValue)
			{
				return StructName + (StructType->IsA<UUserDefinedStruct>() ? TEXT("::GetDefaultValue()") : TEXT("{}"));
			}

			const FString LocalStructNativeName = EmitterContext.GenerateUniqueLocalName();
			EmitterContext.AddLine(FString::Printf(TEXT("auto %s = %s{};"), *LocalStructNativeName, *StructName)); // TODO: ?? should "::GetDefaultValue()" be called here?
			{
				FStructOnScope StructOnScope(StructType);

				FStringOutputDevice ImportError;
				const auto EndOfParsedBuff = UStructProperty::ImportText_Static(StructType, TEXT("FEmitHelper::LiteralTerm"), *CustomValue, StructOnScope.GetStructMemory(), 0, nullptr, &ImportError);
				if (!EndOfParsedBuff || !ImportError.IsEmpty())
				{
					UE_LOG(LogK2Compiler, Error, TEXT("FEmitHelper::LiteralTerm cannot parse struct \"%s\" error: %s"), *CustomValue, *ImportError);
				}

				for (auto LocalProperty : TFieldRange<const UProperty>(StructType))
				{
					FEmitDefaultValueHelper::OuterGenerate(EmitterContext, LocalProperty, LocalStructNativeName, StructOnScope.GetStructMemory(), nullptr, FEmitDefaultValueHelper::EPropertyAccessOperator::Dot);
				}
			}
			return LocalStructNativeName;
		}
	}
	else if (Type.PinSubCategory == UEdGraphSchema_K2::PSC_Self)
	{
		return TEXT("this");
	}
	else if (UEdGraphSchema_K2::PC_Class == Type.PinCategory)
	{
		if (auto FoundClass = Cast<const UClass>(LiteralObject))
		{
			const FString MappedObject = EmitterContext.FindGloballyMappedObject(LiteralObject);
			if (!MappedObject.IsEmpty())
			{
				return MappedObject;
			}
			return FString::Printf(TEXT("LoadClass<UObject>(nullptr, TEXT(\"%s\"), nullptr, 0, nullptr)"), *(LiteralObject->GetPathName().ReplaceCharWithEscapedChar()));
		}
		return FString(TEXT("nullptr"));
	}
	else if((UEdGraphSchema_K2::PC_AssetClass == Type.PinCategory) || (UEdGraphSchema_K2::PC_Asset == Type.PinCategory))
	{
		if (LiteralObject)
		{
			const FString MappedObject = EmitterContext.FindGloballyMappedObject(LiteralObject);
			if (!MappedObject.IsEmpty())
			{
				return MappedObject;
			}
			return FString::Printf(TEXT("FStringAssetReference(TEXT(\"%s\"))"), *(LiteralObject->GetPathName().ReplaceCharWithEscapedChar()));
		}
		return FString(TEXT("nullptr"));
	}
	else if (UEdGraphSchema_K2::PC_Object == Type.PinCategory)
	{
		if (LiteralObject)
		{
			UClass* FoundClass = Cast<UClass>(Type.PinSubCategoryObject.Get());
			UClass* ObjectClassToUse = FoundClass ? EmitterContext.GetFirstNativeOrConvertedClass(FoundClass) : UObject::StaticClass();
			const FString MappedObject = EmitterContext.FindGloballyMappedObject(LiteralObject, ObjectClassToUse, true);
			if (!MappedObject.IsEmpty())
			{
				return MappedObject;
			}
			return FString(TEXT("nullptr"));
		}
		else
		{
			return FString(TEXT("nullptr"));
		}
	}
	else if (UEdGraphSchema_K2::PC_Interface == Type.PinCategory)
	{
		if (!LiteralObject && CustomValue.IsEmpty())
		{
			return FString(TEXT("nullptr"));
		}
	}
	/*
	else if (CoerceProperty->IsA(UInterfaceProperty::StaticClass()))
	{
		if (Term->Type.PinSubCategory == PSC_Self)
		{
			return TEXT("this");
		}
		else
		{
			ensureMsgf(false, TEXT("It is not possible to express this interface property as a literal value!"));
			return Term->Name;
		}
	}
	*/
	ensureMsgf(false, TEXT("It is not possible to express this type as a literal value!"));
	return CustomValue;
}

FString FEmitHelper::DefaultValue(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& Type)
{
	return LiteralTerm(EmitterContext, Type, FString(), nullptr);
}

UFunction* FEmitHelper::GetOriginalFunction(UFunction* Function)
{
	check(Function);
	const FName FunctionName = Function->GetFName();

	UClass* Owner = Function->GetOwnerClass();
	check(Owner);
	for (auto& Inter : Owner->Interfaces)
	{
		if (UFunction* Result = Inter.Class->FindFunctionByName(FunctionName))
		{
			return GetOriginalFunction(Result);
		}
	}

	for (UClass* SearchClass = Owner->GetSuperClass(); SearchClass; SearchClass = SearchClass->GetSuperClass())
	{
		if (UFunction* Result = SearchClass->FindFunctionByName(FunctionName))
		{
			return GetOriginalFunction(Result);
		}
	}

	return Function;
}

bool FEmitHelper::ShouldHandleAsNativeEvent(UFunction* Function)
{
	check(Function);
	auto OriginalFunction = FEmitHelper::GetOriginalFunction(Function);
	check(OriginalFunction);
	if (OriginalFunction != Function)
	{
		const uint32 FlagsToCheckMask = EFunctionFlags::FUNC_Event | EFunctionFlags::FUNC_BlueprintEvent | EFunctionFlags::FUNC_Native;
		const uint32 FlagsToCheck = OriginalFunction->FunctionFlags & FlagsToCheckMask;

		auto OriginalOwner = OriginalFunction->GetOwnerClass();
		const bool bBPInterface = Cast<UBlueprintGeneratedClass>(OriginalOwner) && OriginalOwner->IsChildOf<UInterface>();
		return (FlagsToCheck == FlagsToCheckMask) || bBPInterface;
	}
	return false;
}

bool FEmitHelper::ShouldHandleAsImplementableEvent(UFunction* Function)
{
	check(Function);
	auto OriginalFunction = FEmitHelper::GetOriginalFunction(Function);
	check(OriginalFunction);
	if (OriginalFunction != Function)
	{
		const uint32 FlagsToCheckMask = EFunctionFlags::FUNC_Event | EFunctionFlags::FUNC_BlueprintEvent | EFunctionFlags::FUNC_Native;
		const uint32 FlagsToCheck = OriginalFunction->FunctionFlags & FlagsToCheckMask;
		return (FlagsToCheck == (EFunctionFlags::FUNC_Event | EFunctionFlags::FUNC_BlueprintEvent));
	}
	return false;
}

bool FEmitHelper::GenerateAutomaticCast(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& LType, const FEdGraphPinType& RType, FString& OutCastBegin, FString& OutCastEnd)
{
	// BYTE to ENUM cast
	// ENUM to BYTE cast
	if ((LType.PinCategory == UEdGraphSchema_K2::PC_Byte) && (RType.PinCategory == UEdGraphSchema_K2::PC_Byte))
	{
		auto LTypeEnum = Cast<UEnum>(LType.PinSubCategoryObject.Get());
		auto RTypeEnum = Cast<UEnum>(RType.PinSubCategoryObject.Get());
		if (!RTypeEnum && LTypeEnum)
		{
			const FString EnumCppType = !LTypeEnum->CppType.IsEmpty() ? LTypeEnum->CppType : FEmitHelper::GetCppName(LTypeEnum);
			OutCastBegin = FString::Printf(TEXT("static_cast<%s>("), *EnumCppType);
			OutCastEnd = TEXT(")");
			return true;
		}
		if (!LTypeEnum && RTypeEnum)
		{
			const FString EnumCppType = !RTypeEnum->CppType.IsEmpty() ? RTypeEnum->CppType : FEmitHelper::GetCppName(RTypeEnum);
			OutCastBegin = FString::Printf(TEXT("EnumToByte<%s>("), *EnumCppType); 
			OutCastEnd = TEXT(")");
			return true;
		}
	}

	// OBJECT to OBJECT
	if (LType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		UClass* LClass = Cast<UClass>(LType.PinSubCategoryObject.Get());
		LClass = LClass ? EmitterContext.GetFirstNativeOrConvertedClass(LClass) : nullptr;
		UClass* RClass = Cast<UClass>(RType.PinSubCategoryObject.Get());
		if (LClass && RClass && LClass->IsChildOf(RClass) && !RClass->IsChildOf(LClass))
		{
			OutCastBegin = FString::Printf(TEXT("CastChecked<%s>("), *FEmitHelper::GetCppName(LClass));
			OutCastEnd = TEXT(")");
			return true;
		}
	}

	return false;
}

FString FEmitHelper::ReplaceConvertedMetaData(UObject* Obj)
{
	FString Result;
	const FString ReplaceConvertedMD = FEmitHelper::GenerateReplaceConvertedMD(Obj);
	if (!ReplaceConvertedMD.IsEmpty())
	{
		TArray<FString> AdditionalMD;
		AdditionalMD.Add(ReplaceConvertedMD);
		Result += TEXT(",");
		Result += FEmitHelper::HandleMetaData(nullptr, false, &AdditionalMD);
	}
	return Result;
}
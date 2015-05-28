// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Shader.cpp: Shader implementation.
=============================================================================*/

#include "ShaderCore.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "DiagnosticTable.h"
#include "DerivedDataCacheInterface.h"
#include "ModuleManager.h"
#include "TargetPlatform.h"
#include "RHICommandList.h"
#include "ShaderCache.h"


DEFINE_LOG_CATEGORY(LogShaders);


void FShaderParameterMap::VerifyBindingsAreComplete(const TCHAR* ShaderTypeName, EShaderFrequency Frequency, FVertexFactoryType* InVertexFactoryType) const
{
#if WITH_EDITORONLY_DATA
	// Only people working on shaders (and therefore have LogShaders unsuppressed) will want to see these errors
	if (UE_LOG_ACTIVE(LogShaders, Warning))
	{
		const TCHAR* VertexFactoryName = InVertexFactoryType ? InVertexFactoryType->GetName() : TEXT("?");

		bool bBindingsComplete = true;
		FString UnBoundParameters = TEXT("");
		for (TMap<FString,FParameterAllocation>::TConstIterator ParameterIt(ParameterMap);ParameterIt;++ParameterIt)
		{
			const FString& ParamName = ParameterIt.Key();
			const FParameterAllocation& ParamValue = ParameterIt.Value();
			if(!ParamValue.bBound)
			{
				// Only valid parameters should be in the shader map
				checkSlow(ParamValue.Size > 0);
				bBindingsComplete = bBindingsComplete && ParamValue.bBound;
				UnBoundParameters += FString(TEXT("		Parameter ")) + ParamName + TEXT(" not bound!\n");
			}
		}
		
		if (!bBindingsComplete)
		{
			FString ErrorMessage = FString(TEXT("Found unbound parameters being used in shadertype ")) + ShaderTypeName + TEXT(" (VertexFactory: ") + VertexFactoryName + TEXT(")\n") + UnBoundParameters;
			// An unbound parameter means the engine is not going to set its value (because it was never bound) 
			// but it will be used in rendering, which will most likely cause artifacts

			// We use a non-Slate message box to avoid problem where we haven't compiled the shaders for Slate.
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage, TEXT("Error"));
		}
	}
#endif // WITH_EDITORONLY_DATA
}


void FShaderParameterMap::UpdateHash(FSHA1& HashState) const
{
	for(TMap<FString,FParameterAllocation>::TConstIterator ParameterIt(ParameterMap);ParameterIt;++ParameterIt)
	{
		const FString& ParamName = ParameterIt.Key();
		const FParameterAllocation& ParamValue = ParameterIt.Value();
		HashState.Update((const uint8*)*ParamName, ParamName.Len() * sizeof(TCHAR));
		HashState.Update((const uint8*)&ParamValue.BufferIndex, sizeof(ParamValue.BufferIndex));
		HashState.Update((const uint8*)&ParamValue.BaseIndex, sizeof(ParamValue.BaseIndex));
		HashState.Update((const uint8*)&ParamValue.Size, sizeof(ParamValue.Size));
	}
}

bool FShaderType::bInitializedSerializationHistory = false;

FShaderType::FShaderType(
	const TCHAR* InName,
	const TCHAR* InSourceFilename,
	const TCHAR* InFunctionName,
	uint32 InFrequency,
	ConstructSerializedType InConstructSerializedRef,
	GetStreamOutElementsType InGetStreamOutElementsRef
	):
	Name(InName),
	SourceFilename(InSourceFilename),
	FunctionName(InFunctionName),
	Frequency(InFrequency),
	ConstructSerializedRef(InConstructSerializedRef),
	GetStreamOutElementsRef(InGetStreamOutElementsRef),
	GlobalListLink(this)
{
	for (int32 Platform = 0; Platform < SP_NumPlatforms; Platform++)
	{
		bCachedUniformBufferStructDeclarations[Platform] = false;
	}

	// This will trigger if an IMPLEMENT_SHADER_TYPE was in a module not loaded before InitializeShaderTypes
	// Shader types need to be implemented in modules that are loaded before that
	checkf(!bInitializedSerializationHistory, TEXT("Shader type was loaded after engine init, use ELoadingPhase::PostConfigInit on your module to cause it to load earlier."));

	//make sure the name is shorter than the maximum serializable length
	check(FCString::Strlen(InName) < NAME_SIZE);

	// register this shader type
	GlobalListLink.Link(GetTypeList());
	GetNameToTypeMap().Add(FName(InName), this);

	// Assign the shader type the next unassigned hash index.
	static uint32 NextHashIndex = 0;
	HashIndex = NextHashIndex++;
}

FShaderType::~FShaderType()
{
	GlobalListLink.Unlink();
	GetNameToTypeMap().Remove(FName(Name));
}

TLinkedList<FShaderType*>*& FShaderType::GetTypeList()
{
	static TLinkedList<FShaderType*>* GShaderTypeList = NULL;
	return GShaderTypeList;
}

FShaderType* FShaderType::GetShaderTypeByName(const TCHAR* Name)
{
	for(TLinkedList<FShaderType*>::TIterator It(GetTypeList()); It; It.Next())
	{
		FShaderType* Type = *It;

		if (FPlatformString::Strcmp(Name, Type->GetName()) == 0)
		{
			return Type;
		}
	}

	return NULL;
}

TArray<FShaderType*> FShaderType::GetShaderTypesByFilename(const TCHAR* Filename)
{
	TArray<FShaderType*> OutShaders;
	for(TLinkedList<FShaderType*>::TIterator It(GetTypeList()); It; It.Next())
	{
		FShaderType* Type = *It;

		if (FPlatformString::Strcmp(Filename, Type->GetShaderFilename()) == 0)
		{
			OutShaders.Add(Type);
		}
	}
	return OutShaders;
}

TMap<FName, FShaderType*>& FShaderType::GetNameToTypeMap()
{
	static TMap<FName, FShaderType*>* GShaderNameToTypeMap = NULL;
	if(!GShaderNameToTypeMap)
	{
		GShaderNameToTypeMap = new TMap<FName, FShaderType*>();
	}
	return *GShaderNameToTypeMap;
}

void FShaderType::GetOutdatedTypes(TArray<FShaderType*>& OutdatedShaderTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes)
{
	for(TLinkedList<FShaderType*>::TIterator It(GetTypeList()); It; It.Next())
	{
		FShaderType* Type = *It;
		for(TMap<FShaderId,FShader*>::TConstIterator ShaderIt(Type->ShaderIdMap);ShaderIt;++ShaderIt)
		{
			FShader* Shader = ShaderIt.Value();
			const FVertexFactoryParameterRef* VFParameterRef = Shader->GetVertexFactoryParameterRef();
			const FSHAHash& SavedHash = Shader->GetHash();
			const FSHAHash& CurrentHash = Type->GetSourceHash();
			const bool bOutdatedShader = SavedHash != CurrentHash;
			const bool bOutdatedVertexFactory =
				VFParameterRef && VFParameterRef->GetVertexFactoryType() && VFParameterRef->GetVertexFactoryType()->GetSourceHash() != VFParameterRef->GetHash();

			if (bOutdatedShader)
			{
				OutdatedShaderTypes.AddUnique(Shader->Type);
			}

			if (bOutdatedVertexFactory)
			{
				OutdatedFactoryTypes.AddUnique(VFParameterRef->GetVertexFactoryType());
			}
		}
	}

	for (int32 TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
	{
		UE_LOG(LogShaders, Warning, TEXT("		Recompiling %s"), OutdatedShaderTypes[TypeIndex]->GetName());
	}
	for (int32 TypeIndex = 0; TypeIndex < OutdatedFactoryTypes.Num(); TypeIndex++)
	{
		UE_LOG(LogShaders, Warning, TEXT("		Recompiling %s"), OutdatedFactoryTypes[TypeIndex]->GetName());
	}
}


FArchive& operator<<(FArchive& Ar,FShaderType*& Ref)
{
	if(Ar.IsSaving())
	{
		FName ShaderTypeName = Ref ? FName(Ref->Name) : NAME_None;
		Ar << ShaderTypeName;
	}
	else if(Ar.IsLoading())
	{
		FName ShaderTypeName = NAME_None;
		Ar << ShaderTypeName;
		
		Ref = NULL;

		if(ShaderTypeName != NAME_None)
		{
			// look for the shader type in the global name to type map
			FShaderType** ShaderType = FShaderType::GetNameToTypeMap().Find(ShaderTypeName);
			if (ShaderType)
			{
				// if we found it, use it
				Ref = *ShaderType;
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("ShaderType '%s' was not found!"), *ShaderTypeName.ToString());
			}
		}
	}
	return Ar;
}


TRefCountPtr<FShader> FShaderType::FindShaderById(const FShaderId& Id)
{
	FScopeLock MapLock(&ShaderIdMapCritical);
	TRefCountPtr<FShader> Result = ShaderIdMap.FindRef(Id);
	return Result;
}

FShader* FShaderType::ConstructForDeserialization() const
{
	return (*ConstructSerializedRef)();
}

const FSHAHash& FShaderType::GetSourceHash() const
{
	return GetShaderFileHash(GetShaderFilename());
}

void FShaderType::Initialize(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
		{
			FShaderType* Type = *It;
			GenerateReferencedUniformBuffers(Type->SourceFilename, Type->Name, ShaderFileToUniformBufferVariables, Type->ReferencedUniformBufferStructsCache);

			// Cache serialization history for each shader type
			// This history is used to detect when shader serialization changes without a corresponding .usf change
			{
				// Construct a temporary shader, which is initialized to safe values for serialization
				FShader* TempShader = Type->ConstructForDeserialization();
				check(TempShader != NULL);
				TempShader->Type = Type;

				// Serialize the temp shader to memory and record the number and sizes of serializations
				TArray<uint8> TempData;
				FMemoryWriter Ar(TempData, true);
				FShaderSaveArchive SaveArchive(Ar, Type->SerializationHistory);
				TempShader->SerializeBase(SaveArchive, false);

				// Destroy the temporary shader
				delete TempShader;
			}
		}
	}

	bInitializedSerializationHistory = true;
}

void FShaderType::Uninitialize()
{
	for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
	{
		FShaderType* Type = *It;
		Type->SerializationHistory = FSerializationHistory();
	}

	bInitializedSerializationHistory = false;
}

TMap<FShaderResourceId, FShaderResource*> FShaderResource::ShaderResourceIdMap;
FCriticalSection FShaderResource::ShaderResourceIdMapCritical;

FShaderResource::FShaderResource()
	: SpecificType(NULL)
	, NumInstructions(0)
	, NumTextureSamplers(0)
	, NumRefs(0)
	, Canary(FShader::ShaderMagic_Uninitialized)
{
	INC_DWORD_STAT_BY(STAT_Shaders_NumShaderResourcesLoaded, 1);
}


FShaderResource::FShaderResource(const FShaderCompilerOutput& Output, FShaderType* InSpecificType) 
	: SpecificType(InSpecificType)
	, NumInstructions(Output.NumInstructions)
	, NumTextureSamplers(Output.NumTextureSamplers)
	, NumRefs(0)
	, Canary(FShader::ShaderMagic_Initialized)
	
{
	Target = Output.Target;
	Code = Output.Code;
	check(Code.Num() > 0);

	OutputHash = Output.OutputHash;
	checkSlow(OutputHash != FSHAHash());

	{
		FScopeLock ShaderResourceIdMapLock(&ShaderResourceIdMapCritical);
		ShaderResourceIdMap.Add(GetId(), this);
	}
	
	INC_DWORD_STAT_BY_FName(GetMemoryStatType((EShaderFrequency)Target.Frequency).GetName(), Code.Num());
	INC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
	INC_DWORD_STAT_BY(STAT_Shaders_NumShaderResourcesLoaded, 1);
}


FShaderResource::~FShaderResource()
{
	check(Canary == FShader::ShaderMagic_Uninitialized || Canary == FShader::ShaderMagic_CleaningUp || Canary == FShader::ShaderMagic_Initialized);
	check(NumRefs == 0);
	Canary = 0;

	DEC_DWORD_STAT_BY_FName(GetMemoryStatType((EShaderFrequency)Target.Frequency).GetName(), Code.Num());
	DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
	DEC_DWORD_STAT_BY(STAT_Shaders_NumShaderResourcesLoaded, 1);
}


void FShaderResource::Register()
{
	FScopeLock ShaderResourceIdMapLock(&ShaderResourceIdMapCritical);
	ShaderResourceIdMap.Add(GetId(), this);
}


void FShaderResource::Serialize(FArchive& Ar)
{
	Ar << SpecificType;
	Ar << Target;
	Ar << Code;
	Ar << OutputHash;
	Ar << NumInstructions;
	Ar << NumTextureSamplers;
	
	if (Ar.IsLoading())
	{
		INC_DWORD_STAT_BY_FName(GetMemoryStatType((EShaderFrequency)Target.Frequency).GetName(), (int64)Code.Num());
		INC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
		
		FShaderCache::LogShader((EShaderPlatform)Target.Platform, (EShaderFrequency)Target.Frequency, OutputHash, Code);

		// The shader resource has been serialized in, so this shader resource is now initialized.
		check(Canary != FShader::ShaderMagic_CleaningUp);
		Canary = FShader::ShaderMagic_Initialized;
	}
}


void FShaderResource::AddRef()
{
	// Lock shader id map to prevent anything from acquiring shaders while we manipulate their references
	FScopeLock ShaderResourceIdMapLock(&ShaderResourceIdMapCritical);
	check(Canary != FShader::ShaderMagic_CleaningUp);
	++NumRefs;
}


void FShaderResource::Release()
{
	// We need to lock the resource map so that no resource gets acquired by
	// FindShaderResourceById while we (potentially) remove this resource
	FScopeLock ShaderResourceIdMapLock(&ShaderResourceIdMapCritical);
	check(NumRefs != 0);
	if(--NumRefs == 0)
	{
			ShaderResourceIdMap.Remove(GetId());

		// Send a release message to the rendering thread when the shader loses its last reference.
		BeginReleaseResource(this);

		Canary = FShader::ShaderMagic_CleaningUp;
		BeginCleanup(this);
	}
}


TRefCountPtr<FShaderResource> FShaderResource::FindShaderResourceById(const FShaderResourceId& Id)
{
	FScopeLock ShaderResourceIdMapLock(&ShaderResourceIdMapCritical);
	TRefCountPtr<FShaderResource> Result = ShaderResourceIdMap.FindRef(Id);
	return Result;
}


FShaderResource* FShaderResource::FindOrCreateShaderResource(const FShaderCompilerOutput& Output, FShaderType* SpecificType)
{
	const FShaderResourceId ResourceId(Output, SpecificType ? SpecificType->GetName() : NULL);
	FShaderResource* Resource = FindShaderResourceById(ResourceId);

	if (!Resource)
	{
		Resource = new FShaderResource(Output, SpecificType);
	}

	return Resource;
}

void FShaderResource::GetAllShaderResourceId(TArray<FShaderResourceId>& Ids)
{
	FScopeLock ShaderResourceIdMapLock(&ShaderResourceIdMapCritical);
	ShaderResourceIdMap.GetKeys(Ids);
}

void FShaderResource::FinishCleanup()
{
	delete this;
}

bool FShaderResource::ArePlatformsCompatible(EShaderPlatform CurrentPlatform, EShaderPlatform TargetPlatform)
{
	bool bFeatureLevelCompatible = CurrentPlatform == TargetPlatform;
	
	if (!bFeatureLevelCompatible && IsPCPlatform(CurrentPlatform) && IsPCPlatform(TargetPlatform) )
	{
		if (CurrentPlatform == SP_OPENGL_SM4_MAC || TargetPlatform == SP_OPENGL_SM4_MAC)
		{
			// prevent SP_OPENGL_SM4 == SP_OPENGL_SM4_MAC, allow SP_OPENGL_SM4_MAC == SP_OPENGL_SM4_MAC,
			// allow lesser feature levels on SP_OPENGL_SM4_MAC device.
			// do not allow MAC targets to work on non MAC devices.
			bFeatureLevelCompatible = CurrentPlatform == SP_OPENGL_SM4_MAC && 
				GetMaxSupportedFeatureLevel(CurrentPlatform) >= GetMaxSupportedFeatureLevel(TargetPlatform);
		}
		else
		{
			bFeatureLevelCompatible = GetMaxSupportedFeatureLevel(CurrentPlatform) >= GetMaxSupportedFeatureLevel(TargetPlatform);
		}

		bool bIsTargetD3D = TargetPlatform == SP_PCD3D_SM5 ||
								TargetPlatform == SP_PCD3D_SM4 ||
								TargetPlatform == SP_PCD3D_ES3_1 ||
								TargetPlatform == SP_PCD3D_ES2;

		bool bIsCurrentPlatformD3D = CurrentPlatform == SP_PCD3D_SM5 ||
								CurrentPlatform == SP_PCD3D_SM4 ||
								TargetPlatform == SP_PCD3D_ES3_1 ||
								CurrentPlatform == SP_PCD3D_ES2;

		bFeatureLevelCompatible = bFeatureLevelCompatible && (bIsCurrentPlatformD3D == bIsTargetD3D);
	}

	return bFeatureLevelCompatible;
}

void FShaderResource::InitRHI()
{
	checkf(Code.Num() > 0, TEXT("FShaderResource::InitRHI was called with empty bytecode, which can happen if the resource is initialized multiple times on platforms with no editor data."));

	// we can't have this called on the wrong platform's shaders
	if (!ArePlatformsCompatible(GMaxRHIShaderPlatform, (EShaderPlatform)Target.Platform))
 	{
 		if (FPlatformProperties::RequiresCookedData())
 		{
 			UE_LOG(LogShaders, Fatal, TEXT("FShaderResource::InitRHI got platform %s but it is not compatible with %s"), 
				*LegacyShaderPlatformToShaderFormat((EShaderPlatform)Target.Platform).ToString(), *LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString());
 		}
 		return;
 	}

	INC_DWORD_STAT_BY(STAT_Shaders_NumShadersUsedForRendering, 1);
	SCOPE_CYCLE_COUNTER(STAT_Shaders_RTShaderLoadTime);

	FShaderCache* ShaderCache = FShaderCache::GetShaderCache();

	if(Target.Frequency == SF_Vertex)
	{
		VertexShader = ShaderCache ? ShaderCache->GetVertexShader((EShaderPlatform)Target.Platform, OutputHash, Code) : RHICreateVertexShader(Code);
	}
	else if(Target.Frequency == SF_Pixel)
	{
		PixelShader = ShaderCache ? ShaderCache->GetPixelShader((EShaderPlatform)Target.Platform, OutputHash, Code) : RHICreatePixelShader(Code);
	}
	else if(Target.Frequency == SF_Hull)
	{
		HullShader = ShaderCache ? ShaderCache->GetHullShader((EShaderPlatform)Target.Platform, OutputHash, Code) : RHICreateHullShader(Code);
	}
	else if(Target.Frequency == SF_Domain)
	{
		DomainShader = ShaderCache ? ShaderCache->GetDomainShader((EShaderPlatform)Target.Platform, OutputHash, Code) : RHICreateDomainShader(Code);
	}
	else if(Target.Frequency == SF_Geometry)
	{
		if (SpecificType)
		{
			FStreamOutElementList ElementList;
			TArray<uint32> StreamStrides;
			int32 RasterizedStream = -1;
			SpecificType->GetStreamOutElements(ElementList, StreamStrides, RasterizedStream);
			checkf(ElementList.Num(), *FString::Printf(TEXT("Shader type %s was given GetStreamOutElements implementation that had no elements!"), SpecificType->GetName()));

			//@todo - not using the cache
			GeometryShader = RHICreateGeometryShaderWithStreamOutput(Code, ElementList, StreamStrides.Num(), StreamStrides.GetData(), RasterizedStream);
		}
		else
		{
			GeometryShader = ShaderCache ? ShaderCache->GetGeometryShader((EShaderPlatform)Target.Platform, OutputHash, Code) : RHICreateGeometryShader(Code);
		}
	}
	else if(Target.Frequency == SF_Compute)
	{
		ComputeShader = ShaderCache ? ShaderCache->GetComputeShader((EShaderPlatform)Target.Platform, Code) : RHICreateComputeShader(Code);
	}

	if (Target.Frequency != SF_Geometry)
	{
		checkf(!SpecificType, *FString::Printf(TEXT("Only geometry shaders can use GetStreamOutElements, shader type %s"), SpecificType->GetName()));
	}

	if (!FPlatformProperties::HasEditorOnlyData())
	{
		DEC_DWORD_STAT_BY_FName(GetMemoryStatType((EShaderFrequency)Target.Frequency).GetName(), Code.Num());
		DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, Code.GetAllocatedSize());
		Code.Empty();
	}
}


void FShaderResource::ReleaseRHI()
{
	DEC_DWORD_STAT_BY(STAT_Shaders_NumShadersUsedForRendering, 1);

	VertexShader.SafeRelease();
	PixelShader.SafeRelease();
	HullShader.SafeRelease();
	DomainShader.SafeRelease();
	GeometryShader.SafeRelease();
	ComputeShader.SafeRelease();
}

void FShaderResource::InitializeVertexShaderRHI() 
{ 
	if (!IsInitialized())
	{
		STAT(double ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_Shaders_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);

			InitResourceFromPossiblyParallelRendering();

		}

		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime,(float)ShaderInitializationTime);
	}

	checkSlow(IsInitialized());
}


void FShaderResource::InitializePixelShaderRHI() 
{ 
	if (!IsInitialized())
	{
		STAT(double ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_Shaders_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);

			InitResourceFromPossiblyParallelRendering();
		}

		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime,(float)ShaderInitializationTime);
	}

	checkSlow(IsInitialized());
}


const FHullShaderRHIRef& FShaderResource::GetHullShader() 
{ 
	checkSlow(Target.Frequency == SF_Hull);
	if (!IsInitialized())
	{
		STAT(double ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_Shaders_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);

			InitResourceFromPossiblyParallelRendering();
		}

		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime,(float)ShaderInitializationTime);
	}

	checkSlow(IsInitialized());

	return HullShader; 
}


const FDomainShaderRHIRef& FShaderResource::GetDomainShader() 
{ 
	checkSlow(Target.Frequency == SF_Domain);

	if (!IsInitialized())
	{
		STAT(double ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_Shaders_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);

			InitResourceFromPossiblyParallelRendering();
		}
		
		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime,(float)ShaderInitializationTime);
	}
	
	checkSlow(IsInitialized());

	return DomainShader; 
}


const FGeometryShaderRHIRef& FShaderResource::GetGeometryShader() 
{ 
	checkSlow(Target.Frequency == SF_Geometry);

	if (!IsInitialized())
	{
		STAT(double ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_Shaders_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);

			InitResourceFromPossiblyParallelRendering();
		}

		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime,(float)ShaderInitializationTime);
	}

	checkSlow(IsInitialized());

	return GeometryShader; 
}


const FComputeShaderRHIRef& FShaderResource::GetComputeShader() 
{ 
	checkSlow(Target.Frequency == SF_Compute);

	if (!IsInitialized())
	{
		STAT(double ShaderInitializationTime = 0);
		{
			SCOPE_CYCLE_COUNTER(STAT_Shaders_FrameRTShaderInitForRenderingTime);
			SCOPE_SECONDS_COUNTER(ShaderInitializationTime);

			InitResourceFromPossiblyParallelRendering();
		}

		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime,(float)ShaderInitializationTime);
	}

	checkSlow(IsInitialized());

	return ComputeShader; 
}

FShaderResourceId FShaderResource::GetId() const
{
	FShaderResourceId ShaderId;
	ShaderId.Target = Target;
	ShaderId.OutputHash = OutputHash;
	ShaderId.SpecificShaderTypeName = SpecificType ? SpecificType->GetName() : NULL;
	return ShaderId;
}

FShaderId::FShaderId(const FSHAHash& InMaterialShaderMapHash, FVertexFactoryType* InVertexFactoryType, FShaderType* InShaderType, FShaderTarget InTarget)
	: MaterialShaderMapHash(InMaterialShaderMapHash)
	, ShaderType(InShaderType)
	, SourceHash(InShaderType->GetSourceHash())
	, SerializationHistory(InShaderType->GetSerializationHistory())
	, Target(InTarget)
{
	if (InVertexFactoryType)
	{
		VFSerializationHistory = InVertexFactoryType->GetSerializationHistory((EShaderFrequency)InTarget.Frequency);
		VertexFactoryType = InVertexFactoryType;
		VFSourceHash = InVertexFactoryType->GetSourceHash();
	}
	else
	{
		VFSerializationHistory = NULL;
		VertexFactoryType = NULL;
	}
}

FSelfContainedShaderId::FSelfContainedShaderId() :
	Target(FShaderTarget(SF_NumFrequencies, SP_NumPlatforms))
{}

FSelfContainedShaderId::FSelfContainedShaderId(const FShaderId& InShaderId)
{
	MaterialShaderMapHash = InShaderId.MaterialShaderMapHash;
	VertexFactoryTypeName = InShaderId.VertexFactoryType ? InShaderId.VertexFactoryType->GetName() : TEXT("");
	VFSourceHash = InShaderId.VFSourceHash;
	VFSerializationHistory = InShaderId.VFSerializationHistory ? *InShaderId.VFSerializationHistory : FSerializationHistory();
	ShaderTypeName = InShaderId.ShaderType->GetName();
	SourceHash = InShaderId.SourceHash;
	SerializationHistory = InShaderId.SerializationHistory;
	Target = InShaderId.Target;
}

bool FSelfContainedShaderId::IsValid()
{
	FShaderType** TypePtr = FShaderType::GetNameToTypeMap().Find(FName(*ShaderTypeName));

	if (TypePtr && SourceHash == (*TypePtr)->GetSourceHash() && SerializationHistory == (*TypePtr)->GetSerializationHistory())
	{
		FVertexFactoryType* VFTypePtr = FVertexFactoryType::GetVFByName(VertexFactoryTypeName);

		if (VertexFactoryTypeName == TEXT("") 
			|| (VFTypePtr && VFSourceHash == VFTypePtr->GetSourceHash() && VFSerializationHistory == *VFTypePtr->GetSerializationHistory((EShaderFrequency)Target.Frequency)))
		{
			return true;
		}
	}

	return false;
}

FArchive& operator<<(FArchive& Ar,class FSelfContainedShaderId& Ref)
{
	Ar << Ref.MaterialShaderMapHash 
		<< Ref.VertexFactoryTypeName
		<< Ref.VFSourceHash
		<< Ref.VFSerializationHistory
		<< Ref.ShaderTypeName
		<< Ref.SourceHash
		<< Ref.SerializationHistory
		<< Ref.Target;

	return Ar;
}

/** 
 * Used to construct a shader for deserialization.
 * This still needs to initialize members to safe values since FShaderType::GenerateSerializationHistory uses this constructor.
 */
FShader::FShader() : 
	VFType(NULL),
	Type(NULL), 
	NumRefs(0),
	SetParametersId(0),
	Canary(ShaderMagic_Uninitialized)
{
	// set to undefined (currently shared with SF_Vertex)
	Target.Frequency = 0;
	Target.Platform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];
}

/**
 * Construct a shader from shader compiler output.
 */
FShader::FShader(const CompiledShaderInitializerType& Initializer):
	MaterialShaderMapHash(Initializer.MaterialShaderMapHash),
	VFType(Initializer.VertexFactoryType),
	Type(Initializer.Type),
	Target(Initializer.Target),
	NumRefs(0),
	SetParametersId(0),
	Canary(ShaderMagic_Initialized)
{
	OutputHash = Initializer.OutputHash;
	checkSlow(OutputHash != FSHAHash());

	check(Type);

	// Store off the source hash that this shader was compiled with
	// This will be used as part of the shader key in order to identify when shader files have been changed and a recompile is needed
	SourceHash = Type->GetSourceHash();

	if (VFType)
	{
		// Store off the VF source hash that this shader was compiled with
		VFSourceHash = VFType->GetSourceHash();
	}

	// Bind uniform buffer parameters automatically 
	for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
	{
		if (Initializer.ParameterMap.ContainsParameterAllocation(StructIt->GetShaderVariableName()))
		{
			UniformBufferParameterStructs.Add(*StructIt);
			UniformBufferParameters.Add(StructIt->ConstructTypedParameter());
			FShaderUniformBufferParameter* Parameter = UniformBufferParameters.Last();
			Parameter->Bind(Initializer.ParameterMap, StructIt->GetShaderVariableName(), SPF_Mandatory);
		}
	}

	SetResource(Initializer.Resource);

	// Register the shader now that it is valid, so that it can be reused
	Register();
}


FShader::~FShader()
{
	check(Canary == ShaderMagic_Uninitialized || Canary == ShaderMagic_CleaningUp || Canary == ShaderMagic_Initialized);
	check(NumRefs == 0);
	Canary = 0;

	for (int32 StructIndex = 0; StructIndex < UniformBufferParameters.Num(); StructIndex++)
	{
		delete UniformBufferParameters[StructIndex];
	}
}


const FSHAHash& FShader::GetHash() const 
{ 
	return SourceHash;
}


bool FShader::SerializeBase(FArchive& Ar, bool bShadersInline)
{
	Serialize(Ar);

	Ar << OutputHash;
	Ar << MaterialShaderMapHash;
	Ar << VFType;
	Ar << VFSourceHash;
	Ar << Type;
	Ar << SourceHash;
	Ar << Target;

	if (Ar.IsLoading())
	{
		int32 NumUniformParameters;
		Ar << NumUniformParameters;

		for (int32 ParameterIndex = 0; ParameterIndex < NumUniformParameters; ParameterIndex++)
		{
			FString StructName;
			Ar << StructName;

			FUniformBufferStruct* Struct = FindUniformBufferStructByName(*StructName);
			FShaderUniformBufferParameter* Parameter = Struct ? Struct->ConstructTypedParameter() : new FShaderUniformBufferParameter();

			Ar << *Parameter;

			UniformBufferParameterStructs.Add(Struct);
			UniformBufferParameters.Add(Parameter);
		}

		// The shader has been serialized in, so this shader is now initialized.
		check(Canary != ShaderMagic_CleaningUp);
		Canary = ShaderMagic_Initialized;
	}
	else
	{
		int32 NumUniformParameters = UniformBufferParameters.Num();
		Ar << NumUniformParameters;

		for (int32 StructIndex = 0; StructIndex < UniformBufferParameters.Num(); StructIndex++)
		{
			FString StructName(UniformBufferParameterStructs[StructIndex]->GetStructTypeName());
			Ar << StructName;
			Ar << *UniformBufferParameters[StructIndex];
		}
	}

	if (bShadersInline)
	{
		// Save the shader resource if we are inlining shaders
		if (Ar.IsSaving())
		{
			Resource->Serialize(Ar);
		}

		if (Ar.IsLoading())
		{
			// Load the inlined shader resource
			FShaderResource* ShaderResource = new FShaderResource();
			ShaderResource->Serialize(Ar);

			TRefCountPtr<FShaderResource> ExistingResource = FShaderResource::FindShaderResourceById(ShaderResource->GetId());

			// Reuse an existing shader resource if a matching one already exists in memory
			if (ExistingResource)
			{
				delete ShaderResource;
				ShaderResource = ExistingResource;
			}
			else
			{
				// Register the newly loaded shader resource so it can be reused by other shaders
				ShaderResource->Register();
			}
			
			SetResource(ShaderResource);
		}
	}
	else
	{
		// if saving, there's nothing to, the required data is already saved above to look it up at load time
		if (Ar.IsLoading())
		{
			// generate a resource id
			FShaderResourceId ResourceId;
			ResourceId.Target = Target;
			ResourceId.OutputHash = OutputHash;

			// use it to look up in the registered resource map
			TRefCountPtr<FShaderResource> ExistingResource = FShaderResource::FindShaderResourceById(ResourceId);
			SetResource(ExistingResource);
		}
	}

	return false;
}

void FShader::AddRef()
{
	check(Canary != ShaderMagic_CleaningUp);
	// Lock shader Id maps
	LockShaderIdMap();
	++NumRefs;
	if (NumRefs == 1)
	{
		INC_DWORD_STAT_BY(STAT_Shaders_ShaderMemory, GetSizeBytes());
		INC_DWORD_STAT_BY(STAT_Shaders_NumShadersLoaded,1);
	}
	UnlockShaderIdMap();
}


void FShader::Release()
{
	// Lock the shader id map. Note that we don't necessarily have to deregister at this point but
	// the shader id map has to be locked while we remove references to this shader so that nothing
	// can find the shader in the map after we remove the final reference but before we deregister the shader
	LockShaderIdMap();
	if(--NumRefs == 0)
	{
		DEC_DWORD_STAT_BY(STAT_Shaders_ShaderMemory, GetSizeBytes());
		DEC_DWORD_STAT_BY(STAT_Shaders_NumShadersLoaded,1);

		// Deregister the shader now to eliminate references to it by the type's ShaderIdMap
		Deregister();

		Canary = ShaderMagic_CleaningUp;
		BeginCleanup(this);
	}
	UnlockShaderIdMap();
}


void FShader::Register()
{
	FShaderId ShaderId = GetId();
	check(ShaderId.MaterialShaderMapHash != FSHAHash());
	check(ShaderId.SourceHash != FSHAHash());
	check(Resource);
	Type->AddToShaderIdMap(ShaderId, this);
}

void FShader::LockShaderIdMap()
{
	Type->LockShaderIdMap();
}

void FShader::Deregister()
{
	Type->RemoveFromShaderIdMap(GetId());
}

void FShader::UnlockShaderIdMap()
{
	Type->UnlockShaderIdMap();
}

FShaderId FShader::GetId() const
{
	FShaderId ShaderId(Type->GetSerializationHistory());
	ShaderId.MaterialShaderMapHash = MaterialShaderMapHash;
	ShaderId.VertexFactoryType = VFType;
	ShaderId.VFSourceHash = VFSourceHash;
	ShaderId.VFSerializationHistory = VFType ? VFType->GetSerializationHistory((EShaderFrequency)GetTarget().Frequency) : NULL;
	ShaderId.ShaderType = Type;
	ShaderId.SourceHash = SourceHash;
	ShaderId.Target = Target;
	return ShaderId;
}


void FShader::SetResource(FShaderResource* InResource)
{
	check(InResource && InResource->Target == Target);
	Resource = InResource;
}


void FShader::FinishCleanup()
{
	delete this;
}

void FShader::VerifyBoundUniformBufferParameters()
{
	// Support being called on a NULL pointer
// TODO: doesn't work with uniform buffer parameters on helper structs like FDeferredPixelShaderParameters
	//@todo parallelrendering
	if (0)//&&this)
	{
		for (int32 StructIndex = 0; StructIndex < UniformBufferParameters.Num(); StructIndex++)
		{
			const FShaderUniformBufferParameter& UniformParameter = *UniformBufferParameters[StructIndex];

			if (UniformParameter.SetParametersId != SetParametersId)
			{
				// Log an error when a shader was used for rendering but did not have all of its uniform buffers set
				// This can have false positives, for example when sharing state between draw calls with the same shader, the SetParametersId logic will break down
				// Also if the uniform buffer is compiled into the shader but not actually used due to control flow, failing to set that parameter will cause this error
				UE_LOG(LogShaders, Error, TEXT("Automatically bound uniform buffer parameter %s %s was not set before used for rendering in shader %s!"), 
					UniformBufferParameterStructs[StructIndex]->GetStructTypeName(), 
					UniformBufferParameterStructs[StructIndex]->GetShaderVariableName(),
					GetType()->GetName());
			}
		}

		SetParametersId++;
	}
}


void DumpShaderStats( EShaderPlatform Platform, EShaderFrequency Frequency )
{
#if ALLOW_DEBUG_FILES
	FDiagnosticTableViewer ShaderTypeViewer(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("ShaderStats")));

	// Iterate over all shader types and log stats.
	int32 TotalShaderCount		= 0;
	int32 TotalTypeCount			= 0;
	int32 TotalInstructionCount	= 0;
	int32 TotalSize				= 0;
	float TotalSizePerType		= 0;

	// Write a row of headings for the table's columns.
	ShaderTypeViewer.AddColumn(TEXT("Type"));
	ShaderTypeViewer.AddColumn(TEXT("Instances"));
	ShaderTypeViewer.AddColumn(TEXT("Average instructions"));
	ShaderTypeViewer.AddColumn(TEXT("Size"));
	ShaderTypeViewer.AddColumn(TEXT("AvgSizePerInstance"));
	ShaderTypeViewer.CycleRow();

	for( TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next() )
	{
		const FShaderType* Type = *It;
		if(Type->GetNumShaders())
		{
			// Calculate the average instruction count and total size of instances of this shader type.
			float AverageNumInstructions	= 0.0f;
			int32 NumInitializedInstructions	= 0;
			int32 Size						= 0;
			int32 NumShaders					= 0;
			for(TMap<FShaderId,FShader*>::TConstIterator ShaderIt(Type->ShaderIdMap);ShaderIt;++ShaderIt)
			{
				const FShader* Shader = ShaderIt.Value();
				// Skip shaders that don't match frequency.
				if( Shader->GetTarget().Frequency != Frequency && Frequency != SF_NumFrequencies )
				{
					continue;
				}
				// Skip shaders that don't match platform.
				if( Shader->GetTarget().Platform != Platform && Platform != SP_NumPlatforms )
				{
					continue;
				}
				NumInitializedInstructions += Shader->GetNumInstructions();
				Size += Shader->GetCode().Num();
				NumShaders++;
			}
			AverageNumInstructions = (float)NumInitializedInstructions / (float)Type->GetNumShaders();
			
			// Only add rows if there is a matching shader.
			if( NumShaders )
			{
				// Write a row for the shader type.
				ShaderTypeViewer.AddColumn(Type->GetName());
				ShaderTypeViewer.AddColumn(TEXT("%u"),NumShaders);
				ShaderTypeViewer.AddColumn(TEXT("%.1f"),AverageNumInstructions);
				ShaderTypeViewer.AddColumn(TEXT("%u"),Size);
				ShaderTypeViewer.AddColumn(TEXT("%.1f"),Size / (float)NumShaders);
				ShaderTypeViewer.CycleRow();

				TotalShaderCount += NumShaders;
				TotalInstructionCount += NumInitializedInstructions;
				TotalTypeCount++;
				TotalSize += Size;
				TotalSizePerType += Size / (float)NumShaders;
			}
		}
	}

	// Write a total row.
	ShaderTypeViewer.AddColumn(TEXT("Total"));
	ShaderTypeViewer.AddColumn(TEXT("%u"),TotalShaderCount);
	ShaderTypeViewer.AddColumn(TEXT("%u"),TotalInstructionCount);
	ShaderTypeViewer.AddColumn(TEXT("%u"),TotalSize);
	ShaderTypeViewer.AddColumn(TEXT("0"));
	ShaderTypeViewer.CycleRow();

	// Write an average row.
	ShaderTypeViewer.AddColumn(TEXT("Average"));
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),TotalShaderCount / (float)TotalTypeCount);
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),(float)TotalInstructionCount / TotalShaderCount);
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),TotalSize / (float)TotalShaderCount);
	ShaderTypeViewer.AddColumn(TEXT("%.1f"),TotalSizePerType / TotalTypeCount);
	ShaderTypeViewer.CycleRow();
#endif
}


FShaderType* FindShaderTypeByName(const TCHAR* ShaderTypeName)
{
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		if(!FCString::Stricmp(ShaderTypeIt->GetName(),ShaderTypeName))
		{
			return *ShaderTypeIt;
		}
	}
	return NULL;
}


void DispatchComputeShader(
	FRHICommandList& RHICmdList,
	FShader* Shader,
	uint32 ThreadGroupCountX,
	uint32 ThreadGroupCountY,
	uint32 ThreadGroupCountZ)
{
	Shader->VerifyBoundUniformBufferParameters();
	RHICmdList.DispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}


void DispatchIndirectComputeShader(
	FRHICommandList& RHICmdList,
	FShader* Shader,
	FVertexBufferRHIParamRef ArgumentBuffer,
	uint32 ArgumentOffset)
{
	Shader->VerifyBoundUniformBufferParameters();
	RHICmdList.DispatchIndirectComputeShader(ArgumentBuffer, ArgumentOffset);
}


const TArray<FName>& GetTargetShaderFormats()
{
	static bool bInit = false;
	static TArray<FName> Results;

#if WITH_ENGINE

	if (!bInit)
	{
		bInit = true;
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		if (!TPM || TPM->RestrictFormatsToRuntimeOnly())
		{
			// for now a runtime format and a cook format are very different, we don't put any formats here
		}
		else
		{
			const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				Platforms[Index]->GetAllTargetedShaderFormats(Results);
			}
		}
	}

#endif // WITH_ENGINE

	return Results;
}

void ShaderMapAppendKeyString(EShaderPlatform Platform, FString& KeyString)
{
	// Globals that should cause all shaders to recompile when changed must be appended to the key here
	// Key should be kept as short as possible while being somewhat human readable for debugging

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Compat.UseDXT5NormalMaps"));
		KeyString += (CVar && CVar->GetValueOnAnyThread() != 0) ? TEXT("_DXTN") : TEXT("_BC5N");
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CompileShadersForDevelopment"));
		KeyString += (CVar && CVar->GetValueOnAnyThread() != 0) ? TEXT("_DEV") : TEXT("_NoDEV");
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bValue = CVar ? CVar->GetValueOnAnyThread() != 0 : true;
		KeyString += bValue ? TEXT("_SL") : TEXT("_NoSL");
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BasePassOutputsVelocity"));
		if (CVar && CVar->GetValueOnGameThread() != 0)
		{
			KeyString += TEXT("_GV");
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GBuffer"));
		if (CVar ? CVar->GetValueOnAnyThread() == 0 : false)
		{
			KeyString += TEXT("_NoGB");
		}
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
		KeyString += (CVar && CVar->GetInt() != 0) ? TEXT("_DBuf") : TEXT("_NoDBuf");
	}

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.KeepDebugInfo"));
		KeyString += (CVar && CVar->GetInt() != 0) ? TEXT("_NoStrip") : TEXT("");
	}

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
		KeyString += (CVar && CVar->GetInt() != 0) ? TEXT("") : TEXT("_NoOpt");
	}

	if( Platform == SP_PS4 )
	{
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4MixedModeShaderDebugInfo"));
			if (CVar && CVar->GetValueOnAnyThread() != 0)
			{
				KeyString += TEXT("_MMDBG");
			}
		}

		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4DumpShaderSDB"));
			if (CVar && CVar->GetValueOnAnyThread() != 0)
			{
				KeyString += TEXT("_SDB");
			}
		}
	}
}

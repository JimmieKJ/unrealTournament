// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLShaders.cpp: OpenGL shader RHI implementation.
=============================================================================*/
 
#include "OpenGLDrvPrivate.h"
#include "Shader.h"
#include "GlobalShader.h"

#define CHECK_FOR_GL_SHADERS_TO_REPLACE 0

#if PLATFORM_WINDOWS
#include <mmintrin.h>
#elif PLATFORM_MAC
#include <xmmintrin.h>
#endif

const uint32 SizeOfFloat4 = 16;
const uint32 NumFloatsInFloat4 = 4;

FORCEINLINE void FOpenGLShaderParameterCache::FRange::MarkDirtyRange(uint32 NewStartVector, uint32 NewNumVectors)
{
	if (NumVectors > 0)
	{
		uint32 High = StartVector + NumVectors;
		uint32 NewHigh = NewStartVector + NewNumVectors;
		
		uint32 MaxVector = FMath::Max(High, NewHigh);
		uint32 MinVector = FMath::Min(StartVector, NewStartVector);
		
		StartVector = MinVector;
		NumVectors = (MaxVector - MinVector) + 1;
	}
	else
	{
		StartVector = NewStartVector;
		NumVectors = NewNumVectors;
	}
}

/**
 * Verify that an OpenGL shader has compiled successfully. 
 */
static bool VerifyCompiledShader(GLuint Shader, const ANSICHAR* GlslCode )
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileVerifyTime);

	GLint CompileStatus;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);
	if (CompileStatus != GL_TRUE)
	{
		GLint LogLength;
		ANSICHAR DefaultLog[] = "No log";
		ANSICHAR *CompileLog = DefaultLog;
		glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
#if PLATFORM_ANDROID
		if ( LogLength == 0 )
		{
			// make it big anyway
			// there was a bug in android 2.2 where glGetShaderiv would return 0 even though there was a error message
			// https://code.google.com/p/android/issues/detail?id=9953
			LogLength = 4096;
		}
#endif
		if (LogLength > 1)
		{
			CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
			glGetShaderInfoLog(Shader, LogLength, NULL, CompileLog);
		}

#if DEBUG_GL_SHADERS
		if (GlslCode)
		{
			UE_LOG(LogRHI,Error,TEXT("Shader:\n%s"),ANSI_TO_TCHAR(GlslCode));

#if 0
			const ANSICHAR *Temp = GlslCode;

			for ( int i = 0; i < 30 && (*Temp != '\0'); ++i )
			{
				FString Converted = ANSI_TO_TCHAR( Temp );
				Converted.LeftChop( 256 );

				UE_LOG(LogRHI,Display,TEXT("%s"), *Converted );
				Temp += Converted.Len();
			}
#endif
		}	
#endif
		UE_LOG(LogRHI,Fatal,TEXT("Failed to compile shader. Compile log:\n%s"), ANSI_TO_TCHAR(CompileLog));

		if (LogLength > 1)
		{
			FMemory::Free(CompileLog);
		}
		return false;
	}
	return true;
}

/**
 * Verify that an OpenGL program has linked successfully.
 */
static bool VerifyLinkedProgram(GLuint Program)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLinkVerifyTime);

	GLint LinkStatus = 0;
	glGetProgramiv(Program, GL_LINK_STATUS, &LinkStatus);
	if (LinkStatus != GL_TRUE)
	{
		GLint LogLength;
		ANSICHAR DefaultLog[] = "No log";
		ANSICHAR *CompileLog = DefaultLog;
		glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
		if (LogLength > 1)
		{
			CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
			glGetProgramInfoLog(Program, LogLength, NULL, CompileLog);
		}

		UE_LOG(LogRHI,Error,TEXT("Failed to link program. Compile log:\n%s"),
			ANSI_TO_TCHAR(CompileLog));

		if (LogLength > 1)
		{
			FMemory::Free(CompileLog);
		}
		return false;
	}
	return true;
}

// ============================================================================================================================

class FOpenGLCompiledShaderKey
{
public:
	FOpenGLCompiledShaderKey(
		GLenum InTypeEnum,
		uint32 InCodeSize,
		uint32 InCodeCRC
		)
		: TypeEnum(InTypeEnum)
		, CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
	{}

	friend bool operator ==(const FOpenGLCompiledShaderKey& A,const FOpenGLCompiledShaderKey& B)
	{
		return A.TypeEnum == B.TypeEnum && A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC;
	}

	friend uint32 GetTypeHash(const FOpenGLCompiledShaderKey &Key)
	{
		return GetTypeHash(Key.TypeEnum) ^ GetTypeHash(Key.CodeSize) ^ GetTypeHash(Key.CodeCRC);
	}

private:
	GLenum TypeEnum;
	uint32 CodeSize;
	uint32 CodeCRC;
};

typedef TMap<FOpenGLCompiledShaderKey,GLuint> FOpenGLCompiledShaderCache;

static FOpenGLCompiledShaderCache& GetOpenGLCompiledShaderCache()
{
	static FOpenGLCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

// ============================================================================================================================


static const TCHAR* ShaderNameFromShaderType(GLenum ShaderType)
{
	switch(ShaderType)
	{
		case GL_VERTEX_SHADER: return TEXT("vertex");
		case GL_FRAGMENT_SHADER: return TEXT("fragment");
		case GL_GEOMETRY_SHADER: return TEXT("geometry");
		case GL_TESS_CONTROL_SHADER: return TEXT("hull");
		case GL_TESS_EVALUATION_SHADER: return TEXT("domain");
		case GL_COMPUTE_SHADER: return TEXT("compute");
		default: return NULL;
	}
}

// ============================================================================================================================

namespace
{
	inline void AppendCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source)
	{
		if (Dest.Num() > 0)
		{
			Dest.Insert(Source, FCStringAnsi::Strlen(Source), Dest.Num() - 1);;
		}
		else
		{
			Dest.Append(Source, FCStringAnsi::Strlen(Source) + 1);
		}
	}

	inline void ReplaceCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source, const ANSICHAR * Replacement)
	{
		int32 SourceLen = FCStringAnsi::Strlen(Source);
		int32 ReplacementLen = FCStringAnsi::Strlen(Replacement);
		int32 FoundIndex = 0;
		for (const ANSICHAR * FoundPointer = FCStringAnsi::Strstr(Dest.GetData(), Source);
			nullptr != FoundPointer;
			FoundPointer = FCStringAnsi::Strstr(Dest.GetData()+FoundIndex, Source))
		{
			FoundIndex = FoundPointer - Dest.GetData();
			Dest.RemoveAt(FoundIndex, SourceLen);
			Dest.Insert(Replacement, ReplacementLen, FoundIndex);
		}
	}

	inline const ANSICHAR * CStringEndOfLine(const ANSICHAR * Text)
	{
		const ANSICHAR * LineEnd = FCStringAnsi::Strchr(Text, '\n');
		if (nullptr == LineEnd)
		{
			LineEnd = Text + FCStringAnsi::Strlen(Text);
		}
		return LineEnd;
	}

	inline bool CStringIsBlankLine(const ANSICHAR * Text)
	{
		while (!FCharAnsi::IsLinebreak(*Text))
		{
			if (!FCharAnsi::IsWhitespace(*Text))
			{
				return false;
			}
			++Text;
		}
		return true;
	}

	inline bool MoveHashLines(TArray<ANSICHAR> & Dest, TArray<ANSICHAR> & Source)
	{
		// Walk through the lines to find the first non-# line...
		const ANSICHAR * LineStart = Source.GetData();
		for (bool FoundNonHashLine = false; !FoundNonHashLine;)
		{
			const ANSICHAR * LineEnd = CStringEndOfLine(LineStart);
			if (LineStart[0] != '#' && !CStringIsBlankLine(LineStart))
			{
				FoundNonHashLine = true;
			}
			else if (LineEnd[0] == '\n')
			{
				LineStart = LineEnd + 1;
			}
			else
			{
				LineStart = LineEnd;
			}
		}
		// Copy the hash lines over, if we found any. And delete from
		// the source.
		if (LineStart > Source.GetData())
		{
			int32 LineLength = LineStart - Source.GetData();
			if (Dest.Num() > 0)
			{
				Dest.Insert(Source.GetData(), LineLength, Dest.Num() - 1);
			}
			else
			{
				Dest.Append(Source.GetData(), LineLength);
				Dest.Append("", 1);
			}
			if (Dest.Last(1) != '\n')
			{
				Dest.Insert("\n", 1, Dest.Num() - 1);
			}
			Source.RemoveAt(0, LineStart - Source.GetData());
			return true;
		}
		return false;
	}
}

/**
 * Compiles an OpenGL shader using the given GLSL microcode.
 * @returns the compiled shader upon success.
 */
template <typename ShaderType>
ShaderType* CompileOpenGLShader(const TArray<uint8>& Code)
{
	typedef TArray<ANSICHAR> FAnsiCharArray;

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileTime);
	VERIFY_GL_SCOPE();

	ShaderType* Shader = nullptr;
	const GLenum TypeEnum = ShaderType::TypeEnum;
	FMemoryReader Ar(Code, true);
	FOpenGLCodeHeader Header = { 0 };

	Ar << Header;
	// Suppress static code analysis warning about a potential comparison of two constants
	CA_SUPPRESS(6326);
	if (Header.GlslMarker != 0x474c534c
		|| (TypeEnum == GL_VERTEX_SHADER && Header.FrequencyMarker != 0x5653)
		|| (TypeEnum == GL_FRAGMENT_SHADER && Header.FrequencyMarker != 0x5053)
		|| (TypeEnum == GL_GEOMETRY_SHADER && Header.FrequencyMarker != 0x4753)
		|| (TypeEnum == GL_COMPUTE_SHADER && Header.FrequencyMarker != 0x4353 && FOpenGL::SupportsComputeShaders())
		|| (TypeEnum == GL_TESS_CONTROL_SHADER && Header.FrequencyMarker != 0x4853 && FOpenGL::SupportsTessellation()) /* hull shader*/
		|| (TypeEnum == GL_TESS_EVALUATION_SHADER && Header.FrequencyMarker != 0x4453 && FOpenGL::SupportsTessellation()) /* domain shader*/
		)
	{
		UE_LOG(LogRHI,Fatal,
			TEXT("Corrupt shader bytecode. GlslMarker=0x%08x FrequencyMarker=0x%04x"),
			Header.GlslMarker,
			Header.FrequencyMarker
			);
		return nullptr;
	}

	int32 CodeOffset = Ar.Tell();

	// The code as given to us.
	FAnsiCharArray GlslCodeOriginal;
	AppendCString(GlslCodeOriginal, (ANSICHAR*)Code.GetData() + CodeOffset);
	uint32 GlslCodeOriginalCRC = FCrc::MemCrc_DEPRECATED(GlslCodeOriginal.GetData(), GlslCodeOriginal.Num() + 1);

	// The amended code we actually compile.
	FAnsiCharArray GlslCode;

	// Find the existing compiled shader in the cache.
	FOpenGLCompiledShaderKey Key(TypeEnum, GlslCodeOriginal.Num(), GlslCodeOriginalCRC);
	GLuint Resource = GetOpenGLCompiledShaderCache().FindRef(Key);
	if (!Resource)
	{
#if CHECK_FOR_GL_SHADERS_TO_REPLACE
		{
			// 1. Check for specific file
			FString PotentialShaderFileName = FString::Printf(TEXT("%s-%d-0x%x.txt"), ShaderNameFromShaderType(TypeEnum), GlslCodeOriginal.Num(), GlslCodeOriginalCRC);
			FString PotentialShaderFile = FPaths::ProfilingDir();
			PotentialShaderFile *= PotentialShaderFileName;

			UE_LOG( LogRHI, Log, TEXT("Looking for shader file '%s' for potential replacement."), *PotentialShaderFileName );

			int64 FileSize = IFileManager::Get().FileSize(*PotentialShaderFile);
			if( FileSize > 0 )
			{
				FArchive* Ar = IFileManager::Get().CreateFileReader(*PotentialShaderFile);
				if( Ar != NULL )
				{
					UE_LOG(LogRHI, Log, TEXT("Replacing %s shader with length %d and CRC 0x%x with the one from a file."), (TypeEnum == GL_VERTEX_SHADER) ? TEXT("vertex") : ((TypeEnum == GL_FRAGMENT_SHADER) ? TEXT("fragment") : TEXT("geometry")), GlslCodeOriginal.Num(), GlslCodeOriginalCRC);

					// read in the file
					GlslCodeOriginal.Empty();
					GlslCodeOriginal.AddUninitialized(FileSize + 1);
					Ar->Serialize(GlslCodeOriginal.GetData(), FileSize);
					delete Ar;
					GlslCodeOriginal[FileSize] = 0;
				}
			}
		}
#endif

		Resource = glCreateShader(TypeEnum);

#if (PLATFORM_ANDROID || PLATFORM_HTML5)
		if (IsES2Platform(GMaxRHIShaderPlatform))
		{
			// #version NNN has to be the first line in the file, so it has to be added before anything else.
			if (FOpenGL::UseES30ShadingLanguage())
			{
				AppendCString(GlslCode, "#version 300 es\n");
			}
			else 
			{
				AppendCString(GlslCode, "#version 100\n");
			}
			ReplaceCString(GlslCodeOriginal, "#version 100", "");
		}
#endif

#if PLATFORM_ANDROID 
		if (IsES2Platform(GMaxRHIShaderPlatform))
		{
			// This #define fixes compiler errors on Android (which doesn't seem to support textureCubeLodEXT)
			if (FOpenGL::UseES30ShadingLanguage())
			{
				if (TypeEnum == GL_VERTEX_SHADER)
				{
					AppendCString(GlslCode,
						"#define texture2D texture \n"
						"#define texture2DProj textureProj \n"
						"#define texture2DLod textureLod \n"
						"#define texture2DProjLod textureProjLod \n"
						"#define textureCube texture \n"
						"#define textureCubeLod textureLod \n"
						"#define textureCubeLodEXT textureLod \n");

					ReplaceCString(GlslCodeOriginal, "attribute", "in");
					ReplaceCString(GlslCodeOriginal, "varying", "out");
				} 
				else if (TypeEnum == GL_FRAGMENT_SHADER)
				{
					// #extension directives have to come before any non-# directives. Because
					// we add non-# stuff below and the #extension directives
					// get added to the incoming shader source we move any # directives
					// to be right after the #version to ensure they are always correct.
					MoveHashLines(GlslCode, GlslCodeOriginal);

					AppendCString(GlslCode,
						"#define texture2D texture \n"
						"#define texture2DProj textureProj \n"
						"#define texture2DLod textureLod \n"
						"#define texture2DLodEXT textureLod \n"
						"#define texture2DProjLod textureProjLod \n"
						"#define textureCube texture \n"
						"#define textureCubeLod textureLod \n"
						"#define textureCubeLodEXT textureLod \n"
						"\n"
						"#define gl_FragColor out_FragColor \n"
						"out mediump vec4 out_FragColor; \n");

					ReplaceCString(GlslCodeOriginal, "varying", "in");
				}
			}
			else if ( (TypeEnum == GL_FRAGMENT_SHADER) &&
				FOpenGL::RequiresDontEmitPrecisionForTextureSamplers() )
			{
				// Daniel: This device has some shader compiler compatibility issues force them to be disabled
				//			The cross compiler will put the DONTEMITEXTENSIONSHADERTEXTURELODENABLE define around incompatible sections of code
				AppendCString(GlslCode,
					"#define DONTEMITEXTENSIONSHADERTEXTURELODENABLE \n"
					"#define DONTEMITSAMPLERDEFAULTPRECISION \n"
					"#define texture2DLodEXT(a, b, c) texture2D(a, b) \n"
					"#define textureCubeLodEXT(a, b, c) textureCube(a, b) \n");
			}
			else if ( ( TypeEnum == GL_FRAGMENT_SHADER) && 
				FOpenGL::RequiresTextureCubeLodEXTToTextureCubeLodDefine() )
			{
				AppendCString(GlslCode,
					"#define textureCubeLodEXT textureCubeLod \n");
			}
			else if(!FOpenGL::SupportsShaderTextureLod() || !FOpenGL::SupportsShaderTextureCubeLod())
			{
				AppendCString(GlslCode,
					"#define texture2DLodEXT(a, b, c) texture2D(a, b) \n"
					"#define textureCubeLodEXT(a, b, c) textureCube(a, b) \n");
			}
		}

#elif PLATFORM_HTML5

		// HTML5 use case is much simpler, use a separate chunk of code from android. 
		if (!FOpenGL::SupportsShaderTextureLod())
		{
			AppendCString(GlslCode,
				"#define DONTEMITEXTENSIONSHADERTEXTURELODENABLE \n"
				"#define texture2DLodEXT(a, b, c) texture2D(a, b) \n"
				"#define textureCubeLodEXT(a, b, c) textureCube(a, b) \n");
		}

#endif

		// Append the possibly edited shader to the one we will compile.
		// This is to make it easier to debug as we can see the whole
		// shader source.
		AppendCString(GlslCode, "\n\n");
		AppendCString(GlslCode, GlslCodeOriginal.GetData());

		const ANSICHAR * GlslCodeString = GlslCode.GetData();
		int32 GlslCodeLength = GlslCode.Num() - 1;
		glShaderSource(Resource, 1, (const GLchar**)&GlslCodeString, &GlslCodeLength);
		glCompileShader(Resource);

		GetOpenGLCompiledShaderCache().Add(Key,Resource);
	}

	Shader = new ShaderType();
	Shader->Resource = Resource;
	Shader->Bindings = Header.Bindings;
	Shader->UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;

#if DEBUG_GL_SHADERS
	Shader->GlslCode = GlslCode;
	Shader->GlslCodeString = (ANSICHAR*)Shader->GlslCode.GetData();
#endif

	return Shader;
}

/**
 * Helper for constructing strings of the form XXXXX##.
 * @param Str - The string to build.
 * @param Offset - Offset into the string at which to set the number.
 * @param Index - Number to set. Must be in the range [0,100).
 */
static ANSICHAR* SetIndex(ANSICHAR* Str, int32 Offset, int32 Index)
{
	check(Index >= 0 && Index < 100);

	Str += Offset;
	if (Index >= 10)
	{
		*Str++ = '0' + (ANSICHAR)(Index / 10);
	}
	*Str++ = '0' + (ANSICHAR)(Index % 10);
	*Str = '\0';
	return Str;
}

FVertexShaderRHIRef FOpenGLDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	return CompileOpenGLShader<FOpenGLVertexShader>(Code);
}

FPixelShaderRHIRef FOpenGLDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	return CompileOpenGLShader<FOpenGLPixelShader>(Code);
}

FGeometryShaderRHIRef FOpenGLDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code)
{
	return CompileOpenGLShader<FOpenGLGeometryShader>(Code);
}

FHullShaderRHIRef FOpenGLDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	return CompileOpenGLShader<FOpenGLHullShader>(Code);
}

FDomainShaderRHIRef FOpenGLDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	return CompileOpenGLShader<FOpenGLDomainShader>(Code);
}

FGeometryShaderRHIRef FOpenGLDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) 
{
	UE_LOG(LogRHI, Fatal,TEXT("OpenGL Render path does not support stream output!"));
	return NULL;
}


static void MarkShaderParameterCachesDirty(FOpenGLShaderParameterCache* ShaderParameters, bool UpdateCompute)
{
	const int32 StageStart = UpdateCompute  ? CrossCompiler::SHADER_STAGE_COMPUTE : CrossCompiler::SHADER_STAGE_VERTEX;
	const int32 StageEnd = UpdateCompute ? CrossCompiler::NUM_SHADER_STAGES : CrossCompiler::NUM_NON_COMPUTE_SHADER_STAGES;
	for (int32 Stage = StageStart; Stage < StageEnd; ++Stage)
	{
		ShaderParameters[Stage].MarkAllDirty();
	}
}

void FOpenGLDynamicRHI::BindUniformBufferBase(FOpenGLContextState& ContextState, int32 NumUniformBuffers, FUniformBufferRHIRef* BoundUniformBuffers, uint32 FirstUniformBuffer, bool ForceUpdate)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLUniformBindTime);
	checkSlow(IsInRenderingThread());
	for (int32 BufferIndex = 0; BufferIndex < NumUniformBuffers; ++BufferIndex)
	{
		GLuint Buffer = 0;
		uint32 Offset = 0;
		uint32 Size = ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE;
		int32 BindIndex = FirstUniformBuffer + BufferIndex;
		if (IsValidRef(BoundUniformBuffers[BufferIndex]))
		{
			FRHIUniformBuffer* UB = BoundUniformBuffers[BufferIndex].GetReference();
			Buffer = ((FOpenGLUniformBuffer*)UB)->Resource;
			Size = ((FOpenGLUniformBuffer*)UB)->GetSize();
#if SUBALLOCATED_CONSTANT_BUFFER
			Offset = ((FOpenGLUniformBuffer*)UB)->Offset;
#endif
		}
		else
		{
			if (PendingState.ZeroFilledDummyUniformBuffer == 0)
			{
				void* ZeroBuffer = FMemory::Malloc(ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
				FMemory::Memzero(ZeroBuffer,ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
				FOpenGL::GenBuffers(1, &PendingState.ZeroFilledDummyUniformBuffer);
				check(PendingState.ZeroFilledDummyUniformBuffer != 0);
				CachedBindUniformBuffer(ContextState,PendingState.ZeroFilledDummyUniformBuffer);
				glBufferData(GL_UNIFORM_BUFFER, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE, ZeroBuffer, GL_STATIC_DRAW);
				FMemory::Free(ZeroBuffer);
				IncrementBufferMemory(GL_UNIFORM_BUFFER, false, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
			}

			Buffer = PendingState.ZeroFilledDummyUniformBuffer;
		}

		if (ForceUpdate || (Buffer != 0 && ContextState.UniformBuffers[BindIndex] != Buffer)|| ContextState.UniformBufferOffsets[BindIndex] != Offset)
		{
			FOpenGL::BindBufferRange(GL_UNIFORM_BUFFER, BindIndex, Buffer, Offset, Size);
			ContextState.UniformBuffers[BindIndex] = Buffer;
			ContextState.UniformBufferOffsets[BindIndex] = Offset;
			ContextState.UniformBufferBound = Buffer;	// yes, calling glBindBufferRange also changes uniform buffer binding.
		}
	}
}

// ============================================================================================================================

class FOpenGLLinkedProgramConfiguration
{
public:

	struct ShaderInfo
	{
		FOpenGLShaderBindings Bindings;
		GLuint Resource;
	}
	Shaders[CrossCompiler::NUM_SHADER_STAGES];

	FOpenGLLinkedProgramConfiguration()
	{
		for ( int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Shaders[Stage].Resource = 0;
		}
	}

	friend bool operator ==(const FOpenGLLinkedProgramConfiguration& A,const FOpenGLLinkedProgramConfiguration& B)
	{
		bool bEqual = true;
		for ( int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES && bEqual; Stage++)
		{
			bEqual &= A.Shaders[Stage].Resource == B.Shaders[Stage].Resource;
			bEqual &= A.Shaders[Stage].Bindings == B.Shaders[Stage].Bindings;
		}
		return bEqual;
	}

	friend uint32 GetTypeHash(const FOpenGLLinkedProgramConfiguration &Config)
	{
		uint32 Hash = 0;
		for ( int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Hash ^= GetTypeHash(Config.Shaders[Stage].Bindings);
			Hash ^= Config.Shaders[Stage].Resource;
		}
		return Hash;
	}
};

class FOpenGLLinkedProgram
{
public:
	FOpenGLLinkedProgramConfiguration Config;

	struct FPackedUniformInfo
	{
		GLint	Location;
		uint8	ArrayType;	// OGL_PACKED_ARRAYINDEX_TYPE
		uint8	Index;		// OGL_PACKED_INDEX_TYPE
	};

	// Holds information needed per stage regarding packed uniform globals and uniform buffers
	struct FStagePackedUniformInfo
	{
		// Packed Uniform Arrays (regular globals); array elements per precision/type
		TArray<FPackedUniformInfo>			PackedUniformInfos;

		// Packed Uniform Buffers; outer array is per Uniform Buffer; inner array is per precision/type
		TArray<TArray<FPackedUniformInfo>>	PackedUniformBufferInfos;

		// Holds the unique ID of the last uniform buffer uploaded to the program; since we don't reuse uniform buffers
		// (can't modify existing ones), we use this as a check for dirty/need to mem copy on Mobile
		TArray<uint32>						LastEmulatedUniformBufferSet;
	};
	FStagePackedUniformInfo	StagePackedUniformInfo[CrossCompiler::NUM_SHADER_STAGES];

	GLuint		Program;
	bool		bUsingTessellation;

	TBitArray<>	TextureStageNeeds;
	TBitArray<>	UAVStageNeeds;
	int32		MaxTextureStage;

	TArray<FOpenGLBindlessSamplerInfo> Samplers;

	FOpenGLLinkedProgram()
	: Program(0), bUsingTessellation(false), MaxTextureStage(-1)
	{
		TextureStageNeeds.Init( false, FOpenGL::GetMaxCombinedTextureImageUnits() );
		UAVStageNeeds.Init( false, OGL_MAX_COMPUTE_STAGE_UAV_UNITS );
	}

	~FOpenGLLinkedProgram()
	{
		check(Program);
		glDeleteProgram(Program);
	}

	void ConfigureShaderStage( int Stage, uint32 FirstUniformBuffer );

	// Make sure GlobalArrays (created from shader reflection) matches our info (from the cross compiler)
	static inline void SortPackedUniformInfos(const TArray<FPackedUniformInfo>& ReflectedUniformInfos, const TArray<CrossCompiler::FPackedArrayInfo>& PackedGlobalArrays, TArray<FPackedUniformInfo>& OutPackedUniformInfos)
	{
		check(OutPackedUniformInfos.Num() == 0);
		OutPackedUniformInfos.AddUninitialized(PackedGlobalArrays.Num());
		for (int32 Index = 0; Index < PackedGlobalArrays.Num(); ++Index)
		{
			auto& PackedArray = PackedGlobalArrays[Index];
			int32 FoundIndex = -1;

			// Find this Global Array in the reflection list
			for (int32 FindIndex = 0; FindIndex < ReflectedUniformInfos.Num(); ++FindIndex)
			{
				auto& ReflectedInfo = ReflectedUniformInfos[FindIndex];
				if (ReflectedInfo.ArrayType == PackedArray.TypeName)
				{
					FoundIndex = FindIndex;
					OutPackedUniformInfos[Index] = ReflectedInfo;
					break;
				}
			}

			if (FoundIndex == -1)
			{
				OutPackedUniformInfos[Index].Location = -1;
				OutPackedUniformInfos[Index].ArrayType = -1;
				OutPackedUniformInfos[Index].Index = -1;
			}
		}
	}
};

typedef TMap<FOpenGLLinkedProgramConfiguration,FOpenGLLinkedProgram*> FOpenGLProgramsForReuse;

static FOpenGLProgramsForReuse& GetOpenGLProgramsCache()
{
	static FOpenGLProgramsForReuse ProgramsCache;
	return ProgramsCache;
}

// This short queue preceding released programs cache is here because usually the programs are requested again
// very shortly after they're released, so looking through recently released programs first provides tangible
// performance improvement.

#define LAST_RELEASED_PROGRAMS_CACHE_COUNT 10

static FOpenGLLinkedProgram* StaticLastReleasedPrograms[LAST_RELEASED_PROGRAMS_CACHE_COUNT] = { 0 };
static int32 StaticLastReleasedProgramsIndex = 0;

// ============================================================================================================================

static int32 CountSetBits(const TBitArray<>& Array)
{
	int32 Result = 0;
	for (TBitArray<>::FConstIterator BitIt(Array); BitIt; ++BitIt)
	{
		Result += BitIt.GetValue();
	}
	return Result;
}

void FOpenGLLinkedProgram::ConfigureShaderStage( int Stage, uint32 FirstUniformBuffer )
{
	static const ANSICHAR StagePrefix[CrossCompiler::NUM_SHADER_STAGES] = { 'v', 'p', 'g', 'h', 'd', 'c'};
	static const GLint FirstTextureUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		FOpenGL::GetFirstVertexTextureUnit(),
		FOpenGL::GetFirstPixelTextureUnit(),
		FOpenGL::GetFirstGeometryTextureUnit(),
		FOpenGL::GetFirstHullTextureUnit(),
		FOpenGL::GetFirstDomainTextureUnit(),
		FOpenGL::GetFirstComputeTextureUnit()
	};
	static const GLint FirstUAVUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		FOpenGL::GetFirstComputeUAVUnit()
	};
	ANSICHAR NameBuffer[10] = {0};

	// verify that only CS uses UAVs
	check((Stage != CrossCompiler::SHADER_STAGE_COMPUTE) ? (CountSetBits(UAVStageNeeds) == 0) : true);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderBindParameterTime);
	VERIFY_GL_SCOPE();

	NameBuffer[0] = StagePrefix[Stage];

	// Bind Global uniform arrays (vu_h, pu_i, etc)
	{
		NameBuffer[1] = 'u';
		NameBuffer[2] = '_';
		NameBuffer[3] = 0;
		NameBuffer[4] = 0;

		TArray<FPackedUniformInfo> PackedUniformInfos;
		for (uint8 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
		{
			uint8 ArrayIndexType = CrossCompiler::PackedTypeIndexToTypeName(Index);
			NameBuffer[3] = ArrayIndexType;
			GLint Location = glGetUniformLocation(Program, NameBuffer);
			if ((int32)Location != -1)
			{
				FPackedUniformInfo Info = {Location, ArrayIndexType, Index};
				PackedUniformInfos.Add(Info);
			}
		}

		SortPackedUniformInfos(PackedUniformInfos, Config.Shaders[Stage].Bindings.PackedGlobalArrays, StagePackedUniformInfo[Stage].PackedUniformInfos);
	}

	// Bind uniform buffer packed arrays (vc0_h, pc2_i, etc)
	{
		NameBuffer[1] = 'c';
		NameBuffer[2] = 0;
		NameBuffer[3] = 0;
		NameBuffer[4] = 0;
		NameBuffer[5] = 0;
		NameBuffer[6] = 0;
		for (uint8 UB = 0; UB < Config.Shaders[Stage].Bindings.NumUniformBuffers; ++UB)
		{
			TArray<FPackedUniformInfo> PackedBuffers;
			ANSICHAR* Str = SetIndex(NameBuffer, 2, UB);
			*Str++ = '_';
			Str[1] = 0;
			for (uint8 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
			{
				uint8 ArrayIndexType = CrossCompiler::PackedTypeIndexToTypeName(Index);
				Str[0] = ArrayIndexType;
				GLint Location = glGetUniformLocation(Program, NameBuffer);
				if ((int32)Location != -1)
				{
					FPackedUniformInfo Info = {Location, ArrayIndexType, Index};
					PackedBuffers.Add(Info);
				}
			}

			StagePackedUniformInfo[Stage].PackedUniformBufferInfos.Add(PackedBuffers);
		}
	}

	// Reserve and setup Space for Emulated Uniform Buffers
	StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.Empty(Config.Shaders[Stage].Bindings.NumUniformBuffers);
	StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.AddZeroed(Config.Shaders[Stage].Bindings.NumUniformBuffers);

	// Bind samplers.
	NameBuffer[1] = 's';
	NameBuffer[2] = 0;
	NameBuffer[3] = 0;
	NameBuffer[4] = 0;
	int32 LastFoundIndex = -1;
	for (int32 SamplerIndex = 0; SamplerIndex < Config.Shaders[Stage].Bindings.NumSamplers; ++SamplerIndex)
	{
		SetIndex(NameBuffer, 2, SamplerIndex);
		GLint Location = glGetUniformLocation(Program, NameBuffer);
		if (Location == -1)
		{
			if (LastFoundIndex != -1)
			{
				// It may be an array of samplers. Get the initial element location, if available, and count from it.
				SetIndex(NameBuffer, 2, LastFoundIndex);
				int32 OffsetOfArraySpecifier = (LastFoundIndex>9)?4:3;
				int32 ArrayIndex = SamplerIndex-LastFoundIndex;
				NameBuffer[OffsetOfArraySpecifier] = '[';
				ANSICHAR* EndBracket = SetIndex(NameBuffer, OffsetOfArraySpecifier+1, ArrayIndex);
				*EndBracket++ = ']';
				*EndBracket = 0;
				Location = glGetUniformLocation(Program, NameBuffer);
			}
		}
		else
		{
			LastFoundIndex = SamplerIndex;
		}

		if (Location != -1)
		{
			if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
			{
				// Non-bindless, setup the unit info
			glUniform1i(Location, FirstTextureUnit[Stage] + SamplerIndex);
			TextureStageNeeds[ FirstTextureUnit[Stage] + SamplerIndex ] = true;
			MaxTextureStage = FMath::Max( MaxTextureStage, FirstTextureUnit[Stage] + SamplerIndex);
		}
			else
			{
				//Bindless, save off the slot information
				FOpenGLBindlessSamplerInfo Info;
				Info.Handle = Location;
				Info.Slot = FirstTextureUnit[Stage] + SamplerIndex;
				Samplers.Add(Info);
			}
		}
	}

	// Bind UAVs/images.
	NameBuffer[1] = 'i';
	NameBuffer[2] = 0;
	NameBuffer[3] = 0;
	NameBuffer[4] = 0;
	int32 LastFoundUAVIndex = -1;
	for (int32 UAVIndex = 0; UAVIndex < Config.Shaders[Stage].Bindings.NumUAVs; ++UAVIndex)
	{
		SetIndex(NameBuffer, 2, UAVIndex);
		GLint Location = glGetUniformLocation(Program, NameBuffer);
		if (Location == -1)
		{
			if (LastFoundUAVIndex != -1)
			{
				// It may be an array of UAVs. Get the initial element location, if available, and count from it.
				SetIndex(NameBuffer, 2, LastFoundUAVIndex);
				int32 OffsetOfArraySpecifier = (LastFoundUAVIndex>9)?4:3;
				int32 ArrayIndex = UAVIndex-LastFoundUAVIndex;
				NameBuffer[OffsetOfArraySpecifier] = '[';
				ANSICHAR* EndBracket = SetIndex(NameBuffer, OffsetOfArraySpecifier+1, ArrayIndex);
				*EndBracket++ = ']';
				*EndBracket = '\0';
				Location = glGetUniformLocation(Program, NameBuffer);
			}
		}
		else
		{
			LastFoundUAVIndex = UAVIndex;
		}

		if (Location != -1)
		{
			// compute shaders have layout(binding) for images 
			// glUniform1i(Location, FirstUAVUnit[Stage] + UAVIndex);
			
			UAVStageNeeds[ FirstUAVUnit[Stage] + UAVIndex ] = true;
		}
	}

	// Bind uniform buffers.
	if (FOpenGL::SupportsUniformBuffers())
	{
		NameBuffer[1] = 'b';
		NameBuffer[2] = 0;
		NameBuffer[3] = 0;
		NameBuffer[4] = 0;
		for (int32 BufferIndex = 0; BufferIndex < Config.Shaders[Stage].Bindings.NumUniformBuffers; ++BufferIndex)
		{
			SetIndex(NameBuffer, 2, BufferIndex);
			GLint Location = FOpenGL::GetUniformBlockIndex(Program, NameBuffer);
			if (Location >= 0)
			{
				FOpenGL::UniformBlockBinding(Program, Location, FirstUniformBuffer + BufferIndex);
			}
		}
	}
}

#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION

#define ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097 1
/*
	As of CL 1862097 uniform buffer names are mangled to avoid collisions between variables referenced 
	in different shaders of the same program

	layout(std140) uniform _vb0
	{
	#define View View_vb0
	anon_struct_0000 View;
	};

	layout(std140) uniform _vb1
	{
	#define Primitive Primitive_vb1
	anon_struct_0001 Primitive;
	};
*/
	

struct UniformData
{
	UniformData(uint32 InOffset, uint32 InArrayElements)
		: Offset(InOffset)
		, ArrayElements(InArrayElements)
	{
	}
	uint32 Offset;
	uint32 ArrayElements;

	bool operator == (const UniformData& RHS) const
	{
		return	Offset == RHS.Offset &&	ArrayElements == RHS.ArrayElements;
	}
	bool operator != (const UniformData& RHS) const
	{
		return	!(*this == RHS);
	}
};
#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
static void VerifyUniformLayout(const FString& BlockName, const TCHAR* UniformName, const UniformData& GLSLUniform)
#else
static void VerifyUniformLayout(const TCHAR* UniformName, const UniformData& GLSLUniform)
#endif //#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
{
	static TMap<FString, UniformData> Uniforms;

	if(!Uniforms.Num())
	{
		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
			UE_LOG(LogRHI, Log, TEXT("UniformBufferStruct %s %s %d"),
				StructIt->GetStructTypeName(),
				StructIt->GetShaderVariableName(),
				StructIt->GetSize()
				);
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
			const TArray<FUniformBufferStruct::FMember>& StructMembers = StructIt->GetMembers();
			for(int32 MemberIndex = 0;MemberIndex < StructMembers.Num();++MemberIndex)
			{
				const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];

				FString BaseTypeName;
				switch(Member.GetBaseType())
				{
					case UBMT_STRUCT:  BaseTypeName = TEXT("struct");  break;
					case UBMT_BOOL:    BaseTypeName = TEXT("bool"); break;
					case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
					case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
					case UBMT_FLOAT32: BaseTypeName = TEXT("float"); break;
					case UBMT_TEXTURE: BaseTypeName = TEXT("texture"); break;
					case UBMT_SAMPLER: BaseTypeName = TEXT("sampler"); break;
					default:           UE_LOG(LogShaders, Fatal,TEXT("Unrecognized uniform buffer struct member base type."));
				};
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				UE_LOG(LogRHI, Log, TEXT("  +%d %s%dx%d %s[%d]"),
					Member.GetOffset(),
					*BaseTypeName,
					Member.GetNumRows(),
					Member.GetNumColumns(),
					Member.GetName(),
					Member.GetNumElements()
					);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				FString CompositeName = FString(StructIt->GetShaderVariableName()) + TEXT("_") + Member.GetName();

				// GLSL returns array members with a "[0]" suffix
				if(Member.GetNumElements())
				{
					CompositeName += TEXT("[0]");
				}

				check(!Uniforms.Contains(CompositeName));
				Uniforms.Add(CompositeName, UniformData(Member.GetOffset(), Member.GetNumElements()));
			}
		}
	}

#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
	/* unmangle the uniform name by stripping the block name from it
	
	layout(std140) uniform _vb0
	{
	#define View View_vb0
		anon_struct_0000 View;
	};
	*/
	FString RequestedUniformName = FString(UniformName).Replace(*(BlockName + TEXT(".")), TEXT("."));
	if(RequestedUniformName.StartsWith(TEXT(".")))
	{
		RequestedUniformName = RequestedUniformName.RightChop(1);
	}
#else
	FString RequestedUniformName = UniformName;
#endif

	const UniformData* FoundUniform = Uniforms.Find(RequestedUniformName);

	// MaterialTemplate uniform buffer does not have an entry in the FUniformBufferStructs list, so skipping it here
	if(!(RequestedUniformName.StartsWith("Material_") || RequestedUniformName.StartsWith("MaterialCollection")))
	{
		if(!FoundUniform || (*FoundUniform != GLSLUniform))
		{
			UE_LOG(LogRHI, Fatal, TEXT("uniform buffer member %s in the GLSL source doesn't match it's declaration in it's FUniformBufferStruct"), *RequestedUniformName);
		}
	}
}

static void VerifyUniformBufferLayouts(GLuint Program)
{
	GLint NumBlocks = 0;
	glGetProgramiv(Program, GL_ACTIVE_UNIFORM_BLOCKS, &NumBlocks);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
	UE_LOG(LogRHI, Log, TEXT("program %d has %d uniform blocks"), Program, NumBlocks);
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP

	for(GLint BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		const GLsizei BufferSize = 256;
		char Buffer[BufferSize] = {0};
		GLsizei Length = 0;

		GLint ActiveUniforms = 0;
		GLint BlockBytes = 0;

		glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &ActiveUniforms);
		glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &BlockBytes);
		glGetActiveUniformBlockName(Program, BlockIndex, BufferSize, &Length, Buffer);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
		FString BlockName(Buffer);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097

		FString ReferencedBy;
		{
			GLint ReferencedByVS = 0;
			GLint ReferencedByPS = 0;
			GLint ReferencedByGS = 0;
			GLint ReferencedByHS = 0;
			GLint ReferencedByDS = 0;
			GLint ReferencedByCS = 0;

			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, &ReferencedByVS);
			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, &ReferencedByPS);
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER
			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER, &ReferencedByGS);
#endif
			if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
			{
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER, &ReferencedByHS);
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER, &ReferencedByDS);
#endif
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER 
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER, &ReferencedByCS);
#endif
			}

			if(ReferencedByVS) {ReferencedBy += TEXT("V");}
			if(ReferencedByHS) {ReferencedBy += TEXT("H");}
			if(ReferencedByDS) {ReferencedBy += TEXT("D");}
			if(ReferencedByGS) {ReferencedBy += TEXT("G");}
			if(ReferencedByPS) {ReferencedBy += TEXT("P");}
			if(ReferencedByCS) {ReferencedBy += TEXT("C");}
		}
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		UE_LOG(LogRHI, Log, TEXT("  [%d] uniform block (%s) = %s, %d active uniforms, %d bytes {"),
			BlockIndex, 
			*ReferencedBy,
			ANSI_TO_TCHAR(Buffer),
			ActiveUniforms, 
			BlockBytes
			); 
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		if(ActiveUniforms)
		{
			// the other TArrays copy construct this to get the proper array size
			TArray<GLint> ActiveUniformIndices;
			ActiveUniformIndices.Init(ActiveUniforms);

			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, ActiveUniformIndices.GetData());
			
			TArray<GLint> ActiveUniformOffsets(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_OFFSET, ActiveUniformOffsets.GetData());

			TArray<GLint> ActiveUniformSizes(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_SIZE, ActiveUniformSizes.GetData());

			TArray<GLint> ActiveUniformTypes(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_TYPE, ActiveUniformTypes.GetData());

			TArray<GLint> ActiveUniformArrayStrides(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_ARRAY_STRIDE, ActiveUniformArrayStrides.GetData());

			extern const TCHAR* GetGLUniformTypeString( GLint UniformType );

			for(GLint i = 0; i < ActiveUniformIndices.Num(); ++i)
			{
				const GLint UniformIndex = ActiveUniformIndices[i];
				GLsizei Size = 0;
				GLenum Type = 0;
				glGetActiveUniform(Program, UniformIndex , BufferSize, &Length, &Size, &Type, Buffer);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP				
				UE_LOG(LogRHI, Log, TEXT("    [%d] +%d %s %s %d elements %d array stride"),
					UniformIndex,
					ActiveUniformOffsets[i],
					GetGLUniformTypeString(ActiveUniformTypes[i]),
					ANSI_TO_TCHAR(Buffer),
					ActiveUniformSizes[i],
					ActiveUniformArrayStrides[i]
				); 
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		
				const UniformData GLSLUniform
				(
					ActiveUniformOffsets[i], 
					ActiveUniformArrayStrides[i] > 0 ? ActiveUniformSizes[i] : 0 // GLSL has 1 as array size for non-array uniforms, but FUniformBufferStruct assumes 0
				);
#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
				VerifyUniformLayout(BlockName, ANSI_TO_TCHAR(Buffer), GLSLUniform);
#else
				VerifyUniformLayout(ANSI_TO_TCHAR(Buffer), GLSLUniform);
#endif
			}
		}
	}
}
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION

/**
 * Link vertex and pixel shaders in to an OpenGL program.
 */
static FOpenGLLinkedProgram* LinkProgram( const FOpenGLLinkedProgramConfiguration& Config)
{
	ANSICHAR Buf[32] = {0};

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLinkTime);
	VERIFY_GL_SCOPE();

	GLuint Program = glCreateProgram();

	// ensure that compute shaders are always alone
	check( (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource == 0) != (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource == 0));
	check( (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource == 0) != (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource == 0));

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
	{
		glAttachShader(Program, Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource);
	}
	if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
	{
		glAttachShader(Program, Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource);
	}
	if (Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource)
	{
		glAttachShader(Program, Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource);
	}
	if (Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource)
	{
		glAttachShader(Program, Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource);
	}
	if (Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource)
	{
		glAttachShader(Program, Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource);
	}
	if (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource)
	{
		glAttachShader(Program, Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource);
	}
	
	// E.g. GLSL_430 uses layout(location=xx) instead of having to call glBindAttribLocation and glBindFragDataLocation
	if (OpenGLShaderPlatformNeedsBindLocation(GMaxRHIShaderPlatform))
	{
		// Bind attribute indices.
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
		{
			uint32 Mask = Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.InOutMask;
			uint32 Index = 0;
			FCStringAnsi::Strcpy(Buf, "in_ATTRIBUTE");
			while (Mask)
			{
				if (Mask & 0x1)
				{
					if (Index < 10)
					{
						Buf[12] = '0' + Index;
						Buf[13] = 0;
					}
					else
					{
						Buf[12] = '1';
						Buf[13] = '0' + (Index % 10);
						Buf[14] = 0;
					}
				glBindAttribLocation(Program, Index, Buf);
				}
				Index++;
				Mask >>= 1;
			}
		}

		// Bind frag data locations.
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
		{
			uint32 Mask = (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.InOutMask) & 0x7fff; // mask out the depth bit
			uint32 Index = 0;
			FCStringAnsi::Strcpy(Buf, "out_Target");
			while (Mask)
			{
				if (Mask & 0x1)
				{
					if (Index < 10)
					{
						Buf[10] = '0' + Index;
						Buf[11] = 0;
					}
					else
					{
						Buf[10] = '1';
						Buf[11] = '0' + (Index % 10);
						Buf[12] = 0;
					}
					FOpenGL::BindFragDataLocation(Program, Index, Buf);
				}
				Index++;
				Mask >>= 1;
			}
		}
	}

	// Link.
	glLinkProgram(Program);
	if (!VerifyLinkedProgram(Program))
	{
		return NULL;
	}

	FOpenGLLinkedProgram* LinkedProgram = new FOpenGLLinkedProgram;
	LinkedProgram->Config = Config;
	LinkedProgram->Program = Program;
	LinkedProgram->bUsingTessellation = Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource && Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource;

	glUseProgram(Program);

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_VERTEX,
			OGL_FIRST_UNIFORM_BUFFER
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_VERTEX].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_PIXEL,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_PIXEL].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_GEOMETRY,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_GEOMETRY].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_HULL,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.NumUniformBuffers
		);
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_DOMAIN,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Bindings.NumUniformBuffers
		);
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_COMPUTE,
			OGL_FIRST_UNIFORM_BUFFER
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_COMPUTE].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings.PackedGlobalArrays.Num());
	}
#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION	
	VerifyUniformBufferLayouts(Program);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
	return LinkedProgram;
}

FComputeShaderRHIRef FOpenGLDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	
	FOpenGLComputeShader* ComputeShader = CompileOpenGLShader<FOpenGLComputeShader>(Code);
	const ANSICHAR* GlslCode = NULL;
	if (!ComputeShader->bSuccessfullyCompiled)
	{
#if DEBUG_GL_SHADERS
		GlslCode = ComputeShader->GlslCodeString;
#endif
		ComputeShader->bSuccessfullyCompiled = VerifyCompiledShader(ComputeShader->Resource, GlslCode);
	}

	check( ComputeShader != 0);

	FOpenGLLinkedProgramConfiguration Config;

	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource = ComputeShader->Resource;
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings = ComputeShader->Bindings;

	ComputeShader->LinkedProgram = LinkProgram( Config);

	if (ComputeShader->LinkedProgram == NULL)
	{
#if DEBUG_GL_SHADERS
		if (ComputeShader->bSuccessfullyCompiled)
		{
			UE_LOG(LogRHI,Error,TEXT("Compute Shader:\n%s"),ANSI_TO_TCHAR(ComputeShader->GlslCode.GetData()));
		}
#endif //DEBUG_GL_SHADERS
		checkf(ComputeShader->LinkedProgram, TEXT("Compute shader failed to compile & link."));
	}

	return ComputeShader;
}

// ============================================================================================================================

FBoundShaderStateRHIRef FOpenGLDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI,
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI, 
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{ 
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateBoundShaderStateTime);

	if(!PixelShaderRHI)
	{
		// use special null pixel shader when PixelShader was set to NULL
		PixelShaderRHI = TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel))->GetPixelShader();
	}

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);

	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		check(VertexDeclarationRHI);

		DYNAMIC_CAST_OPENGLRESOURCE(VertexDeclaration,VertexDeclaration);
		DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,VertexShader);
		DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,PixelShader);
		DYNAMIC_CAST_OPENGLRESOURCE(HullShader,HullShader);
		DYNAMIC_CAST_OPENGLRESOURCE(DomainShader,DomainShader);
		DYNAMIC_CAST_OPENGLRESOURCE(GeometryShader,GeometryShader);

		FOpenGLLinkedProgramConfiguration Config;

		check(VertexShader);
		check(PixelShader);

		// Fill-in the configuration
		Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings = VertexShader->Bindings;
		Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource = VertexShader->Resource;
		Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings = PixelShader->Bindings;
		Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource = PixelShader->Resource;
		if (GeometryShader)
		{
			Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings = GeometryShader->Bindings;
			Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource = GeometryShader->Resource;
		}

		if ( FOpenGL::SupportsTessellation())
		{
			if ( HullShader)
			{
				Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Bindings = HullShader->Bindings;
				Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource = HullShader->Resource;
			}
			if ( DomainShader)
			{
				Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Bindings = DomainShader->Bindings;
				Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource = DomainShader->Resource;
			}
		}

		// Check if we already have such a program in released programs cache. Use it, if we do.
		FOpenGLLinkedProgram* LinkedProgram = 0;

		int32 Index = StaticLastReleasedProgramsIndex;
		for( int CacheIndex = 0; CacheIndex < LAST_RELEASED_PROGRAMS_CACHE_COUNT; ++CacheIndex )
		{
			FOpenGLLinkedProgram* Prog = StaticLastReleasedPrograms[Index];
			if( Prog && Prog->Config == Config )
			{
				StaticLastReleasedPrograms[Index] = 0;
				LinkedProgram = Prog;
				break;
			}
			Index = (Index == LAST_RELEASED_PROGRAMS_CACHE_COUNT-1) ? 0 : Index+1;
		}

		if (!LinkedProgram)
		{
			FOpenGLLinkedProgram** CachedProgram = GetOpenGLProgramsCache().Find( Config);

			if (CachedProgram)
			{
				LinkedProgram = *CachedProgram;
			}
			else
			{
				const ANSICHAR* GlslCode = NULL;
				if (!VertexShader->bSuccessfullyCompiled)
				{
#if DEBUG_GL_SHADERS
					GlslCode = VertexShader->GlslCodeString;
#endif
					VertexShader->bSuccessfullyCompiled = VerifyCompiledShader(VertexShader->Resource, GlslCode);
				}
				if (!PixelShader->bSuccessfullyCompiled)
				{
#if DEBUG_GL_SHADERS
					GlslCode = PixelShader->GlslCodeString;
#endif
					PixelShader->bSuccessfullyCompiled = VerifyCompiledShader(PixelShader->Resource, GlslCode);
				}
				if (GeometryShader && !GeometryShader->bSuccessfullyCompiled)
				{
#if DEBUG_GL_SHADERS
					GlslCode = GeometryShader->GlslCodeString;
#endif
					GeometryShader->bSuccessfullyCompiled = VerifyCompiledShader(GeometryShader->Resource, GlslCode);
				}
				if ( FOpenGL::SupportsTessellation() )
				{
					if (HullShader && !HullShader->bSuccessfullyCompiled)
					{
#if DEBUG_GL_SHADERS
						GlslCode = HullShader->GlslCodeString;
#endif
						HullShader->bSuccessfullyCompiled = VerifyCompiledShader(HullShader->Resource, GlslCode);
					}
					if (DomainShader && !DomainShader->bSuccessfullyCompiled)
					{
#if DEBUG_GL_SHADERS
						GlslCode = DomainShader->GlslCodeString;
#endif
						DomainShader->bSuccessfullyCompiled = VerifyCompiledShader(DomainShader->Resource, GlslCode);
					}
				}

				// Make sure we have OpenGL context set up, and invalidate the parameters cache and current program (as we'll link a new one soon)
				GetContextStateForCurrentContext().Program = -1;
				MarkShaderParameterCachesDirty(PendingState.ShaderParameters, false);

				// Link program, using the data provided in config
				LinkedProgram = LinkProgram(Config);

				// Add this program to the cache
				GetOpenGLProgramsCache().Add(Config,LinkedProgram);

				if (LinkedProgram == NULL)
				{
#if DEBUG_GL_SHADERS
					if (VertexShader->bSuccessfullyCompiled)
					{
						UE_LOG(LogRHI,Error,TEXT("Vertex Shader:\n%s"),ANSI_TO_TCHAR(VertexShader->GlslCode.GetData()));
					}
					if (PixelShader->bSuccessfullyCompiled)
					{
						UE_LOG(LogRHI,Error,TEXT("Pixel Shader:\n%s"),ANSI_TO_TCHAR(PixelShader->GlslCode.GetData()));
					}
					if (GeometryShader && GeometryShader->bSuccessfullyCompiled)
					{
						UE_LOG(LogRHI,Error,TEXT("Geometry Shader:\n%s"),ANSI_TO_TCHAR(GeometryShader->GlslCode.GetData()));
					}
					if ( FOpenGL::SupportsTessellation() )
					{
						if (HullShader && HullShader->bSuccessfullyCompiled)
						{
							UE_LOG(LogRHI,Error,TEXT("Hull Shader:\n%s"),ANSI_TO_TCHAR(HullShader->GlslCode.GetData()));
						}
						if (DomainShader && DomainShader->bSuccessfullyCompiled)
						{
							UE_LOG(LogRHI,Error,TEXT("Domain Shader:\n%s"),ANSI_TO_TCHAR(DomainShader->GlslCode.GetData()));
						}
					}
#endif //DEBUG_GL_SHADERS
					check(LinkedProgram);
				}
			}
		}

		FOpenGLBoundShaderState* BoundShaderState = new FOpenGLBoundShaderState(
			LinkedProgram,
			VertexDeclarationRHI,
			VertexShaderRHI,
			PixelShaderRHI,
			GeometryShaderRHI,
			HullShaderRHI,
			DomainShaderRHI
			);

		return BoundShaderState;
	}
}

void DestroyShadersAndPrograms()
{
	FOpenGLProgramsForReuse& ProgramCache = GetOpenGLProgramsCache();

	for( TMap<FOpenGLLinkedProgramConfiguration,FOpenGLLinkedProgram*>::TIterator It( ProgramCache ); It; ++It )
	{
		delete It.Value();
	}
	ProgramCache.Empty();
	
	StaticLastReleasedProgramsIndex = 0;

	FOpenGLCompiledShaderCache& ShaderCache = GetOpenGLCompiledShaderCache();
	for( TMap<FOpenGLCompiledShaderKey,GLuint>::TIterator It( ShaderCache ); It; ++It )
	{
		glDeleteShader(It.Value());
	}
	ShaderCache.Empty();
}

struct FSamplerPair
{
	GLuint Texture;
	GLuint Sampler;

	friend bool operator ==(const FSamplerPair& A,const FSamplerPair& B)
	{
		return A.Texture == B.Texture && A.Sampler == B.Sampler;
	}

	friend uint32 GetTypeHash(const FSamplerPair &Key)
	{
		return Key.Texture ^ (Key.Sampler << 18);
	}
};

static TMap<FSamplerPair, GLuint64> BindlessSamplerMap;

void FOpenGLDynamicRHI::SetupBindlessTextures( FOpenGLContextState& ContextState, const TArray<FOpenGLBindlessSamplerInfo> &Samplers )
{
	if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
	{
		return;
	}

	// Bind all textures via Bindless
	for (int32 Texture = 0; Texture < Samplers.Num(); Texture++)
	{
		const FOpenGLBindlessSamplerInfo &Sampler = Samplers[Texture];

		GLuint64 BindlessSampler = 0xffffffff;
		FSamplerPair Pair;
		Pair.Texture = PendingState.Textures[Sampler.Slot].Resource;
		Pair.Sampler = (PendingState.SamplerStates[Sampler.Slot] != NULL) ? PendingState.SamplerStates[Sampler.Slot]->Resource : 0;

		if (Pair.Texture)
		{
			// Find Sampler pair
			if ( BindlessSamplerMap.Contains(Pair))
			{
				BindlessSampler = BindlessSamplerMap[Pair];
			}
			else
			{
				// if !found, create

				if (Pair.Sampler)
				{
					BindlessSampler = FOpenGL::GetTextureSamplerHandle( Pair.Texture, Pair.Sampler);
				}
				else
				{
					BindlessSampler = FOpenGL::GetTextureHandle( Pair.Texture);
				}

				FOpenGL::MakeTextureHandleResident( BindlessSampler);

				BindlessSamplerMap.Add( Pair, BindlessSampler);
			}

			FOpenGL::UniformHandleui64( Sampler.Handle, BindlessSampler);
		}
	}
}

void FOpenGLDynamicRHI::BindPendingShaderState( FOpenGLContextState& ContextState )
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLShaderBindTime);
	VERIFY_GL_SCOPE();

	bool ForceUniformBindingUpdate = false;

	GLuint PendingProgram = PendingState.BoundShaderState->LinkedProgram->Program;
	if (ContextState.Program != PendingProgram)
	{
		glUseProgram(PendingProgram);
		ContextState.Program = PendingProgram;
		ContextState.bUsingTessellation = PendingState.BoundShaderState->LinkedProgram->bUsingTessellation;
		MarkShaderParameterCachesDirty(PendingState.ShaderParameters, false);
		//Disable the forced rebinding to reduce driver overhead
#if 0
		ForceUniformBindingUpdate = true;
#endif
	}

	if (!GUseEmulatedUniformBuffers)
	{
		int32 NextUniformBufferIndex = OGL_FIRST_UNIFORM_BUFFER;

		int32 NumVertexUniformBuffers = PendingState.BoundShaderState->VertexShader->Bindings.NumUniformBuffers;
		BindUniformBufferBase(
			ContextState,
			NumVertexUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Vertex],
			NextUniformBufferIndex,
			ForceUniformBindingUpdate);
		NextUniformBufferIndex += NumVertexUniformBuffers;

		int32 NumPixelUniformBuffers = PendingState.BoundShaderState->PixelShader->Bindings.NumUniformBuffers;
		BindUniformBufferBase(
			ContextState,
			NumPixelUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Pixel],
			NextUniformBufferIndex,
			ForceUniformBindingUpdate);
		NextUniformBufferIndex += NumPixelUniformBuffers;

		if (PendingState.BoundShaderState->GeometryShader)
		{
			int32 NumGeometryUniformBuffers = PendingState.BoundShaderState->GeometryShader->Bindings.NumUniformBuffers;
			BindUniformBufferBase(
				ContextState,
				NumGeometryUniformBuffers,
				PendingState.BoundUniformBuffers[SF_Geometry],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumGeometryUniformBuffers;
		}

		if (PendingState.BoundShaderState->HullShader)
		{
			int32 NumHullUniformBuffers = PendingState.BoundShaderState->HullShader->Bindings.NumUniformBuffers;
			BindUniformBufferBase(ContextState,
				NumHullUniformBuffers,
				PendingState.BoundUniformBuffers[SF_Hull],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumHullUniformBuffers;
		}

		if (PendingState.BoundShaderState->DomainShader)
		{
			int32 NumDomainUniformBuffers = PendingState.BoundShaderState->DomainShader->Bindings.NumUniformBuffers;
			BindUniformBufferBase(ContextState,
				NumDomainUniformBuffers,
				PendingState.BoundUniformBuffers[SF_Domain],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumDomainUniformBuffers;
		}

		SetupBindlessTextures( ContextState, PendingState.BoundShaderState->LinkedProgram->Samplers );
	}
}

FOpenGLBoundShaderState::FOpenGLBoundShaderState(
	FOpenGLLinkedProgram* InLinkedProgram,
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI,
	FGeometryShaderRHIParamRef InGeometryShaderRHI,
	FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI
	)
	:	CacheLink(InVertexDeclarationRHI, InVertexShaderRHI, InPixelShaderRHI,
		InHullShaderRHI, InDomainShaderRHI,	InGeometryShaderRHI, this)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,InPixelShader);
	DYNAMIC_CAST_OPENGLRESOURCE(HullShader,InHullShader);
	DYNAMIC_CAST_OPENGLRESOURCE(DomainShader,InDomainShader);
	DYNAMIC_CAST_OPENGLRESOURCE(GeometryShader,InGeometryShader);

	VertexDeclaration = InVertexDeclaration;
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
	GeometryShader = InGeometryShader;

	HullShader = InHullShader;
	DomainShader = InDomainShader;

	LinkedProgram = InLinkedProgram;
}

FOpenGLBoundShaderState::~FOpenGLBoundShaderState()
{
	check(LinkedProgram);
	FOpenGLLinkedProgram* Prog = StaticLastReleasedPrograms[StaticLastReleasedProgramsIndex];
	StaticLastReleasedPrograms[StaticLastReleasedProgramsIndex++] = LinkedProgram;
	if (StaticLastReleasedProgramsIndex == LAST_RELEASED_PROGRAMS_CACHE_COUNT)
	{
		StaticLastReleasedProgramsIndex = 0;
	}
	OnProgramDeletion(LinkedProgram->Program);
}

bool FOpenGLBoundShaderState::NeedsTextureStage(int32 TextureStageIndex)
{
	return LinkedProgram->TextureStageNeeds[TextureStageIndex];
}

int32 FOpenGLBoundShaderState::MaxTextureStageUsed()
{
	return LinkedProgram->MaxTextureStage;
}

bool FOpenGLComputeShader::NeedsTextureStage(int32 TextureStageIndex)
{
	return LinkedProgram->TextureStageNeeds[TextureStageIndex];
}

int32 FOpenGLComputeShader::MaxTextureStageUsed()
{
	return LinkedProgram->MaxTextureStage;
}

bool FOpenGLComputeShader::NeedsUAVStage(int32 UAVStageIndex)
{
	return LinkedProgram->UAVStageNeeds[UAVStageIndex];
}

void FOpenGLDynamicRHI::BindPendingComputeShaderState(FOpenGLContextState& ContextState, FComputeShaderRHIParamRef ComputeShaderRHI)
{
	VERIFY_GL_SCOPE();
	DYNAMIC_CAST_OPENGLRESOURCE(ComputeShader,ComputeShader);
	bool ForceUniformBindingUpdate = false;

	GLuint PendingProgram = ComputeShader->LinkedProgram->Program;
	if (ContextState.Program != PendingProgram)
	{
		glUseProgram(PendingProgram);
		ContextState.Program = PendingProgram;
		MarkShaderParameterCachesDirty(PendingState.ShaderParameters, true);
		ForceUniformBindingUpdate = true;
	}

	if (!GUseEmulatedUniformBuffers)
	{
		BindUniformBufferBase(
			ContextState,
			ComputeShader->Bindings.NumUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Compute],
			OGL_FIRST_UNIFORM_BUFFER,
			ForceUniformBindingUpdate);
		SetupBindlessTextures( ContextState, ComputeShader->LinkedProgram->Samplers );
	}
}

/** Constructor. */
FOpenGLShaderParameterCache::FOpenGLShaderParameterCache() :
	GlobalUniformArraySize(-1)
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = 0;
	}
}

void FOpenGLShaderParameterCache::InitializeResources(int32 UniformArraySize)
{
	check(GlobalUniformArraySize == -1);

	// Uniform arrays have to be multiples of float4s.
	UniformArraySize = Align(UniformArraySize,SizeOfFloat4);

	PackedGlobalUniforms[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	PackedUniformsScratch[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);

	FMemory::Memzero(PackedGlobalUniforms[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	FMemory::Memzero(PackedUniformsScratch[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	for (int32 ArrayIndex = 1; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = PackedGlobalUniforms[ArrayIndex - 1] + UniformArraySize;
		PackedUniformsScratch[ArrayIndex] = PackedUniformsScratch[ArrayIndex - 1] + UniformArraySize;
	}
	GlobalUniformArraySize = UniformArraySize;

	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = UniformArraySize / SizeOfFloat4;
	}
}

/** Destructor. */
FOpenGLShaderParameterCache::~FOpenGLShaderParameterCache()
{
	if (GlobalUniformArraySize > 0)
	{
		FMemory::Free(PackedUniformsScratch[0]);
		FMemory::Free(PackedGlobalUniforms[0]);
	}

	FMemory::MemZero(PackedUniformsScratch);
	FMemory::MemZero(PackedGlobalUniforms);

	GlobalUniformArraySize = -1;
}

/**
 * Marks all uniform arrays as dirty.
 */
void FOpenGLShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = GlobalUniformArraySize / SizeOfFloat4;
	}
}

/**
 * Set parameter values.
 */
void FOpenGLShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	check(GlobalUniformArraySize != -1);
	check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
	check(ByteOffset + NumBytes <= (uint32)GlobalUniformArraySize);
	PackedGlobalUniformDirty[BufferIndex].MarkDirtyRange(ByteOffset / SizeOfFloat4, (NumBytes + SizeOfFloat4 - 1) / SizeOfFloat4);
	FMemory::Memcpy(PackedGlobalUniforms[BufferIndex] + ByteOffset, NewValues, NumBytes);
}

/**
 * Commit shader parameters to the currently bound program.
 * @param ParameterTable - Information on the bound uniform arrays for the program.
 */
void FOpenGLShaderParameterCache::CommitPackedGlobals(const FOpenGLLinkedProgram* LinkedProgram, int32 Stage)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLUniformCommitTime);
	VERIFY_GL_SCOPE();
	const uint32 BytesPerRegister = 16;

	/**
	 * Note that this always uploads the entire uniform array when it is dirty.
	 * The arrays are marked dirty either when the bound shader state changes or
	 * a value in the array is modified. OpenGL actually caches uniforms per-
	 * program. If we shadowed those per-program uniforms we could avoid calling
	 * glUniform4?v for values that have not changed since the last invocation
	 * of the program. 
	 *
	 * It's unclear whether the driver does the same thing and whether there is
	 * a performance benefit. Even if there is, this type of caching makes any
	 * multithreading vastly more difficult, so for now uniforms are not cached
	 * per-program.
	 */
	const TArray<FOpenGLLinkedProgram::FPackedUniformInfo>& PackedUniforms = LinkedProgram->StagePackedUniformInfo[Stage].PackedUniformInfos;
	const TArray<CrossCompiler::FPackedArrayInfo>& PackedArrays = LinkedProgram->Config.Shaders[Stage].Bindings.PackedGlobalArrays;
	for (int32 PackedUniform = 0; PackedUniform < PackedUniforms.Num(); ++PackedUniform)
	{
		const FOpenGLLinkedProgram::FPackedUniformInfo& UniformInfo = PackedUniforms[PackedUniform];
		const int32 ArrayIndex = UniformInfo.Index;
		const int32 NumVectors = PackedArrays[PackedUniform].Size / BytesPerRegister;
		GLint Location = UniformInfo.Location;
		const void* UniformData = PackedGlobalUniforms[ArrayIndex];

		// This has to be >=. If LowVector == HighVector it means that particular vector was written to.
		if (PackedGlobalUniformDirty[ArrayIndex].NumVectors > 0)
		{
			const int32 StartVector = PackedGlobalUniformDirty[ArrayIndex].StartVector;
			int32 NumDirtyVectors = FMath::Min((int32)PackedGlobalUniformDirty[ArrayIndex].NumVectors, NumVectors - StartVector);
			check(NumDirtyVectors);
			UniformData = (uint8*)UniformData + StartVector * SizeOfFloat4;
			Location += StartVector;
			switch (UniformInfo.Index)
			{
			case CrossCompiler::PACKED_TYPEINDEX_HIGHP:
			case CrossCompiler::PACKED_TYPEINDEX_MEDIUMP:
			case CrossCompiler::PACKED_TYPEINDEX_LOWP:
				glUniform4fv(Location, NumDirtyVectors, (GLfloat*)UniformData);
				break;

			case CrossCompiler::PACKED_TYPEINDEX_INT:
				glUniform4iv(Location, NumDirtyVectors, (GLint*)UniformData);
				break;

			case CrossCompiler::PACKED_TYPEINDEX_UINT:
				FOpenGL::Uniform4uiv(Location, NumDirtyVectors, (GLuint*)UniformData);
				break;
			}

			PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
			PackedGlobalUniformDirty[ArrayIndex].NumVectors = 0;
		}
	}
}

void FOpenGLShaderParameterCache::CommitPackedUniformBuffers(FOpenGLLinkedProgram* LinkedProgram, int32 Stage, FUniformBufferRHIRef* RHIUniformBuffers, const TArray<FOpenGLUniformBufferCopyInfo>& UniformBuffersCopyInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLConstantBufferUpdateTime);
	VERIFY_GL_SCOPE();

	// Uniform Buffers are split into precision/type; the list of RHI UBs is traversed and if a new one was set, its
	// contents are copied per precision/type into corresponding scratch buffers which are then uploaded to the program
	const FOpenGLShaderBindings& Bindings = LinkedProgram->Config.Shaders[Stage].Bindings;
	check(Bindings.NumUniformBuffers <= FOpenGLRHIState::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);

	if (Bindings.bFlattenUB)
	{
		int32 LastInfoIndex = 0;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FOpenGLUniformBuffer* UniformBuffer = (FOpenGLUniformBuffer*)RHIUniformBuffers[BufferIndex].GetReference();
			check(UniformBuffer);
			const uint32* RESTRICT SourceData = UniformBuffer->EmulatedBufferData->Data.GetData();
			for (int32 InfoIndex = LastInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
			{
				const FOpenGLUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
				if (Info.SourceUBIndex == BufferIndex)
				{
					check((Info.DestOffsetInFloats + Info.SizeInFloats) * sizeof(float) <= (uint32)GlobalUniformArraySize);
					float* RESTRICT ScratchMem = (float*)PackedGlobalUniforms[Info.DestUBTypeIndex];
					ScratchMem += Info.DestOffsetInFloats;
					FMemory::Memcpy(ScratchMem, SourceData + Info.SourceOffsetInFloats, Info.SizeInFloats * sizeof(float));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].MarkDirtyRange(Info.DestOffsetInFloats / NumFloatsInFloat4, (Info.SizeInFloats + NumFloatsInFloat4 - 1) / NumFloatsInFloat4);
				}
				else
				{
					LastInfoIndex = InfoIndex;
					break;
				}
			}
		}
	}
	else
	{
		const auto& PackedUniformBufferInfos = LinkedProgram->StagePackedUniformInfo[Stage].PackedUniformBufferInfos;
		int32 LastCopyInfoIndex = 0;
		auto& EmulatedUniformBufferSet = LinkedProgram->StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FOpenGLUniformBuffer* UniformBuffer = (FOpenGLUniformBuffer*)RHIUniformBuffers[BufferIndex].GetReference();
			check(UniformBuffer);
			if (EmulatedUniformBufferSet[BufferIndex] != UniformBuffer->UniqueID)
			{
				EmulatedUniformBufferSet[BufferIndex] = UniformBuffer->UniqueID;

				// Go through the list of copy commands and perform the appropriate copy into the scratch buffer
				for (int32 InfoIndex = LastCopyInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
				{
					const FOpenGLUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
					if (Info.SourceUBIndex == BufferIndex)
					{
						const uint32* RESTRICT SourceData = UniformBuffer->EmulatedBufferData->Data.GetData();
						SourceData += Info.SourceOffsetInFloats;
						float* RESTRICT ScratchMem = (float*)PackedUniformsScratch[Info.DestUBTypeIndex];
						ScratchMem += Info.DestOffsetInFloats;
						FMemory::Memcpy(ScratchMem, SourceData, Info.SizeInFloats * sizeof(float));
					}
					else if (Info.SourceUBIndex > BufferIndex)
					{
						// Done finding current copies
						LastCopyInfoIndex = InfoIndex;
						break;
					}

					// keep going since we could have skipped this loop when skipping cached UBs...
				}

				// Upload the split buffers to the program
				const auto& UniformBufferUploadInfoList = PackedUniformBufferInfos[BufferIndex];
				auto& UBInfo = Bindings.PackedUniformBuffers[BufferIndex];
				for (int32 InfoIndex = 0; InfoIndex < UniformBufferUploadInfoList.Num(); ++InfoIndex)
				{
					const auto& UniformInfo = UniformBufferUploadInfoList[InfoIndex];
					const void* RESTRICT UniformData = PackedUniformsScratch[UniformInfo.Index];
					int32 NumVectors = UBInfo[InfoIndex].Size / SizeOfFloat4;
					check(UniformInfo.ArrayType == UBInfo[InfoIndex].TypeName);
					switch (UniformInfo.Index)
					{
					case CrossCompiler::PACKED_TYPEINDEX_HIGHP:
					case CrossCompiler::PACKED_TYPEINDEX_MEDIUMP:
					case CrossCompiler::PACKED_TYPEINDEX_LOWP:
						glUniform4fv(UniformInfo.Location, NumVectors, (GLfloat*)UniformData);
						break;

					case CrossCompiler::PACKED_TYPEINDEX_INT:
						glUniform4iv(UniformInfo.Location, NumVectors, (GLint*)UniformData);
						break;

					case CrossCompiler::PACKED_TYPEINDEX_UINT:
						FOpenGL::Uniform4uiv(UniformInfo.Location, NumVectors, (GLuint*)UniformData);
						break;
					}
				}
			}
		}
	}
}

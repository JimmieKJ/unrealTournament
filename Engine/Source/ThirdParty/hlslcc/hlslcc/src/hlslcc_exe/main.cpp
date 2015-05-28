// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if WIN32
#include <Windows.h>
#else
#include <stdarg.h>
#include <syslog.h>
#endif
#include "ShaderCompilerCommon.h"
#include "hlslcc.h"
#include "LanguageSpec.h"
//#include "glsl/ir_gen_glsl.h"

enum EHlslccBackend
{
	HB_Glsl,
	HB_Invalid,
};

/** Debug output. */
static char* DebugBuffer = 0;
static void dprintf(const char* Format, ...)
{
	const int BufSize = (1 << 20);
	va_list Args;
	int Count;

	if (DebugBuffer == nullptr)
	{
		DebugBuffer = (char*)malloc(BufSize);
	}

	va_start(Args, Format);
#if WIN32
	Count = vsnprintf_s(DebugBuffer, BufSize, _TRUNCATE, Format, Args);
#else
	Count = vsnprintf(DebugBuffer, BufSize, Format, Args);
#endif
	va_end(Args);

	if (Count < -1)
	{
		// Overflow, add a line feed and null terminate the string.
		DebugBuffer[BufSize - 2] = '\n';
		DebugBuffer[BufSize - 1] = 0;
	}
	else
	{
		// Make sure the string is null terminated.
		DebugBuffer[Count] = 0;
	}
	
#if WIN32
	OutputDebugString(DebugBuffer);
#elif __APPLE__
	syslog(LOG_DEBUG, "%s", DebugBuffer);
#endif
	fprintf(stdout, "%s", DebugBuffer);
}

#include "ir.h"
#include "irDump.h"

struct FGlslCodeBackend : public FCodeBackend
{
	FGlslCodeBackend(unsigned int InHlslCompileFlags) : FCodeBackend(InHlslCompileFlags) {}

	// Returns false if any issues
	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override
	{
		ir_function_signature* EntryPointSig = FindEntryPointFunction(Instructions, ParseState, EntryPoint);
		if (EntryPointSig)
		{
			EntryPointSig->is_main = true;
			return true;
		}

		return false;
	}

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override
	{
		IRDump(ir);
		return 0;
	}
};
#define FRAMEBUFFER_FETCH_ES2	"FramebufferFetchES2"
#include "ir.h"

struct FGlslLanguageSpec : public ILanguageSpec
{
	virtual bool SupportsDeterminantIntrinsic() const {return false;}
	virtual bool SupportsTransposeIntrinsic() const {return false;}
	virtual bool SupportsIntegerModulo() const {return true;}

	// half3x3 <-> float3x3
	virtual bool SupportsMatrixConversions() const {return false;}
	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir)
	{
		make_intrinsic_genType(ir, State, FRAMEBUFFER_FETCH_ES2, ir_invalid_opcode, IR_INTRINSIC_FLOAT, 0, 4, 4);
	}
};


char* LoadShaderFromFile(const char* Filename);

struct SCmdOptions
{
	const char* ShaderFilename;
	EHlslShaderFrequency Frequency;
	EHlslCompileTarget Target;
	EHlslccBackend Backend;
	const char* Entry;
	bool bDumpAST;
	bool bNoPreprocess;
	bool bFlattenUB;
	bool bFlattenUBStructures;
	bool bGroupFlattenedUB;
	bool bExpandExpressions;
	bool bCSE;
	bool bSeparateShaderObjects;
	const char* OutFile;

	SCmdOptions() 
	{
		ShaderFilename = nullptr;
		Frequency = HSF_InvalidFrequency;
		Target = HCT_InvalidTarget;
		Backend = HB_Invalid;
		Entry = nullptr;
		bDumpAST = false;
		bNoPreprocess = false;
		bFlattenUB = false;
		bFlattenUBStructures = false;
		bGroupFlattenedUB = false;
		bExpandExpressions = false;
		bCSE = false;
		bSeparateShaderObjects = false;
		OutFile = nullptr;
	}
};

static int ParseCommandLine( int argc, char** argv, SCmdOptions& OutOptions)
{
	while (argc)
	{
		if (**argv == '-')
		{
			if (!strcmp(*argv, "-vs"))
			{
				OutOptions.Frequency = HSF_VertexShader;
			}
			else if (!strcmp(*argv, "-ps"))
			{
				OutOptions.Frequency = HSF_PixelShader;
			}
			else if (!strcmp(*argv, "-gs"))
			{
				OutOptions.Frequency = HSF_GeometryShader;
			}
			else if (!strcmp(*argv, "-ds"))
			{
				OutOptions.Frequency = HSF_DomainShader;
			}
			else if (!strcmp(*argv, "-hs"))
			{
				OutOptions.Frequency = HSF_HullShader;
			}
			else if (!strcmp(*argv, "-cs"))
			{
				OutOptions.Frequency = HSF_ComputeShader;
			}
			else if (!strcmp(*argv, "-gl3"))
			{
				OutOptions.Target = HCT_FeatureLevelSM4;
				OutOptions.Backend = HB_Glsl;
			}
			else if (!strcmp(*argv, "-gl4"))
			{
				OutOptions.Target = HCT_FeatureLevelSM5;
				OutOptions.Backend = HB_Glsl;
			}
			else if (!strcmp(*argv, "-es2"))
			{
				OutOptions.Target = HCT_FeatureLevelES2;
				OutOptions.Backend = HB_Glsl;
			}
			else if (!strcmp(*argv, "-mac"))
			{
				// Ignore...
			}
			else if (!strncmp(*argv, "-entry=", 7))
			{
				OutOptions.Entry = (*argv) + 7;
			}
			else if (!strcmp(*argv, "-ast"))
			{
				OutOptions.bDumpAST = true;
			}
			else if (!strcmp(*argv, "-nopp"))
			{
				OutOptions.bNoPreprocess = true;
			}
			else if (!strcmp(*argv, "-flattenub"))
			{
				OutOptions.bFlattenUB = true;
			}
			else if (!strcmp(*argv, "-flattenubstruct"))
			{
				OutOptions.bFlattenUBStructures = true;
			}
			else if (!strcmp(*argv, "-groupflatub"))
			{
				OutOptions.bGroupFlattenedUB = true;
			}
			else if (!strcmp(*argv, "-cse"))
			{
				OutOptions.bCSE = true;
			}
			else if (!strcmp(*argv, "-xpxpr"))
			{
				OutOptions.bExpandExpressions = true;
			}
			else if (!strncmp(*argv, "-o=", 3))
			{
				OutOptions.OutFile = (*argv) + 3;
			}
			else if (!strcmp( *argv, "-separateshaders"))
			{
				OutOptions.bSeparateShaderObjects = true;
			}
			else
			{
				dprintf("Warning: Unknown option %s\n", *argv);
			}
		}
		else
		{
			OutOptions.ShaderFilename = *argv;
		}

		argc--;
		argv++;
	}

	if (!OutOptions.ShaderFilename)
	{
		dprintf( "Provide a shader filename\n");
		return -1;
	}
	if (!OutOptions.Entry)
	{
		//default to Main
		dprintf( "No shader entrypoint specified, defaulting to 'Main'\n");
		OutOptions.Entry = "Main";
	}
	if (OutOptions.Frequency == HSF_InvalidFrequency)
	{
		//default to PixelShaders
		dprintf( "No shader frequency specified, defaulting to PS\n");
		OutOptions.Frequency = HSF_PixelShader;
	}
	if (OutOptions.Target == HCT_InvalidTarget)
	{
		//Default to GL3 shaders
		OutOptions.Target = HCT_FeatureLevelSM4;
	}
	if (OutOptions.Backend == HB_Invalid)
	{
		//Default to Glsl shaders
		dprintf("No backend specified, defaulting to Glsl\n");
		OutOptions.Backend = HB_Glsl;
	}

	return 0;
}

/* 
to debug issues which only show up when multiple shaders get compiled by the same process,
such as the ShaderCompilerWorker
*/ 
#define NUMBER_OF_MAIN_RUNS 1
#if NUMBER_OF_MAIN_RUNS > 1
int actual_main( int argc, char** argv);

int main( int argc, char** argv)
{
	int result = 0;

	for(int c = 0; c < NUMBER_OF_MAIN_RUNS; ++c)
	{
		char** the_real_argv = new char*[argc];

		for(int i = 0; i < argc; ++i)
		{
			the_real_argv[i] = strdup(argv[i]);
		}
		
		int the_result = actual_main(argc, the_real_argv);
		
		for(int i = 0; i < argc; ++i)
		{
			delete the_real_argv[i];
		}

		delete[] the_real_argv;


		result += the_result;
	}
	return result;
}

int actual_main( int argc, char** argv)
#else
int main( int argc, char** argv)
#endif
{
	char* HLSLShaderSource = 0;
	char* GLSLShaderSource = 0;
	char* ErrorLog = 0;

	SCmdOptions Options;
	{
		int Result = ParseCommandLine( argc-1, argv+1, Options);
		if (Result != 0)
		{
			return Result;
		}
	}
	
	HLSLShaderSource = LoadShaderFromFile(Options.ShaderFilename);
	if (!HLSLShaderSource)
	{
		dprintf( "Failed to open input shader %s\n", Options.ShaderFilename);
		return -2;
	}

	int Flags = HLSLCC_PackUniforms; // | HLSLCC_NoValidation | HLSLCC_PackUniforms;
	Flags |= Options.bNoPreprocess ? HLSLCC_NoPreprocess : 0;
	Flags |= Options.bDumpAST ? HLSLCC_PrintAST : 0;
	Flags |= Options.bFlattenUB ? HLSLCC_FlattenUniformBuffers : 0;
	Flags |= Options.bFlattenUBStructures ? HLSLCC_FlattenUniformBufferStructures : 0;
	Flags |= Options.bGroupFlattenedUB ? HLSLCC_GroupFlattenedUniformBuffers : 0;
	Flags |= Options.bCSE ? HLSLCC_ApplyCommonSubexpressionElimination : 0;
	Flags |= Options.bExpandExpressions ? HLSLCC_ExpandSubexpressions : 0;
	Flags |= Options.bSeparateShaderObjects ? HLSLCC_SeparateShaderObjects : 0;

	FGlslCodeBackend GlslCodeBackend(Flags);
	FGlslLanguageSpec GlslLanguageSpec;//(Options.Target == HCT_FeatureLevelES2);

	FCodeBackend* CodeBackend = &GlslCodeBackend;
	ILanguageSpec* LanguageSpec = &GlslLanguageSpec;
	switch (Options.Backend)
	{
	case HB_Glsl:
	default:
		CodeBackend = &GlslCodeBackend;
		LanguageSpec = &GlslLanguageSpec;
		Flags |= HLSLCC_DX11ClipSpace;
		break;
	}
	int Result = 0;

	{
		//FCRTMemLeakScope::BreakOnBlock(33758);
		FCRTMemLeakScope MemLeakScopeContext;
		FHlslCrossCompilerContext Context(Flags, Options.Frequency, Options.Target);
		if (Context.Init(Options.ShaderFilename, LanguageSpec))
		{
			FCRTMemLeakScope MemLeakScopeRun;
			Result = Context.Run(
				HLSLShaderSource,
				Options.Entry,
				CodeBackend,
				&GLSLShaderSource,
				&ErrorLog) ? 1 : 0;
		}

		if (GLSLShaderSource)
		{
			dprintf("GLSL Shader Source --------------------------------------------------------------\n");
			dprintf("%s",GLSLShaderSource);
			dprintf("\n-------------------------------------------------------------------------------\n\n");
		}

		if (ErrorLog)
		{
			dprintf("Error Log ----------------------------------------------------------------------\n");
			dprintf("%s",ErrorLog);
			dprintf("\n-------------------------------------------------------------------------------\n\n");
		}

		if (Options.OutFile && GLSLShaderSource)
		{
			FILE *fp = fopen( Options.OutFile, "w");

			if (fp)
			{
				fprintf( fp, "%s", GLSLShaderSource);
				fclose(fp);
			}
		}

		free(HLSLShaderSource);
		free(GLSLShaderSource);
		free(ErrorLog);

		if (DebugBuffer)
		{
			free(DebugBuffer);
			DebugBuffer = nullptr;
		}
	}

	return 0;
}

char* LoadShaderFromFile(const char* Filename)
{
	char* Source = 0;
	FILE* fp = fopen(Filename, "r");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t FileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		Source = (char*)malloc(FileSize + 1);
		size_t NumRead = fread(Source, 1, FileSize, fp);
		Source[NumRead] = 0;
		fclose(fp);
	}
	return Source;
}

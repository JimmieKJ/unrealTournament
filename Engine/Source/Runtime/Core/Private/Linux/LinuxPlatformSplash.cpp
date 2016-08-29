// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	SplashScreen.cpp: Splash screen for game/editor startup
================================================================================*/


#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "EngineVersion.h"
#include "EngineBuildSettings.h"

#if WITH_EDITOR

#include <GL/glcorearb.h>
#include <GL/glext.h>
#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "ImageWrapper.h"

#include "ft2build.h"
#include FT_FREETYPE_H

/**
 * Splash screen functions and static globals
 */

struct Rect
{
	int32 Top;
	int32 Left;
	int32 Right;
	int32 Bottom;
};


struct SplashFont
{
	enum State
	{
		NotLoaded = 0,
		Loaded
	};

	FT_Face Face;
	int32 Status;
};

FT_Library FontLibrary;

SplashFont FontSmall;
SplashFont FontNormal;
SplashFont FontLarge;

static SDL_Thread *GSplashThread = nullptr;
static SDL_Window *GSplashWindow = nullptr;
static SDL_Renderer *GSplashRenderer = nullptr;
static SDL_Texture *GSplashTexture = nullptr;
static SDL_Surface *GSplashScreenImage = nullptr;
static SDL_Surface *GSplashIconImage = nullptr;
static SDL_Surface *message = nullptr;
static FText GSplashScreenAppName;
static FText GSplashScreenText[ SplashTextType::NumTextTypes ];
static Rect GSplashScreenTextRects[ SplashTextType::NumTextTypes ];
static FString GSplashPath;
static FString GIconPath;
static IImageWrapperPtr GSplashImageWrapper;
static IImageWrapperPtr GIconImageWrapper;


static int32 SplashWidth = 0, SplashHeight = 0;
static unsigned char *ScratchSpace = nullptr;
/** 
 * Used for communication with a splash thread. 
 * Negative values -> thread is (or must be) stopped. 
 * >= 0 -> thread is running
 * > 0 -> there is an update that necessitates (re)rendering the string.
 */
static volatile int32 ThreadState = -1;
static int32 SplashBPP = 0;

//////////////////////////////////
// the below GL subset is a hack, as in general using GL for that

// subset of GL functions
/** List all OpenGL entry points used by Unreal. */
#define ENUM_GL_ENTRYPOINTS(EnumMacro) \
	EnumMacro(PFNGLENABLEPROC, glEnable) \
	EnumMacro(PFNGLDISABLEPROC, glDisable) \
	EnumMacro(PFNGLCLEARPROC, glClear) \
	EnumMacro(PFNGLCLEARCOLORPROC, glClearColor) \
	EnumMacro(PFNGLDRAWARRAYSPROC, glDrawArrays) \
	EnumMacro(PFNGLGENBUFFERSARBPROC, glGenBuffers) \
	EnumMacro(PFNGLBINDBUFFERARBPROC, glBindBuffer) \
	EnumMacro(PFNGLBUFFERDATAARBPROC, glBufferData) \
	EnumMacro(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffers) \
	EnumMacro(PFNGLMAPBUFFERARBPROC, glMapBuffer) \
	EnumMacro(PFNGLUNMAPBUFFERARBPROC, glDrawRangeElements) \
	EnumMacro(PFNGLBINDTEXTUREPROC, glBindTexture) \
	EnumMacro(PFNGLACTIVETEXTUREARBPROC, glActiveTexture) \
	EnumMacro(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
	EnumMacro(PFNGLGENTEXTURESPROC, glGenTextures) \
	EnumMacro(PFNGLCREATESHADERPROC, glCreateShader) \
	EnumMacro(PFNGLSHADERSOURCEPROC, glShaderSource) \
	EnumMacro(PFNGLCOMPILESHADERPROC, glCompileShader) \
	EnumMacro(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
	EnumMacro(PFNGLATTACHSHADERPROC, glAttachShader) \
	EnumMacro(PFNGLDETACHSHADERPROC, glDetachShader) \
	EnumMacro(PFNGLLINKPROGRAMPROC, glLinkProgram) \
	EnumMacro(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
	EnumMacro(PFNGLUSEPROGRAMPROC, glUseProgram) \
	EnumMacro(PFNGLDELETESHADERPROC, glDeleteShader) \
	EnumMacro(PFNGLCREATEPROGRAMPROC,glCreateProgram) \
	EnumMacro(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
	EnumMacro(PFNGLGETSHADERIVPROC, glGetShaderiv) \
	EnumMacro(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
	EnumMacro(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
	EnumMacro(PFNGLUNIFORM1FPROC, glUniform1f) \
	EnumMacro(PFNGLUNIFORM2FPROC, glUniform2f) \
	EnumMacro(PFNGLUNIFORM3FPROC, glUniform3f) \
	EnumMacro(PFNGLUNIFORM4FPROC, glUniform4f) \
	EnumMacro(PFNGLUNIFORM1IPROC, glUniform1i) \
	EnumMacro(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
	EnumMacro(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
	EnumMacro(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
	EnumMacro(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
	EnumMacro(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
	EnumMacro(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation) \
	EnumMacro(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
	EnumMacro(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
	EnumMacro(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
	EnumMacro(PFNGLISVERTEXARRAYPROC, glIsVertexArray) \
	EnumMacro(PFNGLTEXPARAMETERIPROC, glTexParameteri)

#define DEFINE_GL_ENTRYPOINTS(Type,Func) Type Func = NULL;
namespace GLFuncPointers	// see explanation in OpenGLLinux.h why we need the namespace
{
	ENUM_GL_ENTRYPOINTS(DEFINE_GL_ENTRYPOINTS);
};

using namespace GLFuncPointers;

//////////////////////////////////


/**
 * Returns the current shader log for a GLSL shader                   
 */
static FString GetGLSLShaderLog( GLuint Shader )
{
	GLint Len;
	FString ShaderLog;

	glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &Len);

	if(Len > 0)
	{
		GLsizei ActualLen;
		GLchar *Log = new GLchar[Len];

		glGetShaderInfoLog(Shader, Len, &ActualLen, Log);
	
		ShaderLog = UTF8_TO_TCHAR( Log );

		delete[] Log;
	}

	return ShaderLog;
}

static FString GetGLSLProgramLog( GLuint Program )
{
	GLint Len;
	FString ProgramLog;
	glGetProgramiv( Program, GL_INFO_LOG_LENGTH, &Len );

	if( Len > 0 )
	{
		GLchar* Log = new GLchar[Len];

		GLsizei ActualLen;
		glGetProgramInfoLog( Program, Len, &ActualLen, Log );

		ProgramLog = UTF8_TO_TCHAR( Log );

		delete[] Log;
	}

	return ProgramLog;
}

//---------------------------------------------------------
static int CompileShaderFromString( const FString& Source, GLuint& ShaderID, GLenum ShaderType )
{
	// Create a new shader ID.
	ShaderID = glCreateShader( ShaderType );
	GLint CompileStatus = GL_FALSE;

	check( ShaderID );
	
	// Allocate a buffer big enough to store the string in ascii format
	ANSICHAR* Chars[2] = {nullptr};
	// pass the #define along to the shader
#if PLATFORM_USES_ES2
	Chars[0] = (ANSICHAR*)"#define PLATFORM_USES_ES2 1\n\n#define PLATFORM_LINUX 0\n";
#elif PLATFORM_LINUX
	Chars[0] = (ANSICHAR*)"#version 150\n\n#define PLATFORM_USES_ES2 0\n\n#define PLATFORM_LINUX 1\n";
#else
	Chars[0] = (ANSICHAR*)"#version 120\n\n#define PLATFORM_USES_ES2 0\n\n#define PLATFORM_LINUX 0\n";
#endif
	Chars[1] = new ANSICHAR[Source.Len()+1];
	FCStringAnsi::Strcpy(Chars[1], Source.Len() + 1, TCHAR_TO_ANSI(*Source));

	// give opengl the source code for the shader
	glShaderSource( ShaderID, 2, (const ANSICHAR**)Chars, NULL );
	delete[] Chars[1];

	// Compile the shader and check for success
	glCompileShader( ShaderID );

	glGetShaderiv( ShaderID, GL_COMPILE_STATUS, &CompileStatus );
	if( CompileStatus == GL_FALSE )
	{
		// The shader did not compile.  Display why it failed.
		FString Log = GetGLSLShaderLog( ShaderID );

		checkf(false, TEXT("Failed to compile shader: %s\n"), *Log );

		// Delete the shader since it failed.
		glDeleteShader( ShaderID );
		ShaderID = 0;
		return -1;
	}
	
	return 0;
}

//---------------------------------------------------------
static int CompileShaderFromFile( const FString& Filename, GLuint& ShaderID, GLenum ShaderType )
{
	// Load the file to a string
	FString Source;
	bool bFileFound = FFileHelper::LoadFileToString( Source, *Filename );
	check(bFileFound);

	return CompileShaderFromString(Source, ShaderID, ShaderType);
}

static int LinkShaders(GLuint& ShaderProgram, GLuint& VertexShader, GLuint& FragmentShader)
{
	// Create a new program id and attach the shaders
	ShaderProgram = glCreateProgram();
	glAttachShader( ShaderProgram, VertexShader );
	glAttachShader( ShaderProgram, FragmentShader );

	// Set up attribute locations for per vertex data
	glBindAttribLocation(ShaderProgram, 0, "InPosition");

	// Link the shaders
	glLinkProgram( ShaderProgram );

	// Check to see if linking succeeded
	GLint LinkStatus;
	glGetProgramiv( ShaderProgram, GL_LINK_STATUS, &LinkStatus );
	if( LinkStatus == GL_FALSE )
	{
		return -1;
	}

	return 0;
}

//---------------------------------------------------------
static int32 OpenFonts ( )
{
	FontSmall.Status = SplashFont::NotLoaded;
	FontNormal.Status = SplashFont::NotLoaded;
	FontLarge.Status = SplashFont::NotLoaded;
	
	if (FT_Init_FreeType(&FontLibrary))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to initialize font library."));		
		return -1;
	}
	
	// small font face
	FString FontPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"));
	
	if (FT_New_Face(FontLibrary, TCHAR_TO_UTF8(*FontPath), 0, &FontSmall.Face))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to open small font face for splash screen."));		
	}
	else
	{
		FT_Set_Pixel_Sizes(FontSmall.Face, 0, 10);
		FontSmall.Status = SplashFont::Loaded;
	}


	// normal font face
	FontPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"));
	
	if (FT_New_Face(FontLibrary, TCHAR_TO_UTF8(*FontPath), 0, &FontNormal.Face))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to open normal font face for splash screen."));		
	}
	else
	{
		FT_Set_Pixel_Sizes(FontNormal.Face, 0, 12);
		FontNormal.Status = SplashFont::Loaded;
	}	
	
	// large font face
	FontPath = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"));
	
	if (FT_New_Face(FontLibrary, TCHAR_TO_UTF8(*FontPath), 0, &FontLarge.Face))
	{
		UE_LOG(LogHAL, Error, TEXT("*** Unable to open large font face for splash screen."));		
	}
	else
	{
		FT_Set_Pixel_Sizes (FontLarge.Face, 0, 40);
		FontLarge.Status = SplashFont::Loaded;
	}
	
	return 0;
}


//---------------------------------------------------------
static void DrawCharacter(int32 penx, int32 peny, FT_GlyphSlot Glyph, int32 CurTypeIndex, float red, float green, float blue)
{

	// drawing boundaries
	int32 xmin = GSplashScreenTextRects[CurTypeIndex].Left;
	int32 xmax = GSplashScreenTextRects[CurTypeIndex].Right;
	int32 ymax = GSplashScreenTextRects[CurTypeIndex].Bottom;
	int32 ymin = GSplashScreenTextRects[CurTypeIndex].Top;
	
	// glyph dimensions
	int32 cwidth = Glyph->bitmap.width;
	int32 cheight = Glyph->bitmap.rows;
	int32 cpitch = Glyph->bitmap.pitch;
		
	unsigned char *pixels = Glyph->bitmap.buffer;
	
	// draw glyph raster to texture
	for (int y=0; y<cheight; y++)
	{
		for (int x=0; x<cwidth; x++)
		{
			// find pixel position in splash image
			int xpos = penx + x;
			int ypos = peny + y - (Glyph->metrics.horiBearingY >> 6);
			
			// make sure pixel is in drawing rectangle
			if (xpos < xmin || xpos >= xmax || ypos < ymin || ypos >= ymax)
				continue;

			// get index of pixel in glyph bitmap			
			int32 src_idx = (y * cpitch) + x;
			int32 dst_idx = (ypos * SplashWidth + xpos) * SplashBPP;
			
			// write pixel
			float alpha = pixels[src_idx] / 255.0f;
			
			ScratchSpace[dst_idx] = ScratchSpace[dst_idx]*(1.0 - alpha) + alpha*red;
			dst_idx++;
			ScratchSpace[dst_idx] = ScratchSpace[dst_idx]*(1.0 - alpha) + alpha*green;
			dst_idx++;
			ScratchSpace[dst_idx] = ScratchSpace[dst_idx]*(1.0 - alpha) + alpha*blue;
		}
	}
}


//---------------------------------------------------------
static int RenderString (GLuint tex_idx)
{
	FT_UInt last_glyph = 0;
	FT_Vector kern;	
	bool bRightJustify = false;
	float red, blue, green; // font color

	// clear the rendering scratch pad.
	FMemory::Memcpy(ScratchSpace, GSplashScreenImage->pixels, SplashWidth*SplashHeight*SplashBPP);

	// draw each type of string
	for (int CurTypeIndex=0; CurTypeIndex<SplashTextType::NumTextTypes; CurTypeIndex++)
	{
		SplashFont *Font;
		
		// initial pen position
		uint32 penx = GSplashScreenTextRects[ CurTypeIndex].Left;
		uint32 peny = GSplashScreenTextRects[ CurTypeIndex].Bottom;
		kern.x = 0;		
		
		// Set font color and text position
		if (CurTypeIndex == SplashTextType::StartupProgress)
		{
			red = green = blue = 200.0f;
			Font = &FontSmall;
		}
		else if (CurTypeIndex == SplashTextType::VersionInfo1)
		{
			red = green = blue = 240.0f;
			Font = &FontNormal;
		}
		else if (CurTypeIndex == SplashTextType::GameName)
		{
			red = green = blue = 240.0f;
			Font = &FontLarge;
			penx = GSplashScreenTextRects[ CurTypeIndex].Right;
			bRightJustify = true;
		}
		else
		{
			red = green = blue = 160.0f;
			Font = &FontSmall;
		}
		
		// sanity check: make sure we have a font loaded.
		if (Font->Status == SplashFont::NotLoaded)
			continue;

		// adjust verticle pos to allow for descenders
		peny += Font->Face->descender >> 6;

		// convert strings to glyphs and place them in bitmap.
		FString Text = GSplashScreenText[CurTypeIndex].ToString();
			
		for (int i=0; i<Text.Len(); i++)
		{
			FT_ULong charcode;
			
			// fetch next glyph
			if (bRightJustify)
			{
				charcode = (Uint32)(Text[Text.Len() - i - 1]);
			}
			else
			{
				charcode = (Uint32)(Text[i]);				
			}
						
			FT_UInt glyph_idx = FT_Get_Char_Index(Font->Face, charcode);
			FT_Load_Glyph(Font->Face, glyph_idx, FT_LOAD_DEFAULT);

			
			if (Font->Face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
			{
				FT_Render_Glyph(Font->Face->glyph, FT_RENDER_MODE_NORMAL);
			}


			// pen advance and kerning
			if (bRightJustify)
			{
				if (last_glyph != 0)
				{
					FT_Get_Kerning(Font->Face, glyph_idx, last_glyph, FT_KERNING_DEFAULT, &kern);
				}
				
				penx -= (Font->Face->glyph->metrics.horiAdvance - kern.x) >> 6;	
			}
			else
			{
				if (last_glyph != 0)
				{
					FT_Get_Kerning(Font->Face, last_glyph, glyph_idx, FT_KERNING_DEFAULT, &kern);
				}	
			}

			last_glyph = glyph_idx;

			// draw character
			DrawCharacter(penx, peny, Font->Face->glyph,CurTypeIndex, red, green, blue);
						
			if (!bRightJustify)
			{
				penx += (Font->Face->glyph->metrics.horiAdvance - kern.x) >> 6;
			}
		}
				
		// store rendered text as texture
		glBindTexture(GL_TEXTURE_2D, tex_idx);
		
		bool bIsBGR = GSplashScreenImage->format->Rmask > GSplashScreenImage->format->Bmask;
		if (SplashBPP == 3)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
				GSplashScreenImage->w, GSplashScreenImage->h,
				0, bIsBGR ? GL_BGR : GL_RGB, GL_UNSIGNED_BYTE, ScratchSpace);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				GSplashScreenImage->w, GSplashScreenImage->h,
				0, bIsBGR ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, ScratchSpace);			
		}
	}

	return 0;
}

/**
 * @brief Helper function to load an image in any format.
 *
 * @param ImagePath an (absolute) path to the image
 * @param OutImageWrapper a pointer to IImageWrapper which may be needed to hold the data (should outlive returned SDL_Surface)
 *
 * @return Splash surface
 */
static SDL_Surface* LinuxSplash_LoadImage(const TCHAR* InImagePath, IImageWrapperPtr & OutImageWrapper)
{
	TArray<uint8> RawFileData;
	FString ImagePath(InImagePath);

	// Load the image buffer first (unless it's BMP)
	if (!ImagePath.EndsWith(TEXT("bmp"), ESearchCase::IgnoreCase) && FFileHelper::LoadFileToArray(RawFileData, *ImagePath))
	{
		auto& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		auto Format = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
		OutImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

		if (OutImageWrapper.IsValid() && OutImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
		{
			const TArray<uint8>* RawData = nullptr;

			if (OutImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
			{
				uint32 Bmask = 0x000000ff;
				uint32 Gmask = 0x0000ff00;
				uint32 Rmask = 0x00ff0000;
				uint32 Amask = 0xff000000;

				return SDL_CreateRGBSurfaceFrom((void*)RawData->GetData(),
					OutImageWrapper->GetWidth(), OutImageWrapper->GetHeight(),
					32, OutImageWrapper->GetWidth() * 4,
					Rmask, Gmask, Bmask, Amask);
			}
		}
	}

	// If for some reason the image cannot be loaded, use the default BMP function
	return SDL_LoadBMP(TCHAR_TO_UTF8(*ImagePath));
}

void LinuxSplash_TearDownSplashResources();

/** Helper function to init resources used by the splash thread */
bool LinuxSplash_InitSplashResources()
{
	checkf(GSplashScreenImage == nullptr, TEXT("LinuxSplash_InitSplashResources() has been called multiple times."));
	checkf(GSplashWindow == nullptr, TEXT("LinuxSplash_InitSplashResources() has been called multiple times."));
	checkf(GSplashIconImage == nullptr, TEXT("LinuxSplash_InitSplashResources() has been called multiple times."));

	if (!FPlatformMisc::PlatformInitMultimedia()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Warning, TEXT("LinuxSplash_InitSplashResources() : PlatformInitMultimedia() failed, there will be no splash."));
		return false;
	}

	// load splash .bmp image
	GSplashScreenImage = LinuxSplash_LoadImage(*GSplashPath, GSplashImageWrapper);
	
	if (GSplashScreenImage == nullptr)
	{
		UE_LOG(LogHAL, Warning, TEXT("LinuxSplash_InitSplashResources() : Could not load splash BMP! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return false;
	}

	// create splash window
	GSplashWindow = SDL_CreateWindow(NULL,
									SDL_WINDOWPOS_CENTERED,
									SDL_WINDOWPOS_CENTERED,
									GSplashScreenImage->w,
									GSplashScreenImage->h,
									SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
									);
	if (GSplashWindow == nullptr)
	{
		UE_LOG(LogHAL, Error, TEXT("LinuxSplash_InitSplashResources() : Splash screen window could not be created! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		LinuxSplash_TearDownSplashResources();
		return false;
	}

	SDL_SetWindowTitle( GSplashWindow, TCHAR_TO_UTF8(*GSplashScreenAppName.ToString()) );

	GSplashIconImage = LinuxSplash_LoadImage(*GIconPath, GIconImageWrapper);

	if (GSplashIconImage != nullptr)
	{
		SDL_SetWindowIcon(GSplashWindow, GSplashIconImage);
	}
	else
	{
		UE_LOG(LogHAL, Warning, TEXT("LinuxSplash_InitSplashResources() : Splash icon could not be created! SDL_Error: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		LinuxSplash_TearDownSplashResources();
		return false;
	}

	return true;
}

/** Helper function to tear down resources used by the splash thread */
void LinuxSplash_TearDownSplashResources()
{
	if (GSplashIconImage)
	{
		SDL_FreeSurface(GSplashIconImage);
		GSplashIconImage = nullptr;
	}
	GIconImageWrapper.Reset();

	if (GSplashWindow)
	{
		SDL_DestroyWindow(GSplashWindow);
		GSplashWindow = nullptr;
	}

	if (GSplashScreenImage)
	{
		SDL_FreeSurface(GSplashScreenImage);
		GSplashScreenImage = nullptr;
		GSplashImageWrapper.Reset();
	}

	// do not deinit SDL here
}

/**
 * Thread function that actually creates the window and
 * renders its contents.
 */
static int StartSplashScreenThread(void *ptr)
{	
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0 );

	SDL_GLContext Context = SDL_GL_CreateContext( GSplashWindow );

	if (Context == NULL)
	{
		UE_LOG(LogHAL, Error, TEXT("Splash screen SDL_GL_CreateContext failed: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return -1;		
	}

	// Initialize all entry points required by Unreal.
	#define GET_GL_ENTRYPOINTS(Type,Func) GLFuncPointers::Func = reinterpret_cast<Type>(SDL_GL_GetProcAddress(#Func));
	ENUM_GL_ENTRYPOINTS(GET_GL_ENTRYPOINTS);

	if (SDL_GL_MakeCurrent( GSplashWindow, Context ) != 0) 
	{
		UE_LOG(LogHAL, Error, TEXT("Splash screen SDL_GL_MakeCurrent failed: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return -1;
	}

	// Initialize Shaders, Programs, etc.
	GLuint VertexShader 	= 0;
	GLuint FragmentShader 	= 0;
	GLuint ShaderProgram	= 0;
	GLuint SplashTextureID 	= 0;

	// Compile Vertex and Fragment Shaders from file.
	if (CompileShaderFromFile( FString::Printf( TEXT("%sShaders/StandaloneRenderer/OpenGL/SplashVertexShader.glsl"), *FPaths::EngineDir()), VertexShader, GL_VERTEX_SHADER) < 0)
	{
		UE_LOG(LogHAL, Error, TEXT("Splash screen CompileShaderFromFile failed."));
		return -1;		
	}

	if (CompileShaderFromFile(FString::Printf( TEXT("%sShaders/StandaloneRenderer/OpenGL/SplashFragmentShader.glsl"), *FPaths::EngineDir()), FragmentShader, GL_FRAGMENT_SHADER) < 0)
	{
		UE_LOG(LogHAL, Error, TEXT("Splash screen CompileShaderFromFile failed."));
		return -1;		
	}

	// Create Shader Program and link it.
	if(LinkShaders(ShaderProgram, VertexShader, FragmentShader) < 0) 
	{
		FString Log = GetGLSLProgramLog( ShaderProgram );
		UE_LOG(LogHAL, Error, TEXT("Failed to link GLSL program: %s"), *Log);
	}

	// Returns the reference to the Splash Texture from the Shader.
	SplashTextureID = glGetUniformLocation( ShaderProgram, "SplashTexture" );

	// Create Vertex Array Object. We need that to be conform with OpenGL 3.2+ spec.
	GLuint VertexArrayObject;
	glGenVertexArrays(1, &VertexArrayObject);
	glBindVertexArray(VertexArrayObject);
	
	// Create Vertex Buffer and upload data.
	GLuint VertexBuffer;
	glGenBuffers(1, &VertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
	float screen_vertex [] = 
	{
		-1.0, -1.0f,
		-1.0f, 1.0f,
		1.0f, 1.0f,

		1.0f, 1.0f,
		1.0f, -1.0f,
		-1.0f, -1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*12, (GLvoid*)screen_vertex, GL_DYNAMIC_DRAW );

	// create texture slot
	GLuint texture;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	// set filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// load splash image as texture in opengl
	bool bIsBGR = GSplashScreenImage->format->Rmask > GSplashScreenImage->format->Bmask;
	if (GSplashScreenImage->format->BytesPerPixel == 3)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
			GSplashScreenImage->w, GSplashScreenImage->h,
			0, bIsBGR ? GL_BGR : GL_RGB, GL_UNSIGNED_BYTE, GSplashScreenImage->pixels);
	}
	else if (GSplashScreenImage->format->BytesPerPixel == 4)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			GSplashScreenImage->w, GSplashScreenImage->h,
			0, bIsBGR ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, GSplashScreenImage->pixels);		
	}
	else
	{
		UE_LOG(LogHAL, Warning, TEXT("Splash BMP image has unsupported format"));
		return -1;
	}

	// remember bmp stats
	SplashWidth = GSplashScreenImage->w;
	SplashHeight =  GSplashScreenImage->h;
	SplashBPP = GSplashScreenImage->format->BytesPerPixel;

	if (SplashWidth <= 0 || SplashHeight <= 0)
	{
		UE_LOG(LogHAL, Warning, TEXT("Invalid splash image dimensions."));		
		return -1;
	}

	// allocate scratch space for rendering text
	ScratchSpace = reinterpret_cast<unsigned char *>(FMemory::Malloc(SplashHeight*SplashWidth*SplashBPP));

	// Setup bounds for game name
	GSplashScreenTextRects[ SplashTextType::GameName ].Top = 0;
	GSplashScreenTextRects[ SplashTextType::GameName ].Bottom = 50;
	GSplashScreenTextRects[ SplashTextType::GameName ].Left = 12;
	GSplashScreenTextRects[ SplashTextType::GameName ].Right = SplashWidth - 12;

	// Setup bounds for version info text 1
	GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].Top = SplashHeight - 60;
	GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].Bottom = SplashHeight - 40;
	GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].Left = 10;
	GSplashScreenTextRects[ SplashTextType::VersionInfo1 ].Right = SplashWidth - 10;

	// Setup bounds for copyright info text
	if (GIsEditor)
	{
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].Top = SplashHeight - 44;
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].Bottom = SplashHeight - 24;
	}
	else
	{
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].Top = SplashHeight - 16;
		GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].Bottom = SplashHeight - 6;
	}

	GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].Left = 10;
	GSplashScreenTextRects[ SplashTextType::CopyrightInfo ].Right = SplashWidth - 20;

	// Setup bounds for startup progress text
	GSplashScreenTextRects[ SplashTextType::StartupProgress ].Top = SplashHeight - 20;
	GSplashScreenTextRects[ SplashTextType::StartupProgress ].Bottom = SplashHeight;
	GSplashScreenTextRects[ SplashTextType::StartupProgress ].Left = 10;
	GSplashScreenTextRects[ SplashTextType::StartupProgress ].Right = SplashWidth - 20;

	OpenFonts();

	glDisable(GL_DEPTH_TEST);

	// drawing loop
	while (ThreadState >= 0)
	{
		SDL_Event event;
		// Poll events otherwise the window will get dark or
		// on some Desktops it will complain that the thread
		// does not work anymore.
		while (SDL_PollEvent(&event))
		{
			// intentionally empty
		}

		SDL_Delay(300);
		
		// Activate Shader Program, Texture etc.
		glBindVertexArray(VertexArrayObject);
		glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
		glUseProgram(ShaderProgram);
		glActiveTexture(GL_TEXTURE0);
		glUniform1i(SplashTextureID, 0);

		if (ThreadState > 0)
		{
			RenderString(texture);
			ThreadState--;
		}

		// Set stream positions and draw.
		glEnableVertexAttribArray(0); 
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Disable Shader Program, VAO, VBO etc.
		glDisableVertexAttribArray(0); 
		glUseProgram(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		SDL_GL_SwapWindow( GSplashWindow );
	}

	// clean up
	glDeleteBuffers(1, &VertexBuffer);
	glDeleteVertexArrays(1, &VertexArrayObject);
	glDetachShader(ShaderProgram, VertexShader);
	glDetachShader(ShaderProgram, FragmentShader);
	glDeleteShader(VertexShader);
	glDeleteShader(FragmentShader);
	glDeleteProgram(ShaderProgram);
 
	if (ScratchSpace)
	{
		FMemory::Free(ScratchSpace);
		ScratchSpace = NULL;
	}

	if (FontSmall.Status == SplashFont::Loaded)
	{
		FT_Done_Face(FontSmall.Face);
	}

	if (FontNormal.Status == SplashFont::Loaded)
	{
		FT_Done_Face(FontNormal.Face);
	}

	if (FontLarge.Status == SplashFont::Loaded)
	{
		FT_Done_Face(FontLarge.Face);
	}

	FT_Done_FreeType(FontLibrary);

	SDL_GL_DeleteContext(Context);
	FMemory::Free(ScratchSpace);

	// set the thread state to -1 to let the caller know we're done (FIXME: can be done without busy-loops)
	ThreadState = -1;

	return 0;
}

#endif //WITH_EDITOR

/**
 * Sets the text displayed on the splash screen (for startup/loading progress)
 *
 * @param	InType		Type of text to change
 * @param	InText		Text to display
 */
static void StartSetSplashText( const SplashTextType::Type InType, const FText& InText )
{
#if WITH_EDITOR
	// Update splash text
	GSplashScreenText[ InType ] = InText;
#endif
}


/**
 * Open a splash screen if there's not one already and if it's not disabled.
 *
 */
void FLinuxPlatformSplash::Show( )
{
#if WITH_EDITOR
	// need to do a splash screen?
	if(GSplashThread || FParse::Param(FCommandLine::Get(),TEXT("NOSPLASH")) == true)
	{
		return;
	}

	// decide on which splash screen to show
	const FText GameName = FText::FromString(FApp::GetGameName());

	bool IsCustom = false;

	// first look for the splash, do not init anything if not found
	{
		const TCHAR* EditorSplashes[] =
		{
			TEXT("EdSplash"),
			TEXT("EdSplashDefault"),
			nullptr
		};

		const TCHAR* GameSplashes[] =
		{
			TEXT("Splash"),
			TEXT("SplashDefault"),
			nullptr
		};

		const TCHAR** SplashesToTry = GIsEditor? EditorSplashes : GameSplashes;
		bool bSplashFound = false;
		for (const TCHAR* SplashImage = *SplashesToTry; SplashImage != nullptr && !bSplashFound; SplashImage = *(++SplashesToTry))
		{
			if (GetSplashPath(SplashImage, GSplashPath, IsCustom))
			{
				bSplashFound = true;
			}
		}

		if (!bSplashFound)
		{
			UE_LOG(LogHAL, Warning, TEXT("Splash screen image not found."));
			return;	// early out
		}
	}

	// look for the icon separately, also avoid initialization if not found
	{
		const TCHAR* EditorIcons[] =
		{
			TEXT("EdIcon"),
			TEXT("EdIconDefault"),
			nullptr
		};

		const TCHAR* GameIcons[] =
		{
			TEXT("Icon"),
			TEXT("IconDefault"),
			nullptr
		};

		const TCHAR** IconsToTry = GIsEditor? EditorIcons : GameIcons;
		bool bIconFound = false;
		for (const TCHAR* IconImage = *IconsToTry; IconImage != nullptr && !bIconFound; IconImage = *(++IconsToTry))
		{
			bool bDummy;
			if (GetSplashPath(IconImage, GIconPath, bDummy))
			{
				bIconFound = true;
			}
		}

		if (!bIconFound)
		{
			UE_LOG(LogHAL, Warning, TEXT("Game icon not found."));
			return;	// early out
		}
	}

	// Don't set the game name if the splash screen is custom.
	if ( !IsCustom )
	{
		StartSetSplashText( SplashTextType::GameName, GameName );
	}

	// In the editor, we'll display loading info
	if( GIsEditor )
	{
		// Set initial startup progress info
		{
			StartSetSplashText( SplashTextType::StartupProgress,
				NSLOCTEXT("UnrealEd", "SplashScreen_InitialStartupProgress", "Loading..." ) );
		}

		// Set version info
		{
			const FText Version = FText::FromString( FEngineVersion::Current().ToString( FEngineBuildSettings::IsPerforceBuild() ? EVersionComponent::Branch : EVersionComponent::Patch ) );

			FText VersionInfo;
			FText AppName;
			if( GameName.IsEmpty() )
			{
				VersionInfo = FText::Format( NSLOCTEXT( "UnrealEd", "UnrealEdTitleWithVersionNoGameName_F", "Unreal Editor {0}" ), Version );
				AppName = NSLOCTEXT( "UnrealEd", "UnrealEdTitleNoGameName_F", "Unreal Editor" );
			}
			else
			{
				VersionInfo = FText::Format( NSLOCTEXT( "UnrealEd", "UnrealEdTitleWithVersion_F", "Unreal Editor {0}  -  {1}" ), Version, GameName );
				AppName = FText::Format( NSLOCTEXT( "UnrealEd", "UnrealEdTitle_F", "Unreal Editor - {0}" ), GameName );
			}

			StartSetSplashText( SplashTextType::VersionInfo1, VersionInfo );

			// Change the window text (which will be displayed in the taskbar)
			GSplashScreenAppName = AppName;
		}

		// Display copyright information in editor splash screen
		{
			const FText CopyrightInfo = NSLOCTEXT( "UnrealEd", "SplashScreen_CopyrightInfo", "Copyright \x00a9 1998-2016   Epic Games, Inc.   All rights reserved." );
			StartSetSplashText( SplashTextType::CopyrightInfo, CopyrightInfo );
		}
	}

	// init splash LinuxSplash_InitSplashResources
	if (!LinuxSplash_InitSplashResources())
	{
		return;
	}

	// Create rendering thread to display splash
	GSplashThread = SDL_CreateThread(StartSplashScreenThread, "StartSplashScreenThread", (void *)NULL);

	if (GSplashThread == nullptr)
	{
		UE_LOG(LogHAL, Error, TEXT("Splash screen SDL_CreateThread failed: %s"), UTF8_TO_TCHAR(SDL_GetError()));
	}
	else
	{
		SDL_DetachThread(GSplashThread);
		ThreadState = 1;
	}
#endif //WITH_EDITOR
}


/**
* Done with splash screen. Close it and clean up.
*/
void FLinuxPlatformSplash::Hide()
{
#if WITH_EDITOR
	// signal thread it's time to quit
	if (ThreadState >= 0)	// if there's a thread at all...
	{
		ThreadState = -99;
		// wait for the thread to be done before tearing it tearing it down (it will set the ThreadState to 0)
		while (ThreadState != -1)
		{
			// busy loop!
			FPlatformProcess::Sleep(0.01f);
		}
	}

	// tear down resources that thread used (safe to call even if they were not initialized
	LinuxSplash_TearDownSplashResources();

	GSplashThread = nullptr;
#endif
}


/**
* Sets the text displayed on the splash screen (for startup/loading progress)
*
* @param	InType		Type of text to change
* @param	InText		Text to display
*/
void FLinuxPlatformSplash::SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
#if WITH_EDITOR
	// We only want to bother drawing startup progress in the editor, since this information is
	// not interesting to an end-user (also, it's not usually localized properly.)	

	if (InType == SplashTextType::CopyrightInfo || GIsEditor)
	{
		bool bWasUpdated = false;
		{
			// Update splash text
			if (FCString::Strcmp( InText, *GSplashScreenText[ InType ].ToString() ) != 0)
			{
				GSplashScreenText[ InType ] = FText::FromString( InText );
				bWasUpdated = true;
			}
		}

		if (bWasUpdated && GSplashThread && ThreadState >= 0)
		{
			ThreadState++;
		}
	}
#endif //WITH_EDITOR
}

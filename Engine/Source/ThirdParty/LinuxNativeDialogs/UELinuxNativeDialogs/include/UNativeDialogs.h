// Author: Damian Kaczmarek <damian@codecharm.co.uk>

#ifndef UNATIVEDIALOGS_H
#define UNATIVEDIALOGS_H

/*
	Usage:

	struct UFileDialogHints hints = DEFAULT_UFILEDIALOGHINTS;
	<setup your hints>
	dialog = UFileDialog_Create(hints);

	and in the event loop call:

	bool status = UFileDialog_ProcessEvents(dialog);
	if(!status) {
		const UFileDialogResult* result =  UFileDialog_Result(dialog);
		<do something with the result>
		UFileDialog_Destroy(dialog);
	}
*/


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct UFileDialog;
typedef struct UFileDialog UFileDialog;

typedef enum
{
	UFileDialogActionOpen = 0,
	UFileDialogActionOpenMultiple = 1,
	UFileDialogActionOpenDirectory = 2,
	UFileDialogActionSave = 3
} UFileDialogActionType;


struct UFileDialogHints
{
	UFileDialogActionType Action;

	//! (optional) for example "All C++ files (*.cpp *.cc *.C *.cxx *.c++)""
	const char* NameFilter;
	//! (optional) absolute directory from which the dialog whould open
	const char* InitialDirectory;
	//! (optional) title to put on open file dialog
	const char* WindowTitle;
	//! (optional) select a default file in the dialog
	const char* DefaultFile;
};

#define DEFAULT_UFILEDIALOGHINTS { \
	UFileDialogActionOpen, \
	"All files (*.*)", \
	NULL, \
	NULL \
}


typedef struct UFileDialogResult
{
	int count;
	const char** selection;
} UFileDialogResult;

/**
 * Initializes the library
 */
bool ULinuxNativeDialogs_Initialize();
void ULinuxNativeDialogs_Shutdown();

UFileDialog* UFileDialog_Create(struct UFileDialogHints* hints);
bool UFileDialog_ProcessEvents(UFileDialog* handle);
// valid after Process returning false
const UFileDialogResult* UFileDialog_Result(UFileDialog* handle);
void UFileDialog_Destroy(UFileDialog* handle);

struct UFontDialog;
typedef struct UFontDialog UFontDialog;

typedef enum {
	UFontDialogNormal = 0,
	UFontDialogBold = 1,
	UFontDialogItalic = 2,
	UFontDialogBoldItalic = 3
} UFontDialogFontFlags;

struct UFontDialogHints
{
	float InitialPointSize;
	int InitialPixelSize;
	const char* InitialFontName;
	const char* WindowTitle;
};

#define DEFAULT_UFONTDIALOGHINTS { \
	0.0f, \
	0 ,  \
	NULL, \
	NULL \
}

typedef struct UFontDialogResult
{
	const char* fontName;
	float pointSize;
	int pixelSize;
	UFontDialogFontFlags flags;
} UFontDialogResult;

UFontDialog* UFontDialog_Create(struct UFontDialogHints* hints);
bool UFontDialog_ProcessEvents(UFontDialog* handle);
// valid after Process returning false
const UFontDialogResult* UFontDialog_Result(UFontDialog* handle);
void UFontDialog_Destroy(UFontDialog* handle);

#ifdef __cplusplus
}
#endif

#endif // UNATIVEDIALOGS_H

#include <Windows.h>

__declspec(dllexport) void ExampleLibraryFunction()
{
	MessageBox(NULL, TEXT("Hello world!"), NULL, MB_OK);
}

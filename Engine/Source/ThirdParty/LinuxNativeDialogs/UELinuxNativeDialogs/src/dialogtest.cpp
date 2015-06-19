#include "UNativeDialogs.h"
#include <cstdio>
#include <unistd.h>


void test_dialog(struct UFileDialogHints* hints)
{
  UFileDialog* dialog = UFileDialog_Create(hints);

  while(UFileDialog_ProcessEvents(dialog)) {
    usleep(5000);
  }

  //UFileDialog_Result(dialog)

  const UFileDialogResult* result =  UFileDialog_Result(dialog);
  printf("Selected files: %i\n", result->count);
  for(int i = 0;i < result->count;++i) {
    printf("File: %s\n", result->selection[i]);
  }

     
  UFileDialog_Destroy(dialog);
}

void test_fontdialog(struct UFontDialogHints* hints)
{
  UFontDialog* dialog = UFontDialog_Create(hints);

  while(UFontDialog_ProcessEvents(dialog)) {
    usleep(5000);
  }
  const UFontDialogResult* result =  UFontDialog_Result(dialog);
  printf("Selected font: %s - %.1f\n", result->fontName, result->pointSize);
  printf("Style: %s%s\n", ((int)result->flags)&UFontDialogBold ? "Bold":"", ((int)result->flags)&UFontDialogItalic ? "Italic":"");

  UFontDialog_Destroy(dialog);
}

int main(void)
{

  {
    struct UFontDialogHints hints = DEFAULT_UFONTDIALOGHINTS;
    hints.InitialFontName = "Arial";
    hints.InitialPixelSize = 20;
    hints.WindowTitle = "Open Font";

    test_fontdialog(&hints);
  }

  {
    struct UFileDialogHints hints = DEFAULT_UFILEDIALOGHINTS;

    hints.InitialDirectory = "/home/";

    hints.NameFilter = "Image files (*.jpg *.png)";


    hints.Action = UFileDialogActionOpen;
    hints.WindowTitle = "Open Single File Test";
    test_dialog(&hints);

    hints.Action = UFileDialogActionOpenMultiple;
    hints.WindowTitle = "Open Multiple Files Test";
    test_dialog(&hints);

    hints.Action = UFileDialogActionOpenDirectory;
    hints.WindowTitle = "Open Directory Test";
    test_dialog(&hints);

    hints.Action = UFileDialogActionSave;
    hints.WindowTitle = "Save As Test";
    test_dialog(&hints);
  }

  return 0;
}
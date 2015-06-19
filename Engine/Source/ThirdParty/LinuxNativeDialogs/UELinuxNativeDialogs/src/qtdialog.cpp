#include <assert.h>
#include "UNativeDialogs.h"

#include <QApplication>
#include <QFileDialog>
#include <QFontDialog>


// KDE integration fails after QApplication is destroyed
// leave it as singleton and lazy initialize
//static QApplication* qApp;

bool ULinuxNativeDialogs_Initialize() {
  /*
    FAT NOTE. This plugin shouldn't be unloaded as things
    could break. Doing multiple QApplication initializations
    and destructions as long as there is only one at a time
    alive is perfectly valid for Qt but unfortunately KDE
    integration is not as forthcoming and a single instance
    of QApplication needs to be left alive
    */
  if(!qApp) {
    static int argc;
    new QApplication(argc, nullptr);
  }

  return true;
}

void ULinuxNativeDialogs_Shutdown() {
  if (qApp) {
    qApp->quit();
    delete qApp;
  }
}

struct UFileDialog
{
public:
  QFileDialog dialog;
  UFileDialog()
  {
    result.count = 0;
    result.selection = NULL;
  }
  virtual ~UFileDialog() {

    for(int i = 0;i < result.count;++i)
    {
      if(result.selection[i])
        free((void*)result.selection[i]);
    }
    if(result.selection)
      free(result.selection);
  }
  UFileDialogResult result;
};

UFileDialog* UFileDialog_Create(UFileDialogHints *hints)
{
  assert(qApp);

  if(hints == NULL)
  {
    return NULL;
  }

  UFileDialog* dialog = new UFileDialog();
  
  if(hints->InitialDirectory)
  {
    dialog->dialog.setDirectory(hints->InitialDirectory);
  }

  if(hints->WindowTitle)
  {
    dialog->dialog.setWindowTitle(hints->WindowTitle);
  }

  if(hints->NameFilter)
  {
    dialog->dialog.setNameFilter(hints->NameFilter);
  }

  if(hints->DefaultFile)
  {
    dialog->dialog.selectFile(hints->DefaultFile);
  }

  switch(hints->Action)
  {
    case UFileDialogActionOpen:
      dialog->dialog.setAcceptMode(QFileDialog::AcceptOpen);
      dialog->dialog.setFileMode(QFileDialog::ExistingFile);
      break;
    case UFileDialogActionOpenMultiple:
      dialog->dialog.setAcceptMode(QFileDialog::AcceptOpen);
      dialog->dialog.setFileMode(QFileDialog::ExistingFiles);
      break;
    case UFileDialogActionOpenDirectory:
      dialog->dialog.setAcceptMode(QFileDialog::AcceptOpen);
      dialog->dialog.setFileMode(QFileDialog::Directory);
      break;
    case UFileDialogActionSave:
      dialog->dialog.setAcceptMode(QFileDialog::AcceptSave);
      dialog->dialog.setFileMode(QFileDialog::AnyFile);
      break;
  }

//  dialog->dialog.close();
  //dialog->dialog.setWindowState( (dialog->dialog.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  dialog->dialog.setWindowFlags(Qt::WindowStaysOnTopHint);

  dialog->dialog.setWindowModality(Qt::ApplicationModal);
  dialog->dialog.show();
  qApp->processEvents();
  dialog->dialog.activateWindow();
  qApp->processEvents();
  dialog->dialog.raise();
  qApp->processEvents();

  return dialog;
}

bool UFileDialog_ProcessEvents(UFileDialog* handle)
{
  assert(qApp);
  qApp->processEvents();

  int result = handle->dialog.result();
  if(result != 0)
  {
    QStringList selectedFiles = handle->dialog.selectedFiles();

    handle->result.count = selectedFiles.size();
    handle->result.selection = (const char**)malloc(sizeof(const char*) * selectedFiles.size());
    QStringList::const_iterator constIterator;
    int i = 0;
    for(constIterator = selectedFiles.begin(); constIterator != selectedFiles.end();++constIterator)
    {
      handle->result.selection[i] = strdup((*constIterator).toStdString().c_str());
      i++;
    }
    return false;
  }
  if(handle->dialog.isHidden()) // someone closed it
  {
    return false;
  }
  return true;
}

void UFileDialog_Destroy(UFileDialog* handle)
{
  if(handle)
    delete handle;
}

const UFileDialogResult* UFileDialog_Result(UFileDialog* handle)
{

  return &(handle->result);
}


struct UFontDialog
{
public:
  QFontDialog dialog;
  UFontDialog(const QFont& initial) : dialog(initial)
  {
    result.flags = UFontDialogNormal;
    result.pointSize = 0;
    result.fontName = "";
  }
  virtual ~UFontDialog() {
    if(result.fontName[0]) {
      free((void*)result.fontName);
    }
  }
  UFontDialogResult result;
};

UFontDialog* UFontDialog_Create(UFontDialogHints *hints)
{
  if(hints == NULL)
  {
    return NULL;
  }

  QFont initial;
  if(hints->InitialFontName) {
    initial.setFamily(hints->InitialFontName);
  }
  if(hints->InitialPointSize) {
    initial.setPointSizeF(hints->InitialPointSize);
  }
  if(hints->InitialPixelSize) {
    initial.setPointSizeF(hints->InitialPixelSize);
  }
  
  UFontDialog* dialog = new UFontDialog(initial);
  dialog->dialog.show();
  if(hints->WindowTitle) {
    dialog->dialog.setWindowTitle(hints->WindowTitle);
  }

  return dialog;
}

bool UFontDialog_ProcessEvents(UFontDialog* handle)
{
  assert(qApp);
  qApp->processEvents();
  int result = handle->dialog.result();
  handle->dialog.setWindowState( (handle->dialog.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  if(result != 0)
  {
    QFont font = handle->dialog.selectedFont();
    handle->result.pointSize = font.pointSizeF();
    handle->result.pixelSize = font.pixelSize();
    handle->result.fontName = strdup(font.family().toStdString().c_str());

    if(font.bold() && font.italic())
      handle->result.flags = UFontDialogBoldItalic;
    else if(font.bold())
      handle->result.flags = UFontDialogBold;
    else if(font.italic())
      handle->result.flags = UFontDialogItalic;
    else
      handle->result.flags = UFontDialogNormal;

    return false;
  }
  if(handle->dialog.isHidden()) // someone closed it
  {
    return false;
  }
  return true;
}

void UFontDialog_Destroy(UFontDialog* handle)
{
  if(handle) {
    delete handle;
  }
}

const UFontDialogResult* UFontDialog_Result(UFontDialog* handle)
{
  return &(handle->result);
}
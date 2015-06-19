#include "UNativeDialogs.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>

char *strdup(const char *s);

struct UFileDialog
{
  GtkWidget* dialog;
  int gtk_response;
  UFileDialogResult result;
};


static void file_selected(GtkDialog *dialog, gint response_id, gpointer user_data)
{
  ((UFileDialog*)user_data)->gtk_response = response_id;
}

bool ULinuxNativeDialogs_Initialize() {
    return true;
}

void ULinuxNativeDialogs_Shutdown() {
    // no-op
}


struct UFileDialog* UFileDialog_Create(struct UFileDialogHints *hints)
{
  if(!hints)
  {
    return NULL;
  }

  if(!gtk_init_check(0, NULL))
  {
    return NULL;
  }

  UFileDialog* uedialog = calloc(1, sizeof(UFileDialog));

  int action;
  const char* label = "_Open";

  const char* title = "Select File";

  switch(hints->Action)
  {
    case UFileDialogActionOpenMultiple:
      title = "Select Files";
    case UFileDialogActionOpen:
      action = GTK_FILE_CHOOSER_ACTION_OPEN;
      break;
    case UFileDialogActionOpenDirectory:
      title = "Select Directory";
      action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      break;
    case UFileDialogActionSave:
      title ="Save As";
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
      label = "_Save";
      break;
    default:;
  }

  if(hints->WindowTitle)
  {
    title = hints->WindowTitle;
  }

  uedialog->dialog = gtk_file_chooser_dialog_new (title,
              NULL,
              action,
              "_Cancel", GTK_RESPONSE_CANCEL,
              label, GTK_RESPONSE_ACCEPT,
              NULL);

  if(hints->NameFilter)
  {
    GtkFileFilter* filter = gtk_file_filter_new();
    char buffer[4096] = {0,};
    strncpy(buffer, hints->NameFilter, sizeof(buffer)-1);

    char *filter_name = buffer;
    char *pattern = "";

    char* paren = strchr(buffer, '(');
    if(paren && paren != buffer)
    {
      // do dumb parsing of pattern
      *(paren - 1) = 0;
      *paren = 0;
      paren++;
      char* right_paren = strrchr(paren, ')');
      *right_paren = 0;

      char* pattern = paren;
      while(1)
      {
        char* pattern_end = strchr(pattern, ' ');
        if(pattern_end)
        {
          *pattern_end = 0;
        }

        if(!strcmp(pattern, "*.*"))
        {
          // replace *.* with * as otherwise selecting folders can be broken
          gtk_file_filter_add_pattern(filter, "*");
        }
        else
        {
          gtk_file_filter_add_pattern(filter, pattern);
        }

        if(!pattern_end)
        {
          break;
        }
        else
        {
          pattern = pattern_end + 1;
        }
      }
    }

    gtk_file_filter_set_name(filter, hints->NameFilter);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(uedialog->dialog),
        filter);

  }

  if(hints->InitialDirectory)
  {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(uedialog->dialog), hints->InitialDirectory);
  }

  if(hints->DefaultFile)
  {
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(uedialog->dialog), hints->DefaultFile);
  }

  switch(hints->Action)
  {
    case UFileDialogActionOpenMultiple:
      gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(uedialog->dialog), TRUE);
      break;
    case UFileDialogActionSave:
      gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(uedialog->dialog), TRUE);
      break;
    default:;
  };


  g_signal_connect (G_OBJECT (uedialog->dialog), "response",
    G_CALLBACK (file_selected),
    uedialog);

  gtk_widget_show(uedialog->dialog);

  return uedialog;
}




bool UFileDialog_ProcessEvents(UFileDialog* handle)
{

  if(!GTK_IS_FILE_CHOOSER(handle->dialog))
  {
    // widget is destroyed
    return false;
  }

  if(handle->gtk_response) {

    switch(handle->gtk_response) {
      case GTK_RESPONSE_ACCEPT:
      {
        //bo gtk_file_chooser_get_filenames
        GSList *elem;
        GSList* list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (handle->dialog));
        int i = 0;
        handle->result.selection = (const char**)malloc(sizeof(const char*) * g_slist_length(list));
        handle->result.count = g_slist_length(list);
        for(elem = list; elem; elem = elem->next) {
          handle->result.selection[i] = strdup(elem->data);
          i++;
        }
        g_slist_foreach(list, (GFunc)g_free, NULL);
        g_slist_free(list);
      }
      break;
      default:
      ;
    }
  
    return false;
  }

  while(gtk_events_pending()) {
    gtk_main_iteration_do(false);
  }

  return true;
}

const UFileDialogResult* UFileDialog_Result(UFileDialog* handle)
{
  if(!handle)
    return NULL;
  return &(handle->result);
}

void UFileDialog_Destroy(UFileDialog* handle)
{
  int i;
  if(GTK_IS_FILE_CHOOSER(handle->dialog))
  {
    gtk_widget_hide(handle->dialog);
    while(gtk_events_pending()) {
      gtk_main_iteration_do(false);
    }
    gtk_widget_destroy (handle->dialog);
  }
  // in laggy environment this is necessary - WHY?
  for(i = 0;i < 10;++i)
    gtk_main_iteration_do(false);

  if(handle->result.selection) {
    for(i = 0;i < handle->result.count;++i) {
      free((char*)handle->result.selection[i]);
    }
    free(handle->result.selection);
  }
  free(handle);
}

struct UFontDialog
{
  GtkWidget* dialog;
  int gtk_response;
  UFontDialogResult result;
};

static void font_selected(GtkDialog *dialog, gint response_id, gpointer user_data)
{
  ((UFontDialog*)user_data)->gtk_response = response_id;
}

UFontDialog* UFontDialog_Create(struct UFontDialogHints *hints)
{
  if(!hints)
  {
    return NULL;
  }

  if(!gtk_init_check(0, NULL))
  {
    return NULL;
  }

  UFontDialog* uedialog = calloc(1, sizeof(UFontDialog));

  const char* title = "Select Font";
  if(hints->WindowTitle)
  {
    title = hints->WindowTitle;
  }

  uedialog->dialog = gtk_font_selection_dialog_new(title);

  const char* initialName = "";
  if(hints->InitialFontName) {
    initialName = hints->InitialFontName;
  }
  int size = 12;
  if(hints->InitialPointSize) {
    size = hints->InitialPointSize;
  }
  else if(hints->InitialPixelSize) {
    size = hints->InitialPixelSize;
  }

  char initialBuffer[1024];
  snprintf(initialBuffer, sizeof(initialBuffer), "%s %i", initialName, size);

  gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(uedialog->dialog), 
      initialBuffer);

  g_signal_connect (G_OBJECT (uedialog->dialog), "response",
    G_CALLBACK (font_selected),
    uedialog);

  gtk_widget_show(uedialog->dialog);

  uedialog->result.fontName = "";
  uedialog->result.pointSize = 0;
  uedialog->result.pixelSize = 0;

  return uedialog;
}

bool UFontDialog_ProcessEvents(UFontDialog* handle)
{
  if(!GTK_IS_FONT_SELECTION_DIALOG(handle->dialog))
  {
    // widget is destroyed
     return false;
  }

  if(handle->gtk_response) {
    switch(handle->gtk_response) {
      case GTK_RESPONSE_OK:
      {

        PangoFontDescription *font_desc;
        gchar *fontname = gtk_font_selection_dialog_get_font_name(
                            GTK_FONT_SELECTION_DIALOG(handle->dialog));

        handle->result.fontName = strdup(fontname);

        font_desc = pango_font_description_from_string(fontname);

        g_free(fontname);

        handle->result.pointSize = (float)pango_font_description_get_size(font_desc)/PANGO_SCALE;
        handle->result.pixelSize = pango_font_description_get_size(font_desc)/PANGO_SCALE;

        handle->result.fontName = strdup(pango_font_description_get_family(font_desc));

        PangoStyle style = pango_font_description_get_style(font_desc);
        if(style & PANGO_STYLE_OBLIQUE || style & PANGO_STYLE_ITALIC)
          handle->result.flags |= UFontDialogItalic;

        PangoWeight weight = pango_font_description_get_weight(font_desc);
        if(weight >= PANGO_WEIGHT_BOLD)
          handle->result.flags |= UFontDialogBold;

        pango_font_description_free(font_desc);
      }
      break;
      default:
      ;
    }
    return false;
  }

  while(gtk_events_pending()) {
    gtk_main_iteration_do(false);
  }

  return true;
}

void UFontDialog_Destroy(UFontDialog* handle)
{
  int i;
  if(GTK_IS_FONT_SELECTION_DIALOG(handle->dialog))
  {
    gtk_widget_hide(handle->dialog);
    while(gtk_events_pending()) {
      gtk_main_iteration_do(false);
    }
    gtk_widget_destroy (handle->dialog);
  }
  // in laggy environment this is necessary - WHY?
  for(i = 0;i < 10;++i)
    gtk_main_iteration_do(false);

  if(handle->result.fontName[0]) {
    free((void*)handle->result.fontName);
  }
  free(handle);
}

const UFontDialogResult* UFontDialog_Result(UFontDialog* handle)
{
  if(!handle)
    return NULL;
  return &(handle->result); 
}

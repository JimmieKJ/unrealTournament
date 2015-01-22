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
        gtk_file_filter_add_pattern(filter, pattern);
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
        for(elem = list; elem; elem = elem->next)
        {
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


  return gtk_main_iteration_do(false);
}

const UFileDialogResult* UFileDialog_Result(UFileDialog* handle)
{
  if(!handle)
  {
    return NULL;
  }
  return &(handle->result);
}

void UFileDialog_Destroy(UFileDialog* handle)
{
  int i;
  if(GTK_IS_FILE_CHOOSER(handle->dialog))
  {
    gtk_widget_destroy (handle->dialog);
  }

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
    UFontDialogResult result;
};

UFontDialog* UFontDialog_Create(struct UFontDialogHints *hints)
{
    return NULL;
}

bool UFontDialog_ProcessEvents(UFontDialog* handle)
{
    return false;
}

void UFontDialog_Destroy(UFontDialog* handle)
{

}

const UFontDialogResult* UFontDialog_Result(UFontDialog* handle)
{
    if(!handle)
    {
        return NULL;
    }

    return &(handle->result);
}
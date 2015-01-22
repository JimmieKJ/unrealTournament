// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;

using System.ComponentModel.Composition;

using Microsoft.VisualStudio.Editor;
using Microsoft.VisualStudio.Language.Intellisense;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.TextManager.Interop;
using Microsoft.VisualStudio.Utilities;

namespace MarkdownMode.AutoCompletion
{
    [Export(typeof(IVsTextViewCreationListener))]
    [ContentType("markdown")]
    [TextViewRole(PredefinedTextViewRoles.Editable)]
    public class AutoCompletionHandlerProvider : IVsTextViewCreationListener
    {
        [Import]
        public IVsEditorAdaptersFactoryService AdapterService { get; private set; }

        [Import]
        public ICompletionBroker CompletionBroker { get; private set; }

        [Import]
        public SVsServiceProvider ServiceProvider { get; private set; }

        public void VsTextViewCreated(IVsTextView textView)
        {
            var wpfTextView = AdapterService.GetWpfTextView(textView);

            if (wpfTextView == null)
            {
                return;
            }

            Func<AutoCompletionHandler> autoCompletionHandler = () => new AutoCompletionHandler(textView, wpfTextView, this);
            wpfTextView.Properties.GetOrCreateSingletonProperty(autoCompletionHandler);
        }
    }
}

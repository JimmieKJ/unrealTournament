// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Utilities;
using Microsoft.VisualStudio.Text;

namespace MarkdownMode.MouseClick
{
    [Export(typeof(IMouseProcessorProvider))]
    [Name("CtrlMouseClick")]
    [Order(Before = "DragDrop")]
    [ContentType("markdown")]
    [TextViewRole(PredefinedTextViewRoles.Interactive)]
    internal sealed class MouseClickProcessorProvider : IMouseProcessorProvider
    {
        public IMouseProcessor GetAssociatedProcessor(IWpfTextView wpfTextView)
        {
            ITextDocument document;

            if (!wpfTextView.TextDataModel.DocumentBuffer.Properties.TryGetProperty(typeof(ITextDocument), out document))
            {
                document = null;
            }

            return new MouseClickProcessor(wpfTextView, document);
        }
    }
}
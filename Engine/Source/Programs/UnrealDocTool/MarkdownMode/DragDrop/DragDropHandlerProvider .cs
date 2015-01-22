// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Utilities;
using Microsoft.VisualStudio.Text.Editor.DragDrop;

namespace MarkdownMode.DragDrop
{
    [Export(typeof(IDropHandlerProvider))]
    [DropFormat(DragDropHandlerProvider.FileDropDataFormat)]
    [DropFormat(DragDropHandlerProvider.VSProjectStorageItemDataFormat)]
    [DropFormat(DragDropHandlerProvider.VSProjectReferenceItemDataFormat)]
    [Name("Image Insertion Drop Handler")]
    [Order(Before = "DefaultFileDropHandler")]
    internal class DragDropHandlerProvider : IDropHandlerProvider
    {
        //Support drag from Visual Studio project items - Could not find these documented by microsoft
        internal const string VSProjectStorageItemDataFormat = "CF_VSSTGPROJECTITEMS";
        internal const string VSProjectReferenceItemDataFormat = "CF_VSREFPROJECTITEMS";

        //Support drag from explorer
        internal const string FileDropDataFormat = "FileDrop";

        public IDropHandler GetAssociatedDropHandler(IWpfTextView TextView)
        {
            ITextDocument Document;

            if (!TextView.TextDataModel.DocumentBuffer.Properties.TryGetProperty(typeof(ITextDocument), out Document))
            {
                Document = null;
            }

            return TextView.Properties.GetOrCreateSingletonProperty<DragDropHandler>(() => new DragDropHandler(TextView, Document));
        }
    }
}

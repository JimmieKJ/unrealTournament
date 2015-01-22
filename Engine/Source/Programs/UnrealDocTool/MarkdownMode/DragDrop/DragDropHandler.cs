// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Collections.Specialized;
using System.IO;
using System.Windows;
using MarkdownMode.Properties;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text.Editor.DragDrop;
using EnvDTE;
using EnvDTE80;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Text.Formatting;
using MarkdownSharp;
using System.Text.RegularExpressions;

namespace MarkdownMode.DragDrop
{
    using System;
    using System.Globalization;
    using System.Security.AccessControl;

    using MarkdownMode.ToolWindow;

    using MarkdownSharp.EpicMarkdown;
    using MarkdownSharp.FileSystemUtils;
    using MarkdownSharp.Preprocessor;

    /// <summary>
    /// Handles a drag and drop of an image onto the editor.
    /// The image came from the file system (FileDrop) or from the VS Solution Explorer.
    /// </summary>
    internal class DragDropHandler : IDropHandler
    {
        //The editor window associated with this drop handler
        private IWpfTextView TextView;

        private ITextDocument Document;

        //The extensions for testing if the file is an image.
        private static readonly List<string> SupportedImageExtensions = new List<string> { ".jpg", ".tga", ".bmp", ".png", ".gif" };

        internal DragDropHandler(IWpfTextView View, ITextDocument Document)
        {
            this.TextView = View;
            this.Document = Document;
        }

        /// <summary>
        /// Drag started
        /// </summary>
        /// <param name="DragDropInfo"></param>
        /// <returns></returns>
        public DragDropPointerEffects HandleDragStarted(DragDropInfo DragDropInfo)
        {
            return DragDropPointerEffects.Link;
        }

        /// <summary>
        /// Drag in progress
        /// </summary>
        /// <param name="DragDropInfo"></param>
        /// <returns></returns>
        public DragDropPointerEffects HandleDraggingOver(DragDropInfo DragDropInfo)
        {
            return DragDropPointerEffects.Link;
        }


        /// <summary>
        /// Called when a file is dropped onto the TextView
        /// </summary>
        /// <param name="dragDropInfo"></param>
        /// <returns></returns>
        public DragDropPointerEffects HandleDataDropped(DragDropInfo dragDropInfo)
        {
            // This function is basically doing three things:
            // 1. Detects user drop position and on ambiguity asks user if replace current link.
            // 2. Detects if user provided file's valid path (in Documentation/Source hierarchy)
            //    exists. If it does, then asks user if the file should be overwritten and copy
            //    the file as needed.
            // 3. Replaces or appends a valid link.

            try
            {
                var spanToReplace = GetSpanToReplace(dragDropInfo); // 1.
                var referenceText = CreateReference(dragDropInfo); // 2.

                using (var editTextView = TextView.TextBuffer.CreateEdit())
                {
                    editTextView.Replace(spanToReplace, referenceText);

                    editTextView.Apply();
                }
            }
            catch (OperationCanceledException)
            {
                // ignore as it is just stopping the whole operation
            }

            return DragDropPointerEffects.Link;
        }

        private static string CreateReference(DragDropInfo dragDropInfo)
        {
            var droppedFilename = GetFilename(dragDropInfo);

            if (string.IsNullOrEmpty(droppedFilename))
            {
                throw new OperationCanceledException("Null or empty filename.");
            }

            return EnforceProperFileLocation(droppedFilename, GetReferenceType(droppedFilename));
        }

        private static string EnforceProperFileLocation(string path, ReferenceType type)
        {
            if (type == ReferenceType.Link)
            {
                return GetLink(path, type);
            }

            var dir = GetDestinationDir(type);
            var dst = Path.Combine(dir, Path.GetFileName(path));

            if (IsInMarkdownHierarchy(path))
            {
                return GetLink(path, type);
            }

            if (!File.Exists(dst) || AskForCopy(dst))
            {
                Directory.CreateDirectory(dir);

                File.Copy(path, dst, true);
            }

            return GetLink(dst, type);
        }

        private static bool IsInMarkdownHierarchy(string path)
        {
            return
                new FileInfo(path).FullName.ToLower().StartsWith(
                    UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument.TransformationData
                                             .CurrentFolderDetails.AbsoluteMarkdownPath.ToLower());
        }

        private static bool AskForCopy(string path)
        {
            var userResultMessageBox =
                MessageBox.Show(
                    MarkdownSharp.Language.Message("WouldYouLikeToOverwriteExistingFile", path),
                    MarkdownSharp.Language.Message("FileExists"),
                    MessageBoxButton.YesNoCancel,
                    MessageBoxImage.Question);

            switch (userResultMessageBox)
            {
                case MessageBoxResult.Yes:
                    return true;

                case MessageBoxResult.No:
                    return false;

                case MessageBoxResult.Cancel:
                    throw new OperationCanceledException("User canceled.");

                default:
                    throw new InvalidOperationException("Should not happen!");
            }
        }

        private static string GetDestinationDir(ReferenceType type)
        {
            var currentDir =
                Path.GetDirectoryName(UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument.LocalPath);

            switch (type)
            {
                case ReferenceType.Image:
                    return Path.Combine(currentDir, "Images");
                case ReferenceType.Attachment:
                    return Path.Combine(currentDir, "Attachments");
                default:
                    throw new InvalidOperationException("Should not happen!");
            }
        }

        private static string GetLink(string path, ReferenceType type)
        {
            var sourceDir = UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument
                .TransformationData.CurrentFolderDetails.AbsoluteMarkdownPath;

            var markdownPath = FileHelper.GetRelativePath(
                sourceDir + Path.DirectorySeparatorChar,
                FileHelper.GetDirectoryIfFile(path) + Path.DirectorySeparatorChar);

            markdownPath = markdownPath.Substring(0, markdownPath.Length - 1);

            switch (type)
            {
                case ReferenceType.Link:
                    return string.Format("[]({0})", Normalizer.NormalizePath(markdownPath));
                case ReferenceType.Image:
                    return string.Format(
                        "![]({0})",
                        GetAttachmentPath(Path.Combine(Path.GetDirectoryName(markdownPath), Path.GetFileName(path))));
                case ReferenceType.Attachment:
                    return string.Format(
                        "[]({0})",
                        GetAttachmentPath(Path.Combine(Path.GetDirectoryName(markdownPath), Path.GetFileName(path))));
                default:
                    throw new InvalidOperationException("Should not happen!");
            }
        }

        private static string GetAttachmentPath(string path)
        {
            var documentPath = UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument
                .TransformationData.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf;

            return Normalizer.TrimNewLines((documentPath.ToLower() == Path.GetDirectoryName(path).ToLower()) ? Path.GetFileName(path) : path);
        }

        enum ReferenceType
        {
            Link,
            Image,
            Attachment
        }

        private static ReferenceType GetReferenceType(string path)
        {
            var ext = Path.GetExtension(path).ToLower();
            var markdownPath = UDNDocRunningTableMonitor.CurrentUDNDocView.CurrentEMDocument
                .TransformationData.CurrentFolderDetails.AbsoluteMarkdownPath.ToLower();

            if (File.GetAttributes(path).HasFlag(FileAttributes.Directory))
            {
                if (path.ToLower().StartsWith(markdownPath))
                {
                    return ReferenceType.Link;
                }

                throw new OperationCanceledException("Cannot link folder outside Markdown hierarchy.");
            }

            return (ext == ".udn")
                       ? ReferenceType.Link
                       : (  SupportedImageExtensions.Contains(ext)
                                ? ReferenceType.Image
                                : ReferenceType.Attachment
                         );
        }

        public static readonly Regex AnchorInline = new Regex(string.Format(@"
                (                           # wrap whole match in $1
                    \[
                        ({0})               # link text = $2
                    \]
                    \(                      # literal paren
                        [ ]*
                        ({1})               # href = $3
                        [ ]*
                        (                   # $4
                        (['""])           # quote char = $5
                        (.*?)               # title = $6
                        \5                  # matching quote
                        [ ]*                # ignore any spaces between closing quote and )
                        )?                  # title is optional
                    \)
                )", Markdown.GetNestedBracketsPattern(), Markdown.GetNestedParensPattern()),
                  RegexOptions.Singleline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private Span GetSpanToReplace(DragDropInfo dragDropInfo)
        {
            var viewLineText =
                TextView.TextViewLines.GetTextViewLineContainingBufferPosition(dragDropInfo.VirtualBufferPosition.Position);

            var spanToReplace = new Span();
            if (viewLineText != null)
            {
                var lineText = viewLineText.ExtentIncludingLineBreak.GetText();

                //Check to see if the text on this line contains a image.
                var imagesFoundOnLine = EMImage.ImagesInline.Matches(lineText);

                int startOfMatch;
                int endOfMatch;

                foreach (Match imageFound in imagesFoundOnLine)
                {
                    //check each match on the line is one in the Position of the buffer?
                    startOfMatch = viewLineText.Start.Position + imageFound.Index;
                    endOfMatch = startOfMatch + imageFound.Length;

                    if (startOfMatch <= dragDropInfo.VirtualBufferPosition.Position.Position
                        && dragDropInfo.VirtualBufferPosition.Position.Position <= endOfMatch)
                    {
                        spanToReplace = new Span(startOfMatch, imageFound.Length);
                        break;
                    }
                }

                //Check for attachment if not found an image
                if (spanToReplace.IsEmpty)
                {
                    var attachmentFoundOnLine = AnchorInline.Matches(lineText);

                    foreach (Match attachmentFound in attachmentFoundOnLine)
                    {
                        //check each match on the line is one in the Position of the buffer?
                        startOfMatch = viewLineText.Start.Position + attachmentFound.Index;
                        endOfMatch = startOfMatch + attachmentFound.Length;

                        if (startOfMatch <= dragDropInfo.VirtualBufferPosition.Position.Position
                            && dragDropInfo.VirtualBufferPosition.Position.Position <= endOfMatch)
                        {
                            spanToReplace = new Span(startOfMatch, attachmentFound.Length);
                            break;
                        }
                    }
                }
            }

            if (spanToReplace.IsEmpty)
            {
                return new Span(dragDropInfo.VirtualBufferPosition.Position, 0);
            }

            //Image/Attachment detected at location ask user to replace or add
            var userResultMessageBox =
                MessageBox.Show(
                    MarkdownSharp.Language.Message("WouldYouLikeToReplaceExistingLink"),
                    MarkdownSharp.Language.Message("ExistingLinkDetected"),
                    MessageBoxButton.YesNoCancel,
                    MessageBoxImage.Question);

            switch (userResultMessageBox)
            {
                case MessageBoxResult.Yes:
                    return spanToReplace;

                case MessageBoxResult.No:
                    return new Span(spanToReplace.End, 0);

                case MessageBoxResult.Cancel:
                    throw new OperationCanceledException("User canceled.");

                default:
                    throw new InvalidOperationException("Should not happen!");
            }
        }

        /// <summary>
        /// Can this handler deal with the dropped object?
        /// </summary>
        /// <param name="DragDropInfo"></param>
        /// <returns></returns>
        public bool IsDropEnabled(DragDropInfo DragDropInfo)
        {
            bool Result = false;

            string DroppedFilename = GetFilename(DragDropInfo);

            if (!string.IsNullOrEmpty(DroppedFilename))
            {
                // We support all file extension types, if not an image or udn file treat as an attachment
                Result = true;
            }

            return Result;
        }

        /// <summary>
        /// Get the dropped filename
        /// </summary>
        /// <param name="Info"></param>
        /// <returns></returns>
        private static string GetFilename(DragDropInfo Info)
        {
            DataObject Data = new DataObject(Info.Data);

            if (Info.Data.GetDataPresent(DragDropHandlerProvider.FileDropDataFormat))
            {
                // The drag and drop operation came from the file system
                StringCollection Files = Data.GetFileDropList();

                if (Files != null && Files.Count == 1)
                {
                    return Files[0];
                }
            }
            else if (Info.Data.GetDataPresent(DragDropHandlerProvider.VSProjectStorageItemDataFormat))
            {            
                // The drag and drop operation came from the VS solution explorer as a storage item
                return Data.GetText();
            }
            else if (Info.Data.GetDataPresent(DragDropHandlerProvider.VSProjectReferenceItemDataFormat))
            {
                // The drag and drop operation came from the VS solution explorer as a reference item have to
                // get the selected item from the solution explorer
                DTE2 dte = Package.GetGlobalService(typeof(DTE)) as DTE2;

                UIHierarchy solutionExplorer = dte.ToolWindows.SolutionExplorer;

                var SolutionExplorerSeclectedItems = solutionExplorer.SelectedItems as System.Array;

                if (SolutionExplorerSeclectedItems != null && SolutionExplorerSeclectedItems.Length == 1)
                {
                    foreach (UIHierarchyItem SolutionExplorerSelectedItem in SolutionExplorerSeclectedItems)
                    {
                        ProjectItem SolutionExplorerSelectedObject = SolutionExplorerSelectedItem.Object as ProjectItem;

                        return SolutionExplorerSelectedObject.Properties.Item("FullPath").Value.ToString();
                    }

                }
            }

            return null;
        }

        public void HandleDragCanceled()
        {
        }
    }
}

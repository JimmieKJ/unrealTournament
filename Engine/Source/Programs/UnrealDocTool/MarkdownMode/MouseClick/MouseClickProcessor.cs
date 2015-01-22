// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Windows;
using System.Windows.Input;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Editor;
using Microsoft.VisualStudio.Text.Formatting;
using CommonUnrealMarkdown;
using System.Text.RegularExpressions;
using System.IO;
using System;
using MarkdownMode.Properties;
using MarkdownSharp;
using EnvDTE80;
using EnvDTE;
using Microsoft.VisualStudio.Shell;

namespace MarkdownMode.MouseClick
{
    internal sealed class MouseClickProcessor : MouseProcessorBase
    {
        private IWpfTextView TextView;
        private ITextDocument Document;
        private string AbsoluteMarkdownPath;
        private string CurrentFolderFromMarkdownAsTopLeaf;
        private string Language;


        private static readonly OutputPaneLogger log = new OutputPaneLogger();

        public MouseClickProcessor(IWpfTextView TextView, ITextDocument Document)
        {
            this.TextView = TextView;
            this.Document = Document;

            SetupFolderSettings();
        }

        public override void PreprocessMouseLeftButtonDown(MouseButtonEventArgs e)
        {
            if (e.ClickCount != 3)
            {
                //Only consider triple clicks
                return;
            }

            //Get the line of text under the Y co-ordinate of the mouse, consider the Viewport
            ITextViewLine ViewLineText = TextView.TextViewLines.GetTextViewLineContainingYCoordinate(e.GetPosition(TextView.VisualElement).Y + TextView.ViewportTop);
            if (ViewLineText == null)
            {
                return;
            }

            string LineText = ViewLineText.ExtentIncludingLineBreak.GetText();
            
            //Check if this is an include span
            if (Markdown.IncludeFileSpan.IsMatch(LineText))
            {
                if (ProcessIncludeLinkNavigate(Markdown.IncludeFileSpan.Match(LineText)))
                {
                    e.Handled = true;
                }
                else
                {
                    e.Handled = false;
                }
            }
            else
            {
                e.Handled = false;
            }
        }

        private void SetupFolderSettings()
        {
            string FileName = Document.FilePath;

            DirectoryInfo SourceDirectoryInfo = new DirectoryInfo(FileName).Parent;

            //Source directory path is found using the SourceDirectoryName setting and the file
            while (SourceDirectoryInfo != null && SourceDirectoryInfo.Name != Settings.Default.SourceDirectoryName)
            {
                SourceDirectoryInfo = SourceDirectoryInfo.Parent;
            }

            if (SourceDirectoryInfo != null)
            {
                string SourceDirectory = SourceDirectoryInfo.FullName;

                string OutputDirectory = Path.Combine(Path.GetTempPath(), "UDTMarkdownMode") + "/";


                string FileOutputDirectory = (new DirectoryInfo(FileName).Parent).FullName.Replace(SourceDirectory + Path.DirectorySeparatorChar, "").Replace(SourceDirectory, "");

                string OutfileName = Path.Combine(Path.Combine(Path.Combine(OutputDirectory, Regex.Replace(Path.GetFileNameWithoutExtension(FileName).ToUpper(), @"[^\.]*?\.(.*$)", "$1")), FileOutputDirectory), "index.html");

                AbsoluteMarkdownPath = SourceDirectory;
                CurrentFolderFromMarkdownAsTopLeaf = FileOutputDirectory;
                Language = System.Text.RegularExpressions.Regex.Replace(Path.GetFileNameWithoutExtension(FileName).ToUpper(), @"[^\.]*?\.(.*$)", "$1");
            }
        }

        private bool ProcessIncludeLinkNavigate (Match match)
        {
            string IncludeFileFolderName = "";
            string IncludeRegion = "";

            if (match.Groups["IncludeFileRegion"].Value.Contains("#"))
            {
                IncludeFileFolderName = match.Groups["IncludeFileRegion"].Value.Split('#')[0];
                IncludeRegion = match.Groups["IncludeFileRegion"].Value.Split('#')[1];
            }
            else
            {
                IncludeFileFolderName = match.Groups["IncludeFileRegion"].Value;
            }

            bool IsURLProblem = false;
            bool bChangedLanguage = false;
            string FileNameLocation = "";

            if (String.IsNullOrWhiteSpace(IncludeFileFolderName))
            {
                if (String.IsNullOrWhiteSpace(IncludeRegion))
                {
                    log.Error(MarkdownSharp.Language.Message("UnableToNavigateToPage") + MarkdownSharp.Language.Message("ExcerptRegionToIncludeWhenNoFileGiven"));
                    IsURLProblem = true;
                }
                else
                {
                    //Assume that this is a reference to a location in this file
                    IncludeFileFolderName = CurrentFolderFromMarkdownAsTopLeaf;
                }
            }

            if (IncludeFileFolderName.ToUpper().Contains("%ROOT%"))
            {
                IncludeFileFolderName = ".";
            }

            if (!IsURLProblem)
            {
                //We know location of file we want but not the file name AbsoluteMarkdownPath\IncludeFileName\*.Language.udn
                if (!Directory.Exists(Path.Combine(AbsoluteMarkdownPath, IncludeFileFolderName)))
                {
                    //unable to locate the path to the file raise error
                    log.Error(MarkdownSharp.Language.Message("BadPathForIncludeFile", match.Groups[0].Value));

                    IsURLProblem = true;
                }
                else
                {
                    FileInfo[] LanguageFileInfo = new DirectoryInfo(Path.Combine(AbsoluteMarkdownPath, IncludeFileFolderName)).GetFiles("*." + Language + ".udn");

                    if (LanguageFileInfo.Length > 0)
                    {
                        FileNameLocation = LanguageFileInfo[0].FullName;
                    }
                    else
                    {
                        // File not found
                        // if this is not an INT file check for the INT version.
                        if (!Language.Equals("INT"))
                        {
                            LanguageFileInfo = new DirectoryInfo(Path.Combine(AbsoluteMarkdownPath, IncludeFileFolderName)).GetFiles("*.INT.udn");
                            if (LanguageFileInfo.Length == 0)
                            {
                                //unable to locate an INT file to replace the language raise error
                                log.Error(MarkdownSharp.Language.Message("UnableToNavigateToPage") + MarkdownSharp.Language.Message("BadIncludeOrMissingMarkdownFileAndNoINTFile", match.Groups[0].Value));
                                IsURLProblem = true;
                            }
                            else
                            {
                                FileNameLocation = (LanguageFileInfo[0].FullName);
                                //Raise info so that know we are allowing missing linked files to still allow processing of the file if INT file is there
                                log.Info(MarkdownSharp.Language.Message("NavigatingToINTPage") + MarkdownSharp.Language.Message("BadIncludeOrMissingMarkdownFileINTUsed", match.Groups[0].Value));
                                IsURLProblem = true;
                                bChangedLanguage = true;
                            }
                        }
                        else
                        {
                            log.Error(MarkdownSharp.Language.Message("UnableToNavigateToPage") + MarkdownSharp.Language.Message("BadIncludeOrMissingMarkdownFile", match.Groups[0].Value));
                            IsURLProblem = true;
                        }
                    }
                }
            }
            
            // If no problem detected
            if (!IsURLProblem || bChangedLanguage)
            {
                if (String.IsNullOrWhiteSpace(IncludeRegion))
                {
                    //No region to consider, navigate straight to the page.
                    DTE2 dte = Package.GetGlobalService(typeof(DTE)) as DTE2;
                    
                    //File found open it, this also makes the editor switch tabs to this file, so the preview window updates.
                    dte.ItemOperations.OpenFile(FileNameLocation);

                    return true;
                }
                else
                {
                    //Are we able to navigate to a region in the file?
                    
                    string IncludeFile = File.ReadAllText(FileNameLocation);
                    Match Excerpts = Regex.Match(IncludeFile, Markdown.GetSubRegionOfFileMatcher(IncludeRegion));

                    if (Excerpts.Success)
                    {
                        //Found excerpt section, get the line number
                        int LineNumber = 0;
                        for (int i = 0; i <= Excerpts.Groups[0].Index - 1; ++i)
                        {
                            if (IncludeFile[i] == '\n')
                            {
                                ++LineNumber;
                            }
                        }

                        DTE2 dte = Package.GetGlobalService(typeof(DTE)) as DTE2;

                        //File found open it, this also makes the editor switch tabs to this file, so the preview window updates.
                        dte.ItemOperations.OpenFile(FileNameLocation);

                        dte.ExecuteCommand("Edit.Goto", LineNumber.ToString());
                    }
                    else
                    {
                        //Region not found
                        log.Error(MarkdownSharp.Language.Message("UnableToNavigateToPage") + MarkdownSharp.Language.Message("NotAbleToFindRegionInFile", match.Groups[0].Value));
                        IsURLProblem = true;

                        DTE2 dte = Package.GetGlobalService(typeof(DTE)) as DTE2;

                        //File found open it, this also makes the editor switch tabs to this file, so the preview window updates.
                        dte.ItemOperations.OpenFile(FileNameLocation);
                    }

                    return true;
                }
            }

            return false;
        }
    }
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Linq;
using System.Drawing;
using System.Drawing.Imaging;
using System.Reflection;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using CommonUnrealMarkdown;
using MarkdownSharp;
using System.Collections.Generic;
using MarkdownSharp.Doxygen;
using UnrealDocTool.Properties;
using UnrealDocTool.Params;
using System.Runtime.InteropServices;
using UnrealDocTool.GUI;
using MarkdownSharp.FileSystemUtils;

//Expectations
//There is a structure in P4 with one directory having child directories Markdown and Doc
//The directory is saved to the app.config file after first run.
//This is used for testing the filestructures for file existisance (on internal links) etc.

namespace UnrealDocTool
{
    using System.Windows.Data;

    class UnrealDocTool
    {
        // Define a static logger variable so that it references the
        // Logger instance named "UnrealDocTool".
        private static Logger log;

        enum HelpType { All, ConvFile, ConvDir, ConvSubDir, ConvAll, ChangeSourceDir, ChangeOutputDir};

        //require INT to be top level directory in SupportedLanguages, leave this first
        //enum SupportedLanguages { INT, CH, KR, JP };

        private static bool DoCompressImages;
        private static int JpegCompressionRate;
        private static string CompressImageType;
        private static ImageFormat CompressImageFormat;
        private static Color DefaultImageFillColor;
        //private static string P4DocumentDirectoryLocation;
        private static string SourceDirectory;
        private static string OutputDirectory;
        private static string DefaultTemplate;

        private static string[] SupportedLanguages;
        private static string[] SupportedLanguageLabels;
        private static Dictionary<string, string> SupportedLanguageMap = new Dictionary<string,string>();
        private static string[] MetadataErrorIfMissing;
        private static string[] MetadataInfoIfMissing;
        private static string[] MetadataCleanKeepThese;
        private static string[] SupportedAvailability;
        private static List<string> AllSupportedAvailability = new List<string>();
        private static string PublicAvailabilityFlag;
        private static bool ThisIsPreview = false;
        private static bool ThisIsLogOnly = false;
        private static bool AlreadyCreatedCommonDirectories = false;
        private static List<string> AlreadyCopiedSearchCommonFiles = new List<string>();
        private static string[] SubsetOfSupportedLanguages;
        private static string[] PublishFlags;

        /// <summary>
        /// Gives usage information
        /// </summary>
        static void Help()
        {
            log.Info(Language.Message("Usage", string.Join(", ", Language.AvailableLangIDs), PublicAvailabilityFlag, string.Join(", ", SupportedAvailability), string.Join(", ", SupportedLanguages)));
        }

        
        /// <summary>
        /// Do some common file processing to all processes.
        /// test that a source directory exists
        /// </summary>
        /// <returns></returns>
        static bool InitialDirectoryChecksOk()
        {
            if (!Directory.Exists(SourceDirectory))
            {
                log.Error(Language.Message("DocumentSourceDirNotFound", SourceDirectory));
                return false;
            }
            else
            {
                return true;
            }
        }

        static void OpenPageInBrowser(string FileName)
        {
            try
            {
                System.Diagnostics.Process.Start(FileName);
            }
            catch (System.ComponentModel.Win32Exception noBrowser)
            {
                if (noBrowser.ErrorCode == -2147467259)
                    log.Error(noBrowser.Message);
            }
            catch (System.Exception other)
            {
                log.Error(other.Message);
            }
        }

        static bool IsInSubDirectory(string path, string subdirPath)
        {
            return new FileInfo(path).FullName.StartsWith(new FileInfo(subdirPath).FullName);
        }

        /// <summary>
        /// Case insensitive directory passed in converted to Values stored on disc
        /// </summary>
        /// <param name="NonCaseSensitiveDirInfo"></param>
        /// <returns></returns>
        static string CaseSensitivePath(DirectoryInfo NonCaseSensitiveDirInfo)
        {
            if (NonCaseSensitiveDirInfo.Root.Name == NonCaseSensitiveDirInfo.Name)
            {
                return NonCaseSensitiveDirInfo.Name;
            }
            else
            {
                DirectoryInfo ParentDirInfo = NonCaseSensitiveDirInfo.Parent;
                return (Path.Combine(CaseSensitivePath(ParentDirInfo), ParentDirInfo.GetDirectories(NonCaseSensitiveDirInfo.Name)[0].Name));
            }
        }

        /// <summary>
        /// Case insensitive Path to File converted to Values stored on disc
        /// </summary>
        /// <param name="NonCaseSensitiveFileInfo"></param>
        /// <returns></returns>
        static string CaseSensitiveFilePath(FileInfo NonCaseSensitiveFileInfo)
        {
            DirectoryInfo ParentDirInfo = NonCaseSensitiveFileInfo.Directory;
            return (Path.Combine(CaseSensitivePath(ParentDirInfo), ParentDirInfo.GetFiles(NonCaseSensitiveFileInfo.Name)[0].Name));
        }

        static void DeleteDirRecursive(string DirectoryName)
        {
            try
            {
                foreach (string SubDirectoryName in Directory.GetDirectories(DirectoryName))
                {
                    DeleteDirRecursive(SubDirectoryName);
                }
                foreach (string FileName in Directory.GetFiles(DirectoryName))
                {
                    File.SetAttributes(FileName, FileAttributes.Normal);
                }
                Directory.Delete(DirectoryName, true);
            }
            catch (Exception e)
            {
                log.Error(e.Message, e.InnerException);
            }
        }

        [DllImport("kernel32.dll")]
        public static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr window, int nCmdShow);

        static void ShowConsole()
        {
            ShowWindow(GetConsoleWindow(), 1);
        }

        static void HideConsole()
        {
            ShowWindow(GetConsoleWindow(), 0);
        }

        [STAThread]
        static void Main(string[] args)
        {
            Directory.SetCurrentDirectory(
                Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location));

            //Get directory for P4 Document Directory location.
            //make sure does not start with \ or / or end with them
            //strip ./ from start if there
            SourceDirectory = Regex.Replace(
                Settings.Default.SourceDirectory, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");
            OutputDirectory = Regex.Replace(
                Settings.Default.OutputDirectory, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");

            Language.LangID = "INT";

            Language.Init(
                Path.Combine(
                    SourceDirectory,
                    Settings.Default.IncludeDirectory,
                    Settings.Default.InternationalizationDirectory));

            DoCompressImages = Settings.Default.DoCompressImages;
            if (DoCompressImages)
            {
                //If we are not compressing an image no need to set other conversion variables
                JpegCompressionRate = Settings.Default.ImageJPGCompressionValue;
                try
                {
                    DefaultImageFillColor = ImageConversion.GetColorFromHexString(Settings.Default.ImageFillColor);
                }
                catch (Exception)
                {
                    DefaultImageFillColor = default(Color);
                }
                CompressImageType = Settings.Default.CompressImageType;
                CompressImageFormat = ImageConversion.GetImageFormat(CompressImageType);
                if (CompressImageFormat == null)
                {
                    log.Error(Language.Message("UnsupportedCompressImgType", CompressImageType));
                    return;
                }
            }

            SupportedLanguages = Settings.Default.SupportedLanguages.ToUpper().Split(',');
            SupportedLanguageLabels = Settings.Default.SupportedLanguageLabels.Split(',');
            for (int i = 0; i < SupportedLanguages.Length; i++)
            {
                SupportedLanguageMap.Add(SupportedLanguages[i], SupportedLanguageLabels[i]);
            }

            MetadataErrorIfMissing = Settings.Default.MetadataErrorIfMissing.Split(',');
            MetadataInfoIfMissing = Settings.Default.MetadataInfoIfMissing.Split(',');
            MetadataCleanKeepThese = Settings.Default.MetadataCleanKeepThese.ToLower().Split(',');
            SupportedAvailability = Settings.Default.SupportedAvailability.ToLower().Split(',');
            PublicAvailabilityFlag = Settings.Default.PublicAvailabilityFlag.ToLower();

            //AllSupportedAvailablities needs to include the public and supported availabilities
            foreach (string Availability in SupportedAvailability)
            {
                AllSupportedAvailability.Add(Availability);
            }
            AllSupportedAvailability.Add(PublicAvailabilityFlag);

            var config = new Configuration(SupportedAvailability, PublicAvailabilityFlag, SupportedLanguages, false);

            if (args.Length == 0)
            {
                config = new Configuration(SupportedAvailability, PublicAvailabilityFlag, SupportedLanguages, true);
                // There are no arguments, so try to obtain them from GUI.
                var gui = new GUI.GUI();

                var wpfLogger = new WPFLogger();

                gui.Init(config, wpfLogger);

                log = wpfLogger;

                HideConsole();

                gui.Run();
            }
            else
            {
                log = new LogFileLogger();

                try
                {
                    config.ParamsManager.Parse(args);

                    RunUDNConversion(config, log);
                }
                catch (ParsingErrorException e)
                {
                    if (e.Sender != null)
                    {
                        log.Error(Language.Message("ErrorParsingParam", e.Sender.Name));
                    }

                    log.Error(e.Message);
                    log.Info(config.ParamsManager.GetHelp());
                }
                catch (Exception e)
                {
                    // catching all and printing out -- finishing anyway
                    // print out exception and exit gently rather than panic
                    PrintUnhandledException(e);
                }
                finally
                {
                    log.WriteToLog(Language.Message("UnrealDocToolFinished"));
                }
            }

            AppDomain.CurrentDomain.ProcessExit += (sender, e) => OnProcessExit(sender, e, config);
        }

        private static void PrintUnhandledException(Exception e)
        {
            log.Error("Unhandled exception: " + e.Message + "\nCall stack:\n" + e.StackTrace);

            if (e.InnerException == null)
            {
                return;
            }

            log.Error("\nInner exception:\n");
            PrintUnhandledException(e.InnerException);
        }

        public static void OnProcessExit(object sender, EventArgs e, Configuration config)
        {
            User.Default.SupportedAvailability = String.Join(",",config.PublishFlagsParam.ChosenStringOptions);
            User.Default.SourceDirectory = config.SourceParam.Value;
            User.Default.OutputDirectory = config.OutputParam.Value;
            User.Default.SupportedLanguages = String.Join(",", config.LangParam.ChosenStringOptions);
            User.Default.LogVerbosityParam = config.LogVerbosityParam.ChosenOption.ToString();
            User.Default.HelpFlag = config.HelpFlag.Value;
            User.Default.LogOnlyFlag = config.LogOnlyFlag.Value;
            User.Default.PreviewFlag = config.PreviewFlag.Value;
            User.Default.CleanFlag = config.CleanFlag.Value;
            User.Default.PathsSpec = config.PathsSpec.Value;
            User.Default.DefaultTemplate = config.TemplateParam.Value;
            User.Default.OutputFormat = config.OutputFormatParam.ChosenOption.ToString();
            User.Default.PathPrefix = config.PathPrefixParam.Value;
            User.Default.Save();
        }

        public static void RunUDNConversion(Configuration config, Logger log)
        {
            log.WriteToLog(Language.Message("UnrealDocToolStarted"));

            if (config.HelpFlag)
            {
                log.Info(config.ParamsManager.GetHelp());
                return;
            }

            ThisIsPreview = config.PreviewFlag;

            if (ThisIsPreview)
            {
                //output to a preview file
                OutputDirectory = Path.Combine(Path.GetTempPath(), "UDT");
            }

            ThisIsLogOnly = config.LogOnlyFlag;

            PublishFlags =
                config.PublishFlagsParam.ChosenStringOptions.Union(new string[] { PublicAvailabilityFlag }).ToArray();
            SubsetOfSupportedLanguages = config.LangParam.ChosenStringOptions;
            DefaultTemplate = config.TemplateParam.Value;

            DoxygenHelper.DoxygenXmlPath = config.DoxygenCacheParam;

            switch (config.LogVerbosityParam.ChosenOption)
            {
                case LogVerbosity.Info:
                    log.SetInfoVerbosityLogLevel();
                    break;
                case LogVerbosity.Warn:
                    log.SetWarnVerbosityLogLevel();
                    break;
                case LogVerbosity.Error:
                    log.SetErrorVerbosityLogLevel();
                    break;
            }

            // The output directory in app.config is over-ridden with this value
            // make sure does not start with \ or / or end with them
            // strip ./ from start if there.
            if (!ThisIsPreview)
            {
                // Preview output temp directory take precedence.
                OutputDirectory = Regex.Replace(config.OutputParam.Value, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");
            }

            // The source directory in app.config is over-ridden with this value.
            SourceDirectory = Regex.Replace(config.SourceParam.Value, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");

            // If this path is set, then we should rebuild doxygen cache.
            if (!string.IsNullOrWhiteSpace(config.RebuildDoxygenCacheParam))
            {
                var engineDir = new DirectoryInfo(Path.Combine(SourceDirectory, "..", "..")).FullName;
                log.Info(Language.Message("RebuildingDoxygenCache"));
                DoxygenHelper.DoxygenExec = new FileInfo(config.RebuildDoxygenCacheParam.Value);
                DoxygenHelper.DoxygenInputFilter = Path.Combine(
                    engineDir,
                    "Binaries",
                    "DotNET",
                    "UnrealDocToolDoxygenInputFilter.exe");
                DoxygenHelper.RebuildCache(
                    Path.Combine(engineDir, "Source"), (sender, eventArgs) => log.Info(eventArgs.Data));
                return;
            }

            // If running clean really only need the SourceDirectory value.
            if (config.CleanFlag)
            {
                if (SourceDirectory == "")
                {
                    throw new ParsingErrorException(null, Language.Message("NoOutputDir"));
                }
            }
            else
            {
                if (OutputDirectory == "" || SourceDirectory == "")
                {
                    throw new ParsingErrorException(null, Language.Message("NoOutputOrSourceDir"));
                }

                //If the OutputDirectory is relative then find absolute path
                if (Regex.IsMatch(OutputDirectory, @"\.[/|\\]"))
                {
                    string tempOutputDirectory =
                        (new Uri(Path.GetDirectoryName(Assembly.GetAssembly(typeof(UnrealDocTool)).CodeBase))).LocalPath;
                    while (OutputDirectory.StartsWith(".."))
                    {
                        tempOutputDirectory = Directory.GetParent(tempOutputDirectory).FullName;
                        OutputDirectory = Regex.Replace(OutputDirectory, @"^\.\.[/|\\](.*)", "$1");
                    }

                    OutputDirectory = (new DirectoryInfo(Path.Combine(tempOutputDirectory, OutputDirectory))).FullName;
                }

                //If the SourceDirectory is relative then find absolute path
                if (Regex.IsMatch(SourceDirectory, @"\.[/|\\]"))
                {
                    string tempSourceDirectory =
                        (new Uri(Path.GetDirectoryName(Assembly.GetAssembly(typeof(UnrealDocTool)).CodeBase))).LocalPath;
                    while (SourceDirectory.StartsWith(".."))
                    {
                        tempSourceDirectory = Directory.GetParent(tempSourceDirectory).FullName;
                        SourceDirectory = Regex.Replace(SourceDirectory, @"^\.\.[/|\\](.*)", "$1");
                    }

                    SourceDirectory =
                        CaseSensitivePath(new DirectoryInfo(Path.Combine(tempSourceDirectory, SourceDirectory)));
                }
            }

            // Delete previously created temp folder.
            if (Directory.Exists(Path.Combine(Path.GetTempPath(), "UDT")))
            {
                DeleteDirRecursive(Path.Combine(Path.GetTempPath(), "UDT"));
            }

            if (config.CleanFlag)
            {
                log.Info(Language.Message("CleaningDuplicateLanguageImageFilesAndRecursing", SourceDirectory));
                CleanImagesRecursiveDirectory(SourceDirectory);
                log.Info(Language.Message("CleaningMetadataFromFileInDirAndRecursing", SourceDirectory));
                CleanMetaDataRecursiveDirectory(SourceDirectory);
            }
            else if (ThisIsLogOnly || InitialDirectoryChecksOk())
            {
                RunMarkdownConversion(config.PathsSpec, config, new MarkdownSharp.Markdown(), log);
            }
        }

        private static void RunMarkdownConversion(string pathsSpec, Configuration config, Markdown markdownToHtml, Logger log)
        {
            // Preview mode must take precedence over logging mode.
            if (ThisIsPreview)
            {
                log.Info(Language.Message("PreviewModeEnabled"));
            }
            else if (ThisIsLogOnly)
            {
                log.Info(Language.Message("LoggingModeEnabled"));
            }
            
            var folders = GetDirectoriesPathsSpec(pathsSpec, config);

            var timer = new Stopwatch();
            timer.Start();

            DirectoryStatistics statistics = new DirectoryStatistics();

            foreach (var folder in folders)
            {
                if (folder.Directory.GetFiles().Length == 0)
                {
                    continue;
                }
                statistics.IncrementTotal();
                foreach (var lang in folder.Languages)
                {
                    if (folder.Directory.GetFiles(string.Format("*.{0}.udn", lang)).Length == 0)
                    {
                        continue;
                    }
                    var fileName = folder.Directory.GetFiles(string.Format("*.{0}.udn", lang)).First().FullName;
                    var langLinks = config.LinksToAllLangs ? null : folder.Languages.ToArray();

                    LogConvertingFileInfo(log, fileName, "StartConversion");

                    var result = ConvertFile(markdownToHtml, fileName, lang, langLinks, config.OutputFormatParam);

                    switch (result)
                    {
                        case ConvertFileResponse.NoChange:
                        {
                            LogConvertingFileInfo(log, fileName, "ConvertNoChange");
                            statistics.IncrementNoChangesCount();
                            break;
                        }
                        case ConvertFileResponse.Converted:
                        {
                            var outputFileName = GetTargetFileName(fileName, lang);

                            if (!ThisIsPreview && ThisIsLogOnly)
                            {
                                LogConvertingFileInfo(log, fileName, "ConvertSucceededLogOnly", outputFileName);
                                break;
                            }

                            if (folders.Count == 1 && folders[0].Languages.Count == 1)
                            {
                                LogConvertingFileInfo(log, fileName, "ConvertSucceeded", outputFileName);
                                OpenPageInBrowser(outputFileName);
                            }
                            else
                            {
                                LogConvertingFileInfo(log, fileName, "Converted", outputFileName);
                            }

                            statistics.IncrementConvertedCount();
                            break;
                        }
                        case ConvertFileResponse.Failed:
                        {
                            LogConvertingFileInfo(log, fileName, "ConvertFailed");
                            statistics.IncrementFailedCount();
                            break;
                        }
                        default:
                            LogConvertingFileInfo(log, fileName, "UnknownErrorProcessingFile");
                            break;
                    }
                }
            }

            timer.Stop();

            log.WriteToLog(Language.Message("SummaryStart"));
            log.WriteToLog(Language.Message("ConvertedIn", (Convert.ToDouble(timer.ElapsedMilliseconds) / 1000).ToString()));
            statistics.LogCounts();
            log.WriteToLog(Language.Message("SummaryEnd"));
        }

        private static void LogConvertingFileInfo(Logger log, string fileName, string messageId, params string[] args)
        {
            log.Info(string.Format("{0,-55}: {1}", fileName, Language.Message(messageId, args)));
        }

        private static List<ConversionDirectory> GetDirectoriesPathsSpec(string pathsSpec, Configuration config)
        {
            var folders = new List<ConversionDirectory>();
            var prefix = config.PathPrefixParam.Value;

            if (FileHelper.IsExistingDirectory(prefix))
            {
                prefix += Path.DirectorySeparatorChar;
            }
            
            foreach (var path in pathsSpec.Split(';'))
            {
                var fullPath = prefix + path;

                if (fullPath.EndsWith("*"))
                {
                    DoRecursively(fullPath.Substring(0, fullPath.Length - 1), p => AddFolder(config, p, folders));
                }
                else
                {
                    AddFolder(config, fullPath, folders);
                }
            }

            return folders;
        }

        private static void AddFolder(Configuration config, string fullPath, List<ConversionDirectory> folders)
        {
            if (!IsInSubDirectory(fullPath, SourceDirectory))
            {
                throw new ParsingErrorException(
                    null, Language.Message("FileMustBeInASubdirOfSrcDir", fullPath, SourceDirectory));
            }

            var convDir = ConversionDirectory.CreateFromPath(fullPath, config);

            if (convDir.Languages.Count == 0)
            {
                // Nothing to do, ignore.
                return;
            }

            folders.Add(convDir);
        }

        /// <summary>
        /// Possible return values on converting a file
        /// </summary>
        enum ConvertFileResponse { NoChange, Converted, Failed };

        /// <summary>
        /// Relative css folder is at HTML\css.  Workout from where the TargetFile is located
        /// </summary>
        /// <param name="TargetFileName"></param>
        /// <returns></returns>
        static string GetRelativeHTMLPath(string TargetFileName)
        {
            //Step up until we find directory html.
            DirectoryInfo CurrentDirectory = new DirectoryInfo(TargetFileName).Parent;

            string RelativePath = @"./";

            while (CurrentDirectory.FullName.ToLower() != OutputDirectory.ToLower())
            {
                CurrentDirectory = CurrentDirectory.Parent;
                RelativePath += @"../";
            }

            return RelativePath;
        }

        static ConvertFileResponse ConvertFile(MarkdownSharp.Markdown markdownToHtml, string inputFileName, string language, IEnumerable<string> languagesLinksToGenerate, OutputFormat format = OutputFormat.HTML)
        {
            var result = ConvertFileResponse.NoChange;
            var targetFileName = GetTargetFileName(inputFileName, language);
            
            //Set up parameters in Markdown to aid in processing and generating html
            Markdown.MetadataErrorIfMissing = MetadataErrorIfMissing;
            Markdown.MetadataInfoIfMissing = MetadataInfoIfMissing;
            markdownToHtml.DoNotPublishAvailabilityFlag = Settings.Default.DoNotPublishAvailabilityFlag;
            markdownToHtml.PublishFlags = PublishFlags.ToList();
            markdownToHtml.AllSupportedAvailability = AllSupportedAvailability;

            var fileOutputDirectory = FileHelper.GetRelativePath(
                SourceDirectory, Path.GetDirectoryName(inputFileName));

            var currentFolderDetails = new FolderDetails(
                GetRelativeHTMLPath(targetFileName), OutputDirectory, SourceDirectory, fileOutputDirectory,
                Settings.Default.APIFolderLocation,
                (new DirectoryInfo(inputFileName).Parent).FullName.Replace(
                    SourceDirectory + Path.DirectorySeparatorChar.ToString(), "")
                                                         .Replace(SourceDirectory, "")
                                                         .Replace(Path.DirectorySeparatorChar.ToString(), " - "),
                language);

            markdownToHtml.DefaultTemplate = DefaultTemplate;

            markdownToHtml.ThisIsPreview = ThisIsPreview;

            if (language != "INT")
            {
                currentFolderDetails.DocumentTitle += " - " + language;
            }

            if (ThisIsPreview)
            {
                currentFolderDetails.DocumentTitle += " - PREVIEW!";
            }

            markdownToHtml.SupportedLanguages = SupportedLanguages;
            markdownToHtml.SupportedLanguageLabels = SupportedLanguageLabels;
            for (int i = 0; i < markdownToHtml.SupportedLanguages.Length; i++)
            {
                if(!markdownToHtml.SupportedLanguageMap.ContainsKey(SupportedLanguages[i]))
                {
                    markdownToHtml.SupportedLanguageMap.Add(markdownToHtml.SupportedLanguages[i], markdownToHtml.SupportedLanguageLabels[i]);
                }
            }

            //Pass the default conversion settings to Markdown for use in the image details creation.
            markdownToHtml.DefaultImageDoCompress = DoCompressImages;
            markdownToHtml.DefaultImageFormatExtension = CompressImageType;
            markdownToHtml.DefaultImageFillColor = DefaultImageFillColor;
            markdownToHtml.DefaultImageFormat = CompressImageFormat;
            markdownToHtml.DefaultImageQuality = JpegCompressionRate;

            var errorList = new List<ErrorDetail>();
            var imageDetails = new List<ImageConversion>();
            var attachNames = new List<AttachmentConversionDetail>();

            var output = markdownToHtml.Transform(FileContents(inputFileName), errorList, imageDetails, attachNames, currentFolderDetails, languagesLinksToGenerate, format != OutputFormat.HTML);

            var noFailedErrorReport = true;
            var stopProcessing = false;

            //If output empty then treat as failed, we are not converting most likely due to the publish flags and availability settings
            if (String.IsNullOrWhiteSpace(output))
            {
                noFailedErrorReport = false;
                stopProcessing = true;
                result = ConvertFileResponse.NoChange;
                log.Info(MarkdownSharp.Language.Message("NotConverted", inputFileName));
            }
            else
            {
                //Need to check for error types prior to processing to output log messages in the correct order.
                foreach (var errorInfo in errorList)
                {
                    if (errorInfo.ClassOfMessage == MessageClass.Error || errorInfo.ClassOfMessage == MessageClass.Warning)
                    {
                        log.Info(MarkdownSharp.Language.Message("FileFailed", inputFileName));
                        noFailedErrorReport = false;
                        break;
                    }
                }
            }


            if (noFailedErrorReport)
            {
                log.Info(MarkdownSharp.Language.Message("Converted", inputFileName));
            }


            //On warnings or errors stop processing the file to the publish folder but allow to continue if in preview.
            if (errorList.Count > 0)
            {
                Console.Write("\n");


                foreach (MarkdownSharp.ErrorDetail ErrorInfo in errorList)
                {
                    switch (ErrorInfo.ClassOfMessage)
                    {
                        case MarkdownSharp.MessageClass.Error:
                            log.Error(ErrorInfo.Path ?? inputFileName, ErrorInfo.Line, ErrorInfo.Column, ErrorInfo.Message);
                            if (!ThisIsPreview)
                            {
                                stopProcessing = true;
                                result = ConvertFileResponse.Failed;
                            }
                            break;
                        case MarkdownSharp.MessageClass.Warning:
                            log.Warn(ErrorInfo.Path ?? inputFileName, ErrorInfo.Line, ErrorInfo.Column, ErrorInfo.Message);
                            if (!ThisIsPreview)
                            {
                                stopProcessing = true;
                                result = ConvertFileResponse.Failed;
                            }
                            break;
                        default:
                            log.Info(ErrorInfo.Path ?? inputFileName, ErrorInfo.Line, ErrorInfo.Column, ErrorInfo.Message);
                            break;
                    }
                }
            }

            if (!stopProcessing)
            {
                if (ThisIsPreview || !ThisIsLogOnly)
                {
                    CommonUnrealFunctions.CopyDocumentsImagesAndAttachments(inputFileName, log, OutputDirectory, language, fileOutputDirectory, imageDetails, attachNames);
                }

                var expected = FileContents(targetFileName);

                if (output == expected)
                {
                    result = ConvertFileResponse.NoChange;
                }
                else
                {
                    if (!stopProcessing)
                    {
                        if (!AlreadyCreatedCommonDirectories)
                        {
                            AlreadyCreatedCommonDirectories = CommonUnrealFunctions.CreateCommonDirectories(OutputDirectory, SourceDirectory, log);
                        }

                        Console.Write("\n");
                        if (ThisIsPreview || !ThisIsLogOnly)
                        {
                            //Check output directory exists, if not create the full html structure for this language
                            CommonUnrealFunctions.GenerateDocsFolderStructure(OutputDirectory, fileOutputDirectory, language);

                            CommonUnrealFunctions.SetFileAttributeForReplace(new FileInfo(targetFileName));
                            File.WriteAllText(targetFileName, output);

                            if (format == OutputFormat.PDF)
                            {
                                PdfHelper.CreatePdfFromHtml(targetFileName);
                            }
                        }

                        result = ConvertFileResponse.Converted;
                    }
                }
            }
            return result;
        }

        private static string GetTargetFileName(string inputFileName, string language)
        {
            return Path.Combine(
                OutputDirectory,
                language.ToUpper(),
                FileHelper.GetRelativePath(SourceDirectory, Path.GetDirectoryName(inputFileName)),
                "index.html");
        }

        /// <summary>
        /// Directory information on conversion
        /// </summary>
        class DirectoryStatistics
        {
            int PrivateTotal;
            int PrivateNoChangesCount;
            int PrivateFailedCount;
            int PrivateConvertedCount;

            public DirectoryStatistics(int Total = 0, int NoChangesCount = 0, int FailedCount = 0, int ConvertedCount = 0)
            {
                PrivateTotal = Total;
                PrivateNoChangesCount = NoChangesCount;
                PrivateFailedCount = FailedCount;
                PrivateConvertedCount = ConvertedCount;
            }

            public void IncrementTotal()
            {
                ++PrivateTotal;
            }
            
            public void IncrementNoChangesCount()
            {
                ++PrivateNoChangesCount;
            }

            public void IncrementFailedCount()
            {
                ++PrivateFailedCount;
            }

            public void IncrementConvertedCount()
            {
                ++PrivateConvertedCount;
            }

            public void Append(DirectoryStatistics other)
            {
                PrivateTotal += other.PrivateTotal;
                PrivateNoChangesCount += other.PrivateNoChangesCount;
                PrivateFailedCount += other.PrivateFailedCount;
                PrivateConvertedCount += other.PrivateConvertedCount;
            }
        
            public void LogCounts ()
            {
                log.WriteToLog(Language.Message("FilesExamined", PrivateTotal.ToString()));

                if (PrivateNoChangesCount > 0)
                {
                    log.WriteToLog(Language.Message("FilesNotChanged", PrivateNoChangesCount.ToString()));
                }
                if (PrivateConvertedCount > 0)
                {
                    log.WriteToLog(Language.Message("FilesConverted", PrivateConvertedCount.ToString()));
                }

                if (PrivateFailedCount > 0)
                {
                    log.WriteToLog(Language.Message("FilesFailed", PrivateFailedCount.ToString()));
                }
            }
        };

        private static IEnumerable<string> StringListIntersection(IEnumerable<string> listA, IEnumerable<string> listB)
        {
            var output = new List<string>();

            foreach (var a in listA)
            {
                if (listB.Contains(a))
                {
                    output.Add(a);
                }
            }

            return output;
        }

        private static IEnumerable<string> StringListIntersection(IEnumerable<string> firstList, params IEnumerable<string>[] lists)
        {
            foreach (var list in lists)
            {
                firstList = StringListIntersection(firstList, list);
            }

            return firstList;
        }

        public class ConversionDirectory
        {
            public List<string> Languages { get; private set; }
            public DirectoryInfo Directory { get; private set; }

            public ConversionDirectory(string path, IEnumerable<string> requestedLangs)
            {
                Directory = new DirectoryInfo(path);

                var availableLangs = Directory.GetFiles("*.udn").Select(f => GetLangIdFromFileName(f.Name)).ToList();

                Languages = StringListIntersection(requestedLangs, SupportedLanguages).ToList();
            }

            private static string GetLangIdFromFileName(string name)
            {
                return name.Substring(name.Length - 7, 3).ToUpper();
            }

            public static ConversionDirectory CreateFromPath(string filePath, Configuration config)
            {
                if (new FileInfo(filePath).Attributes.HasFlag(FileAttributes.Directory))
                {
                    return new ConversionDirectory(filePath, config.LangParam.ChosenStringOptions);
                }

                return new ConversionDirectory(
                    Path.GetDirectoryName(filePath), new[] { GetLangIdFromFileName(Path.GetFileName(filePath)) });
            }
        }
        
        static void CleanImagesRecursiveDirectory(string SourcePath)
        {
            //Recurse sub directories
            foreach (string SubDirectory in Directory.GetDirectories(SourcePath))
            {
                String SubDirectoryUpper = SubDirectory.ToUpper();
                //Exclude archive,css, defaultwebtemplate and image folders from processing
                if (!(SubDirectoryUpper.EndsWith("\\INCLUDE") || SubDirectoryUpper.EndsWith("\\JAVASCRIPT") || SubDirectoryUpper.EndsWith("\\IMAGES") || SubDirectoryUpper.EndsWith("\\ATTACHMENTS") || SubDirectoryUpper.EndsWith("\\CSS") || SubDirectoryUpper.EndsWith("\\TEMPLATES")))
                {
                    CleanImagesRecursiveDirectory(SubDirectory);
                }
                else if (SubDirectoryUpper.EndsWith("\\IMAGES"))
                {
                    //Image folder check for child directories
                    foreach (string LanguageImageFolder in Directory.GetDirectories(SubDirectory))
                    {
                        //For each file check bytes against parent directories files.
                        foreach (string LangImageFileName in Directory.GetFiles(LanguageImageFolder))
                        {
                            string INTFileName = Path.Combine(SubDirectory,Path.GetFileName(LangImageFileName));
                            if (File.Exists(INTFileName) && CommonUnrealFunctions.ByteEquals(File.ReadAllBytes(LangImageFileName), File.ReadAllBytes(INTFileName)))
                            {
                                log.Info(Language.Message("DeletingDuplicatedImage", LangImageFileName));
                                CommonUnrealFunctions.SetFileAttributeForReplace(new FileInfo(LangImageFileName));
                                File.Delete(LangImageFileName);
                            }
                        }
                    }
                }
            }
        }



        /// <summary>
        /// Cleans metadata from the string
        /// </summary>
        /// <param name="MarkdownToHtml"></param>
        /// <param name="match">metadata row</param>
        /// <param name="FileName">Name of file containing the metadata</param>
        private static string CleanMetaDataRow(Match match, string FileName)
        {
            string MetaValue = match.Groups["MetaDataValue"].Value;
            string MetaDataCategory = match.Groups["MetaDataKey"].Value;

            //Metadata must be in MetadataCleanKeepThese or in the rename Document Availability to Availability

            if (MetadataCleanKeepThese.Contains(MetaDataCategory.ToLower()))
            {
                return match.Groups[0].Value;
            }
            else
            {
                if (MetaDataCategory.ToLower().Equals("document availability"))
                {
                    return string.Format("Availability: {0}", MetaValue);
                }
                else
                {
                    log.Info(Language.Message("RemovingMetadata", FileName, MetaDataCategory));
                }

            }
            return "";
        }

        /// <summary>
        /// Clean up the meta data
        /// </summary>
        /// <param name="match">metadata block found in filename</param>
        /// <param name="FileName">Name of file containing the metadata</param>
        private static string CleanMetaData(Match match, string FileName)
        {
            string[] MetaDataRows = Regex.Split(match.Groups[0].Value, "\n");

            string CleanedMetaData = "";

            bool FoundAvailabilityMetaData = false;

            foreach (string CurrentMetaDataRow in MetaDataRows)
            {
                string PostProcessingMetaData = Markdown.MetaDataRow.Replace(CurrentMetaDataRow, EveryMatch => CleanMetaDataRow(EveryMatch, FileName));
                if (!string.IsNullOrWhiteSpace(PostProcessingMetaData))
                {
                    CleanedMetaData += PostProcessingMetaData + "\n";
                    if (PostProcessingMetaData.StartsWith("Availability:", StringComparison.OrdinalIgnoreCase))
                    {
                        FoundAvailabilityMetaData = true;
                    }
                }
            }

            if (!FoundAvailabilityMetaData)
            {
                CleanedMetaData = "Availability:Licensee\n" + CleanedMetaData;
                log.Info(Language.Message("AddingMetadataAvailabilityLicensee", FileName));
            }

            return CleanedMetaData;
        }        

        /// <summary>
        /// Cleans metadata in the specified directory of files
        /// </summary>
        /// <param name="SourcePath">Includes Path</param>
        static void CleanMetaDataDirectory(string SourcePath)
        {
            DirectoryInfo CurrentDirectory = new DirectoryInfo(SourcePath);

            //Process each file in each language unless language was a parameter
            //but only if there are any udn files in this directory
            if (CurrentDirectory.GetFiles("*.udn").Length > 0)
            {
                foreach (string Language in SubsetOfSupportedLanguages)
                {
                    if (CurrentDirectory.GetFiles(string.Format("*.{0}.udn", Language)).Length > 0)
                    {
                        FileInfo InputFile = CurrentDirectory.GetFiles(string.Format("*.{0}.udn", Language))[0];

                        //Save file contents prior to processing
                        String Source = FileContents(InputFile.FullName);

                        //Process Metadata
                        String PostProcessing = Markdown.MetaData.Replace(Source, EveryMatch => CleanMetaData(EveryMatch, InputFile.FullName));

                        if (!Source.Equals(PostProcessing))
                        {
                            CommonUnrealFunctions.SetFileAttributeForReplace(InputFile);
                            File.WriteAllText(InputFile.FullName, PostProcessing);
                        }
                        //compare files and save if different
                    }

                }
            }
        }

        static void CleanMetaDataRecursiveDirectory(string sourcePath)
        {
            DoRecursivelyIgnored(sourcePath, recursivePath => CleanMetaDataDirectory(recursivePath));
        }

        private static readonly string[] IgnoredFolders = new[] { "include", "javascript", "images", "attachments", "css", "templates" };
        
        private static bool IsIgnoredDirectory(string sourcePath)
        {
            var sourcePathLowerAbsolute = new DirectoryInfo(sourcePath).FullName.ToLower();
            var apiPath =
                new DirectoryInfo(
                    Path.Combine(
                        Directory.GetCurrentDirectory(),
                        Settings.Default.SourceDirectory,
                        Settings.Default.APIFolderLocation)).FullName.ToLower();

            return !sourcePathLowerAbsolute.StartsWith(apiPath)
                   && IgnoredFolders.Any(e => sourcePathLowerAbsolute.EndsWith(Path.DirectorySeparatorChar + e));
        }

        private static void DoRecursively(string sourcePath, Action<string> action)
        {
			if (!Directory.Exists(sourcePath))
			{
				return;
			}

			action(sourcePath);

            foreach (var subDirectory in Directory.GetDirectories(sourcePath))
            {
                //if (!IsIgnoredDirectory(subDirectory))
                //{
                    DoRecursively(subDirectory, action);
                //}
            }
        }

        private static void DoRecursivelyIgnored(string sourcePath, Action<string> action)
        {
            if (!Directory.Exists(sourcePath))
            {
                return;
            }

            action(sourcePath);

            foreach (var subDirectory in Directory.GetDirectories(sourcePath))
            {
                if (!IsIgnoredDirectory(subDirectory))
                {
                    DoRecursivelyIgnored(subDirectory, action);
                }
            }
        }

        /// <summary>
        /// returns the contents of the specified file as a string  
        /// </summary>
        static string FileContents(string PathFilenameString)
        {
            try
            {
                return File.ReadAllText(PathFilenameString);
            }
            catch (FileNotFoundException)
            {
                return "";
            }
            catch (DirectoryNotFoundException)
            {
                return "";
            }

        }


        /// <summary>
        /// removes any empty newlines and any leading spaces at the start of lines 
        /// all tabs, and all carriage returns
        /// </summary>
        public static string RemoveWhitespace(string Text)
        {
            // Standardize line endings             
            Text = Text.Replace("\r\n", "\n");    // DOS to Unix
            Text = Text.Replace("\r", "\n");      // Mac to Unix

            // remove any tabs entirely
            Text = Text.Replace("\t", "");

            // remove empty newlines
            Text = Regex.Replace(Text, @"^\n", "", RegexOptions.Multiline);

            // remove leading space at the start of lines
            Text = Regex.Replace(Text, @"^\s+", "", RegexOptions.Multiline);

            // remove all newlines
            Text = Text.Replace("\n", "");

            return Text;
        }
    }
}

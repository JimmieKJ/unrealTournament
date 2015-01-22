// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Reflection;
using LogTools;

namespace TwikiToMarkdown
{
    class TwikiToMarkdown
    {
        // Define a static logger variable so that it references the
        // Logger instance named "TwikiToMarkdown".
        private static readonly Logger log = new Logger();

        enum SupportedLanguages { INT, CH, KR, JP };

        static void Main(string[] args)
        {
            log.Info(String.Format("==================== TwikiToMarkdown Started ======================\n"));
            
            string SourcePath = "";
            string SourceImageAttachPath = "";
            string DestPath = "";
            if (args.Length > 0)
            {
                if (args[0].Length > 2)
                {
                    SourcePath = args[0];
                    SourceImageAttachPath = args[1];
                    DestPath = args[2];
                }
                else
                {
                    log.Error("Error - Usuage is TwikiToMarkdown <SourceDataPath> <SourceImageAttachPath> <TargetP4UDNPath>");
                    return;
                }
            }
            else
            {
                Console.WriteLine("\nEnter source path to data folder:\n");
                SourcePath = Console.ReadLine();
                Console.WriteLine("\nEnter dest path to Image and Attachment folder:\n");
                SourceImageAttachPath = Console.ReadLine();
                Console.WriteLine("\nEnter dest path to P4 UDN folder where the markdown folder will be created:\n");
                DestPath = Console.ReadLine();
            }

            DestPath = Path.Combine(DestPath, "Markdown");

            //Load the mapping dictionary of twiki names to markdown locations
            Dictionary<string, string> LinkPageMapping = loadLinkPageMapping();
            
            var Timer = new Stopwatch();

            //Convert English Files
            Timer.Start();

            CopyInitialSetupFiles(DestPath);

            ConvertFile(SourcePath, DestPath, SourceImageAttachPath, LinkPageMapping);

            Timer.Stop();

            log.Info("Completed in " + Convert.ToDouble(Timer.ElapsedMilliseconds) / 60000 + " mins");
            log.Info(String.Format("==================== TwikiToMarkdown Finished ======================\n\n\n"));
        }

        static void ConvertFile(string SourcePath, string TargetPath, string ImageAttachPath, Dictionary<string, string> LinkPageMapping)
        {
            var TwikiToMarkDownConverter = new TwikiToMarkdownFile.TwikiToMarkdownFile();
            TwikiToMarkDownConverter.TwikiMarkdownLookup = LinkPageMapping;

            string Output;
            string Expected;
            string ActualPath;
            string BackupPath;

            SupportedLanguages Language;

            int NoChangesCount = 0;
            int NewFileCount = 0;
            int WhiteSpaceChangesCount = 0;
            int SignificantChangesCount = 0;
            int Total = 0; 
            
            //Check directory exists
            log.Info(String.Format("Running Twiki Conversion on folder " + SourcePath));
            if (!Directory.Exists(SourcePath))
            {
                log.Error("Directory does not exist, skipping transform");
                return;
            }
            
            if (!Directory.Exists(TargetPath))
            {
                Directory.CreateDirectory(TargetPath);
            }

            //Backup old files directory
            string BackupDirectory = Path.Combine(TargetPath, "OldConversions");
            if (!Directory.Exists(BackupDirectory))
            {
                Directory.CreateDirectory(BackupDirectory);
            }

            foreach (string FileName in Directory.GetFiles(SourcePath, "*.txt"))
            {
                //Filename must exist in the mapping directory or it should not be converted. and can not have extension with ,v
                if (!FileName.ToLower().EndsWith(",v") && LinkPageMapping.ContainsKey(Path.GetFileNameWithoutExtension(FileName)))
                {
                    ++Total;

                    //Name of output file depends on the language, the path for the output file is found in the mapping variables dictionary
                    if (FileName.ToLower().EndsWith("ch.txt"))
                    {
                        //Chinese so CH.udn
                        if (LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)] == "%ROOT%")
                        {
                            //Home directory
                            ActualPath = Path.Combine(TargetPath, "UE4Home.CH.udn");
                            BackupPath = Path.Combine(BackupDirectory, "UE4Home.CH.udn");
                        }
                        else
                        {
                            ActualPath = Path.Combine(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".CH.udn");
                            BackupPath = Path.Combine(Path.Combine(BackupDirectory, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".CH.udn");
                        }
                        Language = SupportedLanguages.CH;
                    }
                    else if (FileName.ToLower().EndsWith("kr.txt"))
                    {
                        //Korean so KR.udn
                        if (LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)] == "%ROOT%")
                        {
                            //Home directory
                            ActualPath = Path.Combine(TargetPath, "UE4Home.KR.udn");
                            BackupPath = Path.Combine(BackupDirectory, "UE4Home.KR.udn");
                        }
                        else
                        {
                            ActualPath = Path.Combine(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".KR.udn");
                            BackupPath = Path.Combine(Path.Combine(BackupDirectory, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".KR.udn");
                        }
                        Language = SupportedLanguages.KR;
                    }
                    else if (FileName.ToLower().EndsWith("jp.txt"))
                    {
                        //Japanese so JP.udn
                        if (LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)] == "%ROOT%")
                        {
                            //Home directory
                            ActualPath = Path.Combine(TargetPath, "UE4Home.JP.udn");
                            BackupPath = Path.Combine(BackupDirectory, "UE4Home.JP.udn");
                        }
                        else
                        {
                            ActualPath = Path.Combine(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".JP.udn");
                            BackupPath = Path.Combine(Path.Combine(BackupDirectory, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".JP.udn");
                        }
                        Language = SupportedLanguages.JP;
                    }
                    else
                    {
                        //assume english so INT.udn
                        if (LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)] == "%ROOT%")
                        {
                            //Home directory
                            ActualPath = Path.Combine(TargetPath, "UE4Home.INT.udn");
                            BackupPath = Path.Combine(BackupDirectory, "UE4Home.INT.udn");
                        }
                        else
                        {
                            ActualPath = Path.Combine(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".INT.udn");
                            BackupPath = Path.Combine(Path.Combine(BackupDirectory, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]), (new DirectoryInfo(Path.Combine(TargetPath, LinkPageMapping[Path.GetFileNameWithoutExtension(FileName)]))).Name + ".INT.udn");
                        }
                        Language = SupportedLanguages.INT;
                    }

                    //Create Directory
                    if (!Directory.Exists(new DirectoryInfo(ActualPath).Parent.FullName))
                    {
                        Directory.CreateDirectory(new DirectoryInfo(ActualPath).Parent.FullName);
                        Directory.CreateDirectory(new DirectoryInfo(BackupPath).Parent.FullName);
                    }


                    Expected = FileContents(ActualPath);
                    Output = TwikiToMarkDownConverter.Transform(FileContents(FileName));

                    if (Output == Expected)
                    {
                        NoChangesCount++;
                    }
                    else if (Expected == "" & Output != "")
                    {
                        NewFileCount++;
                        if (!File.Exists(ActualPath))
                            File.WriteAllText(ActualPath, Output);
                    }
                    else if (RemoveWhitespace(Output) == RemoveWhitespace(Expected))
                    {
                        WhiteSpaceChangesCount++;
                        if (File.Exists(ActualPath))
                        {
                            //Move existing file for comparison
                            if (File.Exists(BackupPath))
                            {
                                File.Delete(BackupPath);
                            }
                            File.Move(ActualPath, BackupPath);
                            File.WriteAllText(ActualPath, Output);
                        }
                        else
                        {
                            File.WriteAllText(ActualPath, Output);
                        }

                    }
                    else
                    {
                        SignificantChangesCount++;
                        if (File.Exists(ActualPath))
                        {
                            Console.Write("\n");
                            log.Info(@"Conversion detected changes since last run for " + Path.GetFileName(FileName));
                            //Move existing file for comparison
                            if (File.Exists(BackupPath))
                            {
                                File.Delete(BackupPath);
                            }
                            File.Move(ActualPath, BackupPath);
                            File.WriteAllText(ActualPath, Output);
                        }
                        else
                        {
                            File.WriteAllText(ActualPath, Output);
                        }
                    }

                    //Copy images and attachments, if the images directory does not already exist for this file... 
                    //copy images once, but allow twiki conversion process to be updated if needs be and convert the twiki files repeatedly.
                    string SourceImageAttachPathThisFile = Path.Combine(ImageAttachPath, Path.GetFileNameWithoutExtension(FileName));
                    string TargetImagePath;
                    string TargetAttachPath;
                    switch (Language)
                    {
                        case SupportedLanguages.KR:
                            TargetImagePath = Path.Combine(Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName,"Images"),"KR");
                            TargetAttachPath = Path.Combine(Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName, "Attachments"), "KR");
                            break;
                        case SupportedLanguages.JP:
                            TargetImagePath = Path.Combine(Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName,"Images"),"JP");
                            TargetAttachPath = Path.Combine(Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName, "Attachments"), "JP");
                            break;
                        case SupportedLanguages.CH:
                            TargetImagePath = Path.Combine(Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName,"Images"),"CH");
                            TargetAttachPath = Path.Combine(Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName, "Attachments"), "CH");
                            break;
                        default:
                        //SupportedLanguages.INT:
                            TargetImagePath = Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName,"Images");
                            TargetAttachPath = Path.Combine(new DirectoryInfo(ActualPath).Parent.FullName, "Attachments");
                            break;
                    }

                    if (!Directory.Exists(TargetImagePath))
                    {
                        Directory.CreateDirectory(TargetImagePath);
                    }
                    if (!Directory.Exists(TargetAttachPath))
                    {
                        Directory.CreateDirectory(TargetAttachPath);
                    }

                    if (Directory.Exists(SourceImageAttachPathThisFile))
                    {
                        foreach (string ImageOrAttachmentFile in Directory.GetFiles(SourceImageAttachPathThisFile))
                        {
                            //if the filename extenstion has a ,v in it ignore the file
                            if (!Path.GetExtension(ImageOrAttachmentFile).ToLower().EndsWith(",v"))
                            {
                                //if image type file
                                if (Regex.IsMatch(ImageOrAttachmentFile, @"\.png|\.gif|\.tga|\.bmp|\.jpg", RegexOptions.IgnoreCase))
                                {
                                    if (File.Exists(Path.Combine(TargetImagePath, Path.GetFileName(ImageOrAttachmentFile))))
                                    {
                                        File.SetAttributes(Path.Combine(TargetImagePath, Path.GetFileName(ImageOrAttachmentFile)), FileAttributes.Normal);
                                    }
                                    File.Copy(ImageOrAttachmentFile, Path.Combine(TargetImagePath, Path.GetFileName(ImageOrAttachmentFile)), true);
                                }
                                else
                                {
                                    //attachment
                                    if (File.Exists(Path.Combine(TargetAttachPath, Path.GetFileName(ImageOrAttachmentFile))))
                                    {
                                        File.SetAttributes(Path.Combine(TargetAttachPath, Path.GetFileName(ImageOrAttachmentFile)), FileAttributes.Normal);
                                    }
                                    File.Copy(ImageOrAttachmentFile, Path.Combine(TargetAttachPath, Path.GetFileName(ImageOrAttachmentFile)), true);
                                }
                            }
                        }
                    }

                    Console.Write(".");
                }

            }
            Console.Write("\n");
            log.Info(String.Format("=============================== Summary ==========================\n"));
            log.Info(String.Format("{0:000} files converted in {1,-55}", Total, SourcePath));
            log.Info(String.Format("Files output to {0,-55}\n", TargetPath));

            if (NoChangesCount > 0)
            {
                log.Info(String.Format("{0:000}/{1:000} files did not change.", NoChangesCount, Total));
            }
            if (NewFileCount > 0)
            {
                log.Info(String.Format("{0:000} new files.", NewFileCount));
            }

            if (WhiteSpaceChangesCount > 0)
            {
                log.Info(String.Format("{0:000} had whitespace differences.", WhiteSpaceChangesCount));
            }
            if (SignificantChangesCount > 0)
            {
                log.Info(String.Format("{0:000} had significant file changes.", SignificantChangesCount));
            }

            log.Info(String.Format("=========================== Summary End ==========================\n"));
        }

        /// <summary>
        /// returns the contents of the specified file as a string  
        /// </summary>
        static string FileContents(string PathString, string Filename)
        {
            try
            {
                return File.ReadAllText(Path.Combine(PathString, Filename));
            }
            catch (FileNotFoundException)
            {
                return "";
            }

        }

        static string FileContents(string Filename)
        {
            try
            {
                return File.ReadAllText(Filename);
            }
            catch (FileNotFoundException)
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

        
        /// <summary>
        /// Create the css, images and defaultwebtemplate folders in the markdown folder
        /// </summary>
        public static void CopyInitialSetupFiles(string DestPath)
        {
            string MarkdownInitialSetupFilesLocation = Path.Combine(new Uri(Path.GetDirectoryName(Assembly.GetAssembly(typeof(TwikiToMarkdown)).CodeBase)).LocalPath, "Include");
            foreach (string DirectoryName in Directory.GetDirectories(MarkdownInitialSetupFilesLocation))
            {
                string DestSubPath = Path.Combine(Path.Combine(DestPath, "Include"), Path.GetFileName(DirectoryName));

                if (!Directory.Exists(DestSubPath))
                {
                    Directory.CreateDirectory(DestSubPath);
                }

                foreach (string FileName in Directory.GetFiles(Path.Combine(MarkdownInitialSetupFilesLocation,DirectoryName)))
                {
                    if (File.Exists(Path.Combine(DestSubPath, Path.GetFileName(FileName))))
                    {
                        File.SetAttributes(Path.Combine(DestSubPath, Path.GetFileName(FileName)), FileAttributes.Normal);
                    }
                    File.Copy(FileName, Path.Combine(DestSubPath,Path.GetFileName(FileName)),true);
                }
            }
        }
        
        /// <summary>
        /// Parse the Link Conversion Information filename web page mapping into a dictionary
        /// </summary>
        /// <param name="path"></param>
        /// <returns></returns>
        public static Dictionary<string, string> loadLinkPageMapping()
        {
            Dictionary<string, string> loadedMappings = new Dictionary<string, string>();

            try
            {
                using (StreamReader readFile = new StreamReader(Path.Combine(Path.Combine(new Uri(Path.GetDirectoryName(Assembly.GetAssembly(typeof(TwikiToMarkdown)).CodeBase)).LocalPath, "LinkConversionInformation"), "UE4Docs_Twiki_Mappings.csv")))
                {
                    string line;
                    string[] row;

                    while ((line = readFile.ReadLine()) != null)
                    {
                        row = line.Split(',');
                        loadedMappings.Add(row[0],row[1]);
                    }
                }
            }
            catch (Exception e)
            {
                log.Error(e.Message);
            }

            return loadedMappings;
        } 
    
    }
}

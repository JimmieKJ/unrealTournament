// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using CommonUnrealMarkdown;
using MarkdownSharp;

namespace CommonUnrealMarkdown
{
    public class CommonUnrealFunctions
    {
        public static void GenerateDocsFolderStructure(string OutputDirectory, string DirectoryName, string Language)
        {


            // Generate HTML folders in the following structure
            //  DOCS/INT/Level1/Level2
            //      /CH/Level1/Level2
            //          /images
            //          /attachments
            //      /KR/Level1/Level2
            //          /images
            //          /attachments
            //      /JP/Level1/Level2
            //          /images
            //          /attachments
            //      /CommonImages/Level1/Level2
            //          /images
            //          /attachments

            DirectoryInfo FileMainLanguageDirectory = new DirectoryInfo(Path.Combine(Path.Combine(OutputDirectory, Language), DirectoryName));
            //Check output directory exists, if not create the full html structure for this language
            if (!FileMainLanguageDirectory.Exists)
            {
                FileMainLanguageDirectory.Create();
            }
        }

        class OutputPathEqualityComparer : IEqualityComparer<ImageConversion>
        {
            public bool Equals(ImageConversion x, ImageConversion y)
            {
                return x.OutImageName.Equals(y.OutImageName);
            }

            public int GetHashCode(ImageConversion obj)
            {
                return obj.OutImageName.GetHashCode();
            }
        }

        public static void CopyDocumentsImagesAndAttachments(string fileName, Logger log, string outputDirectory, string language, string fileOutputDirectory, IEnumerable<ImageConversion> imageDetails, IEnumerable<AttachmentConversionDetail> attachmentDetails)
        {
            GenerateDocsFolderStructure(outputDirectory, fileOutputDirectory, language);

            //attachment file check and copy to html folder
            foreach (var attachmentDetail in attachmentDetails)
            {
                try
                {
                    var attachmentInFolderFileName = attachmentDetail.InAttachName;
                    var attachmentOutFolderFileName = attachmentDetail.OutAttachName;

                    //Check out folder exists we may be working with a relative path to another directory
                    var attachmentOutFolder = new DirectoryInfo(attachmentOutFolderFileName).Parent;
                    if (!attachmentOutFolder.Exists)
                    {
                        attachmentOutFolder.Create();
                    }

                    var attachmentOutFolderFile = new FileInfo(attachmentOutFolderFileName);

                    if (attachmentOutFolderFile.Exists && FileEquals(attachmentInFolderFileName, attachmentOutFolderFileName))
                    {
                        continue;
                    }

                    SetFileAttributeForReplace(attachmentOutFolderFile);
                    File.Copy(attachmentInFolderFileName, attachmentOutFolderFileName, true);
                }
                catch (FileNotFoundException)
                {
                    log.Error(
                        MarkdownSharp.Language.Message(
                            "CheckingAttachmentsToSeeIfThereHasBeenAnUpdateThereWasAnError",
                            attachmentDetail.InAttachName));
                }
            }

            //image file check and copy to html folder

            // Create unique list on output path as it get overwritten anyway.
            imageDetails = imageDetails.Select(e => e).Distinct(new OutputPathEqualityComparer()).ToList();

            foreach (var imageDetail in imageDetails)
            {
                var imageInFolderFileName = imageDetail.InImageName;
                var imageOutFolderFileName = imageDetail.OutImageName;

                //Check out folder exists we may be working with a relative path to another directory
                var imageOutFolder = new DirectoryInfo(imageOutFolderFileName).Parent;
                if (!imageOutFolder.Exists)
                {
                    imageOutFolder.Create();
                }

                if (!File.Exists(imageInFolderFileName))
                {
                    Console.Write("\n");
                    log.Warn(
                        Language.Message(
                            "MissingImageFileDetectedPleaseLocate", Path.GetFileName(fileName), imageInFolderFileName));
                }
                else
                {
                    //Copy image across if not same
                    try
                    {
                        var imageOut = imageDetail.WebImage();

                        //Check compressed stream with existing file if different overwrite html/image folders image.
                        var imageOutFolderFile = new FileInfo(imageOutFolderFileName);

                        if (!imageOutFolderFile.Exists || !ByteEquals(imageOut, File.ReadAllBytes(imageOutFolderFileName)))
                        {
                            SetFileAttributeForReplace(imageOutFolderFile);

                            File.WriteAllBytes(imageOutFolderFileName, imageOut);
                        }

                    }
                    catch (FileNotFoundException)
                    {
                        log.Error(
                            MarkdownSharp.Language.Message(
                                "ErrorDuringCheckingImagesToSeeIfThereHasBeenAnUpdate", imageDetail.InImageName));
                    }
                    catch (IOException)
                    {
                        log.Warn(
                            MarkdownSharp.Language.Message(
                                "FileCouldNotBeMovedAlreadyBeingMovedByAnotherProcess", imageDetail.InImageName));
                    }
                }
            }
        }

        /// <summary>
        /// Compare two bytearray, return true if both streams are equivalent, otherwise false.
        /// </summary>
        /// <param name="ByteArray1"></param>
        /// <param name="ByteArray2"></param>
        /// <returns></returns>
        public static bool ByteEquals(byte[] ByteArray1, byte[] ByteArray2)
        {
            if (ByteArray1 == ByteArray2) return true;
            if (ByteArray1 == null || ByteArray2 == null) return false;
            if (ByteArray1.Length != ByteArray2.Length) return false;
            for (int i = 0; i < ByteArray2.Length; i++)
            {
                if (ByteArray1[i] != ByteArray2[i]) return false;
            }
            return true;
        }

        /// <summary>
        /// Compare two files, return true if both are equivalent, otherwise false.
        /// </summary>
        /// <param name="FileName1"></param>
        /// <param name="FileName2"></param>
        /// <returns></returns>
        public static bool FileEquals(string FileName1, string FileName2)
        {
            int File1Byte = 0;
            int File2Byte = 0;

            using (FileStream Stream1 = new FileStream(FileName1, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (FileStream Stream2 = new FileStream(FileName2, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                if (Stream1.Length != Stream2.Length) return false;

                do 
                {
                    File1Byte = Stream1.ReadByte();
                    File2Byte = Stream2.ReadByte();

                } while (File1Byte == File2Byte && File1Byte != -1);
            }
            
            return ((File1Byte - File2Byte) == 0);
        }

        /// <summary>
        /// If a file exists set it's attributes so that it can be edited.
        /// </summary>
        /// <param name="FileToChange">A file info object which will be set</param>
        /// <param name="ByteArray2"></param>
        /// <returns></returns>
        public static void SetFileAttributeForReplace(FileInfo FileToChange)
        {
            if (FileToChange.Exists)
            {
                FileToChange.Attributes = FileAttributes.Normal;
            }
        }


        /// <summary>
        /// Copy files from OutputDirectory/Include/ForlderTypeName to SourceDirectory/Include/FolderTypeName 
        /// </summary>
        private static bool CopyDirectory(string OutputDirectory, string SourceDirectory, string FolderTypeName, Logger log)
        {
            DirectoryInfo SourceDirectoryInfo = new DirectoryInfo(Path.Combine(Path.Combine(SourceDirectory, "Include"), FolderTypeName));
            if (!SourceDirectoryInfo.Exists)
            {
                log.Error(Language.Message("DirectoryCouldNotBeFound", SourceDirectoryInfo.FullName));
                return false;
            }

            DirectoryInfo OutputDirectoryInfo = new DirectoryInfo(Path.Combine(Path.Combine(OutputDirectory, "Include"), FolderTypeName));
            if (!OutputDirectoryInfo.Exists)
            {
                OutputDirectoryInfo.Create();
            }

            foreach (FileInfo FileToCopy in SourceDirectoryInfo.GetFiles())
            {
                FileInfo OutFile = new FileInfo(Path.Combine(OutputDirectoryInfo.FullName, FileToCopy.Name));

                if (!OutFile.Exists || FileToCopy.LastWriteTimeUtc != OutFile.LastWriteTimeUtc)
                {
                    try
                    {
                        SetFileAttributeForReplace(OutFile);

                        FileToCopy.CopyTo(OutFile.FullName, true);
                    }
                    //These exception has occurred when spaming the markdownmode publish flag options.  Unlikely to occur in proper use, just raise warning.
                    catch (IOException)
                    {
                        log.Warn(Language.Message("FileCouldNotBeCopiedAlreadyBeingCopiedByAnotherProcess", OutFile.FullName));
                    }
                    catch (UnauthorizedAccessException)
                    {
                        log.Warn(Language.Message("FileCouldNotBeCopiedAlreadyBeingCopiedByAnotherProcess", OutFile.FullName));
                    }
                }
            }

            return true;
        }
        
        
        /// <summary>
        /// On successful file conversion create directory structure
        /// </summary>
        /// <returns></returns>
        public static bool CreateCommonDirectories(string OutputDirectory, string SourceDirectory, Logger log)
        {

            DirectoryInfo OutputDirectoryInfo = new DirectoryInfo(OutputDirectory);
            if (!OutputDirectoryInfo.Exists)
            {
                //Try to create it
                try
                {
                    OutputDirectoryInfo.Create();
                }
                catch (IOException e)
                {
                    log.Error(Language.Message("DocumentOutputDirCouldNotBeCreated", e.GetType().Name, OutputDirectory));
                    return false;
                }
            }

            //Check CSS folders exists in source include directory
            if (!CopyDirectory(OutputDirectory, SourceDirectory, "CSS", log))
            {
                return false;
            }

            //Check Images folders exists in source include directory
            if (!CopyDirectory(OutputDirectory, SourceDirectory, "Images", log))
            {
                return false;
            }

            //Check Javascript folders exists in source include directory
            if (!CopyDirectory(OutputDirectory, SourceDirectory, "Javascript", log))
            {
                return false;
            }

            return true;
        }
    }
}

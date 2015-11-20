// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using MarkdownSharp;

using Microsoft.VisualStudio.TestTools.UnitTesting;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;

using DotLiquid;

namespace MarkdownSharpTests.MarkdownTests
{
    using MarkdownSharp.Doxygen;

    /// <summary>
    ///This is a test class for MarkdownTest and is intended
    ///to contain all MarkdownTest Unit Tests
    ///</summary>
    [TestClass()]
    public class MarkdownTest
    {
        /// <summary>
        ///Gets or sets the test context which provides
        ///information about and functionality for the current test run.
        ///</summary>
        public TestContext TestContext
        {
            get
            {
                return this.testContextInstance;
            }
            set
            {
                this.testContextInstance = value;
            }
        }

        #region Additional test attributes
        // 
        //You can use the following additional attributes as you write your tests:
        //
        //Use ClassInitialize to run code before running the first test in the class
        //[ClassInitialize()]
        //public static void MyClassInitialize(TestContext testContext)
        //{
        //}
        //
        //Use ClassCleanup to run code after all tests in a class have run
        //[ClassCleanup()]
        //public static void MyClassCleanup()
        //{
        //}
        //
        //Use TestInitialize to run code before running each test
        //[TestInitialize()]
        //public void MyTestInitialize()
        //{
        //}
        //
        //Use TestCleanup to run code after each test has run
        //[TestCleanup()]
        //public void MyTestCleanup()
        //{
        //}
        //
        #endregion

        [TestMethod]
        public void HorizontalRules()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Doxygen()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Publish()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void BlockQuotes()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Bookmarks()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Headers()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Includes()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void RawHTML()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Tables()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void SpanParser()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Regions()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Objects()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Lists()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void TOCs()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Images()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void CodeBlocks()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Links()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Paragraphs()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        [TestMethod]
        public void Variables()
        {
            TestAndCompare(System.Reflection.MethodBase.GetCurrentMethod().Name);
        }

        /// <summary>
        /// Relative css folder is at HTML\css.  Workout from where the TargetFile is located
        /// </summary>
        /// <param name="targetFileName"></param>
        /// <param name="outputDirectory"></param>
        /// <returns></returns>
        private static string GetRelativeHTMLPath(string targetFileName, string outputDirectory)
        {
            //Step up until we find directory html.
            var currentDirectory = new DirectoryInfo(targetFileName).Parent;
            var outputDir = new DirectoryInfo(outputDirectory);

            var relativePath = @"./";

            while (currentDirectory.FullName.ToLower() != outputDir.FullName.ToLower())
            {
                currentDirectory = currentDirectory.Parent;
                relativePath += @"../";
            }

            return relativePath;
        }

        private static void TestAndCompare(string testName)
        {
            var testDir = GetTestDir(testName);

            DirectoryInfo tempDir;
            var actual = CreateCaseAndRun(testDir, out tempDir);

            var actualFile = new FileInfo(Path.Combine(testDir.FullName, testName.ToLower() + ".actual.html"));

            File.WriteAllText(actualFile.FullName, actual);

            var expectedFile = testDir.GetFiles("*.expected.html")[0];

            var expected = Template.Parse(File.ReadAllText(expectedFile.FullName)).Render(Hash.FromAnonymousObject(new { tempPath = tempDir.FullName }));

            if (!XHTMLComparator.IsEqual(actual, expected))
            {
                Assert.Fail("Translation output not as expected.");
            }
        }

        private static DirectoryInfo GetTestDir(string testName)
        {
            var cur = GetUnrealDocToolDirectory();
            
            return new DirectoryInfo(Path.Combine(cur.FullName, "MarkdownSharpTests", "MarkdownTests", testName));
        }

        private const string Prefix = "Availability: Public\nTitle: Test Case\nTemplate: Test.html\n\n\n";

        private static string CreateCaseAndRun(DirectoryInfo srcTestDirectory, out DirectoryInfo tempDir)
        {
            var udnFile = srcTestDirectory.GetFiles("*.udn")[0];

            var sourceDirectory = Regex.Replace(Settings.Default.SourceDirectory, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");

            tempDir = new DirectoryInfo(Path.Combine(sourceDirectory, "Temp"));

            if (tempDir.Exists)
            {
                tempDir.Delete(true);
            }

            tempDir.Create();

            var imgDir = new DirectoryInfo(Path.Combine(srcTestDirectory.FullName, "Images"));
            if (imgDir.Exists)
            {
                CopyDirectory(imgDir.FullName, Path.Combine(tempDir.FullName, "Images"));
            }

            var tmpFile = new FileInfo(Path.Combine(tempDir.FullName, "tmp.INT.udn"));

            File.WriteAllText(tmpFile.FullName, Prefix + File.ReadAllText(udnFile.FullName));

            DoxygenHelper.DoxygenXmlPath = srcTestDirectory.FullName;

            var output = TestCase(tmpFile.FullName);

            tempDir.Delete(true);

            return output;
        }

        private static void CopyDirectory(string src, string dst)
        {
            var srcInfo = new DirectoryInfo(src);
            var dstInfo = new DirectoryInfo(dst);

            if (!srcInfo.Exists)
            {
                throw new ArgumentException("Argument should be existing directory path.", "src");
            }

            if (dstInfo.Exists)
            {
                throw new ArgumentException("Argument should be non-existing directory path.", "dst");
            }

            dstInfo.Create();

            foreach (var dir in srcInfo.GetDirectories())
            {
                CopyDirectory(dir.FullName, Path.Combine(dstInfo.FullName, dir.Name));
            }

            foreach (var file in srcInfo.GetFiles())
            {
                File.Copy(file.FullName, Path.Combine(dstInfo.FullName, file.Name));
                File.SetAttributes(
                    Path.Combine(dstInfo.FullName, file.Name),
                    File.GetAttributes(Path.Combine(dstInfo.FullName, file.Name))
                        & ~(FileAttributes.ReadOnly | FileAttributes.Archive));
            }
        }

        public static string TestCase(string inputFileName, List<ErrorDetail> errorList = null)
        {
            var errors = errorList ?? new List<ErrorDetail>();

            var targetFileName = GetTargetFileName(inputFileName);
            var sourceDirectory = Regex.Replace(Settings.Default.SourceDirectory, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");
            var outputDirectory = Regex.Replace(Settings.Default.OutputDirectory, @"(^\.[/|\\])?[\\|/]*(.*?)[\\|/]*$", "$2");

            Language.LangID = "INT";

            Language.Init(
                Path.Combine(sourceDirectory, Settings.Default.IncludeDirectory, Settings.Default.InternationalizationDirectory));

            var markdown = new Markdown();

            Markdown.MetadataErrorIfMissing = Settings.Default.MetadataErrorIfMissing.Split(',');
            Markdown.MetadataInfoIfMissing = Settings.Default.MetadataInfoIfMissing.Split(',');
            markdown.PublishFlags = new List<string>() { "Licensee", "Public" };
            markdown.AllSupportedAvailability =
                (Settings.Default.SupportedAvailability + ",Public").ToLower().Split(',').ToList();

            markdown.DoNotPublishAvailabilityFlag = "NoPublish";

            markdown.DefaultTemplate = Settings.Default.DefaultTemplate;

            markdown.ThisIsPreview = false;

            markdown.SupportedLanguages = Settings.Default.SupportedLanguages.ToUpper().Split(',');
            markdown.SupportedLanguageLabels = Settings.Default.SupportedLanguageLabels.Split(',');
            for(int i = 0; i < markdown.SupportedLanguages.Length ; i++)
            {
                markdown.SupportedLanguageMap.Add(markdown.SupportedLanguages[i], markdown.SupportedLanguageLabels[i]);
            }

            //Pass the default conversion settings to Markdown for use in the imagedetails creation.
            markdown.DefaultImageDoCompress = Settings.Default.DoCompressImages;
            markdown.DefaultImageFormatExtension = Settings.Default.CompressImageType;
            markdown.DefaultImageFillColor = ImageConversion.GetColorFromHexString(Settings.Default.ImageFillColor);
            markdown.DefaultImageFormat = ImageConversion.GetImageFormat(Settings.Default.CompressImageType);
            markdown.DefaultImageQuality = Settings.Default.ImageJPGCompressionValue;

            var imageDetails = new List<ImageConversion>();
            var attachNames = new List<AttachmentConversionDetail>();
            var languagesLinksToGenerate = new[] { "INT " };

            var fileOutputDirectory =
                (new DirectoryInfo(inputFileName).Parent).FullName.Replace(
                    sourceDirectory + Path.DirectorySeparatorChar.ToString(CultureInfo.InvariantCulture), "")
                                                         .Replace(sourceDirectory, "");

            var currentFolderDetails = new FolderDetails(
                GetRelativeHTMLPath(targetFileName, outputDirectory),
                new DirectoryInfo(outputDirectory).FullName,
                new DirectoryInfo(sourceDirectory).FullName,
                "Temp",
                "API",
                (new DirectoryInfo(inputFileName).Parent).FullName.Replace(
                    sourceDirectory + Path.DirectorySeparatorChar.ToString(), "")
                                                         .Replace(sourceDirectory, "")
                                                         .Replace(Path.DirectorySeparatorChar.ToString(), " - "),
                GetLanguage(inputFileName));

            var text = File.ReadAllText(inputFileName);

            var actual = markdown.Transform(
                text, errors, imageDetails, attachNames, currentFolderDetails, languagesLinksToGenerate);

            return actual;
        }

        private static string GetTargetFileName(string inputFileName)
        {
            var lang = GetLanguage(inputFileName);

            var splittedPath = inputFileName.Split(Path.DirectorySeparatorChar);

            var drive = splittedPath[0];

            splittedPath[0] = "";
            splittedPath[splittedPath.Length - 1] = "index.html";

            for (var i = splittedPath.Length - 2; i >= 0; --i)
            {
                if (splittedPath[i].ToLower() == "source")
                {
                    splittedPath[i] = Path.Combine("HTML", lang);
                }
            }

            return drive + Path.DirectorySeparatorChar + Path.Combine(splittedPath);
        }

        private static string GetLanguage(string fileName)
        {
            var nameElements = Path.GetFileName(fileName).Split('.');

            return nameElements[nameElements.Length - 2];
        }

        private TestContext testContextInstance;

        public static DirectoryInfo GetUnrealDocToolDirectory()
        {
            var cur = new DirectoryInfo(".");

            while (cur.Name != "UnrealDocTool")
            {
                cur = cur.Parent;
            }

            return cur;
        }
    }
}

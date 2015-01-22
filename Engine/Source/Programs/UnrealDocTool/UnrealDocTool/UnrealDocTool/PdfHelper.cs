// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;

using Pechkin;
using Pechkin.Synchronized;

namespace UnrealDocTool
{
    public static class PdfHelper
    {
        public static void CreatePdfFromHtml(string htmlFilePath)
        {
            var htmlFileInfo = new FileInfo(htmlFilePath);

            if (!htmlFileInfo.Exists)
            {
                throw new FileNotFoundException(htmlFileInfo.FullName);
            }

            Debug.Assert(htmlFileInfo.DirectoryName != null, "htmlFileInfo.DirectoryName != null");

            var tmpPdfFileInfo = new FileInfo(Path.Combine(htmlFileInfo.DirectoryName, "tmp.pdf"));
            var pdfOutFileInfo = new FileInfo(GetPdfEquivalentPath(htmlFileInfo.FullName));

            var gc = new GlobalConfig();

            gc.SetImageQuality(100);
            gc.SetOutputDpi(96);
            gc.SetPaperSize(1024, 1123);

            var oc = new ObjectConfig();

            oc.SetLoadImages(true);
            oc.SetAllowLocalContent(true);
            oc.SetPrintBackground(true);
            oc.SetZoomFactor(1.093);
            oc.SetPageUri(htmlFileInfo.FullName);

            if (tmpPdfFileInfo.Exists)
            {
                tmpPdfFileInfo.Delete();
            }

            IPechkin pechkin = new SynchronizedPechkin(gc);

            pechkin.Error += (converter, text) =>
                {
                    Console.Out.WriteLine("error " + text);
                };

            pechkin.Warning += (converter, text) =>
                {
                    Console.Out.WriteLine("warning " + text);
                };

            using (var file = File.OpenWrite(tmpPdfFileInfo.FullName))
            {
                var bytes = pechkin.Convert(oc);
                file.Write(bytes, 0, bytes.Length);
            }

            if (pdfOutFileInfo.Exists)
            {
                pdfOutFileInfo.Delete();
            }

            CreateDirectories(pdfOutFileInfo.DirectoryName);

            tmpPdfFileInfo.MoveTo(pdfOutFileInfo.FullName);
        }

        private static void CreateDirectories(string dirPath)
        {
            var directoryInfo = new DirectoryInfo(dirPath);

            var listToCreate = new LinkedList<DirectoryInfo>();

            while (true)
            {
                Debug.Assert(directoryInfo != null, "directoryInfo != null");

                if (directoryInfo.Exists)
                {
                    break;
                }

                listToCreate.AddFirst(directoryInfo);

                directoryInfo = directoryInfo.Parent;
            }

            foreach (var di in listToCreate)
            {
                di.Create();
            }
        }

        private static string GetPdfEquivalentPath(string htmlPath)
        {
            var splittedPath = htmlPath.Split(Path.DirectorySeparatorChar);

            for (var i = splittedPath.Length - 1; i >= 0; --i)
            {
                if(splittedPath[i].Equals("HTML", StringComparison.OrdinalIgnoreCase))
                {
                    splittedPath[i] = "PDF";
                    break;
                }
            }

            splittedPath[splittedPath.Length - 1] = Path.ChangeExtension(splittedPath[splittedPath.Length - 1], ".pdf");

            return string.Join(new string(Path.DirectorySeparatorChar, 1), splittedPath);
        }
    }
}

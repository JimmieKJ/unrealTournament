// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MarkdownSharp.EpicMarkdown.PathProviders
{
    using System.Text.RegularExpressions;

    public abstract class EMPathProvider
    {
        public abstract string GetPath(TransformationData data);

        public static EMPathProvider CreatePath(string userPath, EMDocument currentDocument, TransformationData data)
        {
            if (EMEMailPath.Verify(userPath))
            {
                return new EMEMailPath(userPath);
            }

            if (EMExternalPath.Verify(userPath))
            {
                return new EMExternalPath(userPath);
            }

            if (EMLocalFilePath.Verify(userPath))
            {
                return new EMLocalFilePath(userPath, currentDocument, data);
            }

            return new EMLocalDocumentPath(userPath, currentDocument, data);
        }

        public virtual bool IsBookmark { get { return false; } }
        public virtual bool ChangedLanguage { get { return false; } }
    }
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.Text.Tagging;

namespace SpellChecker.Definitions
{
    public interface IMisspellingTag : ITag
    {
        IEnumerable<string> Suggestions { get; }
    }
}

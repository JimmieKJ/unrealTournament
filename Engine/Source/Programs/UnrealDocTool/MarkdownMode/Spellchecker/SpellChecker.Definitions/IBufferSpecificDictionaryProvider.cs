// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.Text;

namespace SpellChecker.Definitions
{
    public interface IBufferSpecificDictionaryProvider
    {
        ISpellingDictionary GetDictionary(ITextBuffer buffer);
    }
}

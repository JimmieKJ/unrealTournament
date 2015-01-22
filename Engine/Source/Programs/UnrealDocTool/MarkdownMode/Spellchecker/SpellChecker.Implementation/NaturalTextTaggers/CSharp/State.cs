// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    enum State
    {
        Default,             // default start state.

        Comment,             // single-line comment (// ...)
        MultiLineComment,    // multi-line comment (/*...*/)

        DocComment,          // doc comment (/// ...)
        DocCommentXml,       // doc comment xml (/// <...> blah)
        DocCommentXmlString, // doc comment xml string (/// <blah bar="...">)

        String,              // string ("...")
        MultiLineString,     // multi-line string (@"...")

        Character,           // character ('.')
    }
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace UnrealDocToolDoxygenInputFilter
{
    using System.Text.RegularExpressions;

    class Program
    {
        static void Main(string[] args)
        {
            var filePath = args[0];

            var lines = File.ReadAllLines(filePath);

            FilterOutUCLASS(lines);

            foreach (var line in lines)
            {
                Console.Out.WriteLine(line);
            }
        }

        private static readonly Regex UClassMacroStartPattern = new Regex(@"UCLASS\s*\(", RegexOptions.Compiled);

        private static void FilterOutUCLASS(string[] lines)
        {
            for (var i = 0; i < lines.Length; ++i)
            {
                var line = lines[i];
                var match = UClassMacroStartPattern.Match(line);

                if (!match.Success)
                {
                    continue;
                }

                var start = match.Index;
                var end = 0;

                var parenthesisLevel = 1;
                for (var k = match.Index + match.Length; k < line.Length; ++k)
                {
                    switch (line[k])
                    {
                        case '(':
                            ++parenthesisLevel;
                            break;
                        case ')':
                            --parenthesisLevel;
                            break;
                    }

                    if (parenthesisLevel != 0)
                    {
                        continue;
                    }

                    end = k + 1;

                    break;
                }

                lines[i] = line.Substring(0, start) + line.Substring(end);
            }
        }
    }
}

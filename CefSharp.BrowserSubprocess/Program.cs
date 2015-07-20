﻿// Copyright © 2010-2015 The CefSharp Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

using System;
using System.Collections.Generic;
using System.Linq;
using CefSharp.Internals;

namespace CefSharp.BrowserSubprocess
{
    public class Program
    {
        public static int Main(string[] args)
        {
            Kernel32.OutputDebugString("BrowserSubprocess starting up with command line: " + String.Join("\n", args));

            int result;

            using (var subprocess = Create(args))
            {
                result = subprocess.Run();
            }

            Kernel32.OutputDebugString("BrowserSubprocess shutting down.");
            return result;
        }

        public static CefSubProcess Create(IEnumerable<string> args)
        {
            const string typePrefix = "--type=";
            var typeArgument = args.SingleOrDefault(arg => arg.StartsWith(typePrefix));
            var wcfEnabled = args.Any(a => a.StartsWith(CefSharpArguments.WcfEnabledArgument));

            var type = typeArgument.Substring(typePrefix.Length);

            switch (type)
            {
                case "renderer":
                {
                    return new CefRenderProcess(args, wcfEnabled);
                }
                case "gpu-process":
                {
                    return new CefGpuProcess(args);
                }
                default:
                {
                    return new CefSubProcess(args);
                }
            }
        }
    }
}

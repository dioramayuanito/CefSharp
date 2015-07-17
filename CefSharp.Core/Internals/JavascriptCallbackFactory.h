// Copyright � 2010-2015 The CefSharp Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "JavascriptCallbackProxy.h"

using namespace System;

namespace CefSharp
{
    namespace Internals
    {
        private ref class JavascriptCallbackFactory : public IJavascriptCallbackFactory
        {
        private:
            PendingTaskRepository<JavascriptResponse^>^ _pendingTasks;
        public:
            JavascriptCallbackFactory(PendingTaskRepository<JavascriptResponse^>^ pendingTasks)
                :_pendingTasks(pendingTasks)
            {
            }

            property WeakReference^ BrowserWrapper;

            virtual IJavascriptCallback^ Create(JavascriptCallback^ callback)
            {
                return gcnew JavascriptCallbackProxy(callback, _pendingTasks, BrowserWrapper);
            }
        };
    }
}
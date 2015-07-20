// Copyright � 2010-2015 The CefSharp Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#pragma once

#include "include\cef_v8.h"

#include "CefV8ValueWrapper.h"
#include "CefV8ContextWrapper.h"

using namespace CefSharp::Internals;

namespace CefSharp
{
    namespace Internals
    {
        private ref class JavascriptCallbackWrapper
        {
        private:
            MCefRefPtr<CefV8Value> _value;
            MCefRefPtr<CefV8Context> _context;

        public:
            JavascriptCallbackWrapper(CefRefPtr<CefV8Value> value, CefRefPtr<CefV8Context> context)
                : _value(value), _context(context) 
            {
            }

            !JavascriptCallbackWrapper()
            {
                _value = nullptr;
                _context = nullptr;
            }

            ~JavascriptCallbackWrapper()
            {
                this->!JavascriptCallbackWrapper();
            }

            CefV8ValueWrapper^ GetValue()
            {
                return gcnew CefV8ValueWrapper(_value.get());
            }

            CefV8ContextWrapper^ GetContext()
            {
                return gcnew CefV8ContextWrapper(_context.get());
            }
        };
    }
}
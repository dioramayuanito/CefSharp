﻿// Copyright © 2010-2015 The CefSharp Project. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
#pragma once

#include "Stdafx.h"

#include "CefBrowserWrapper.h"
#include "CefAppUnmanagedWrapper.h"
#include "JavascriptRootObjectWrapper.h"
#include "Serialization\V8Serialization.h"
#include "..\CefSharp.Core\Internals\CefProcessMessageWrapper.h"
#include "..\CefSharp.Core\Internals\Serialization\Primitives.h"

using namespace System;
using namespace System::Diagnostics;
using namespace System::Collections::Generic;
using namespace CefSharp::Internals::Serialization;

namespace CefSharp
{
    CefRefPtr<CefRenderProcessHandler> CefAppUnmanagedWrapper::GetRenderProcessHandler()
    {
        return this;
    };

    // CefRenderProcessHandler
    void CefAppUnmanagedWrapper::OnBrowserCreated(CefRefPtr<CefBrowser> browser)
    {
        auto wrapper = gcnew CefBrowserWrapper(browser);
        _onBrowserCreated->Invoke(wrapper);

        //Multiple CefBrowserWrappers created when opening popups
        _browserWrappers->Add(browser->GetIdentifier(), wrapper);
    }

    void CefAppUnmanagedWrapper::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
    {
        auto wrapper = FindBrowserWrapper(browser->GetIdentifier(), false);

        if (wrapper != nullptr)
        {
            _browserWrappers->Remove(wrapper->BrowserId);
            _onBrowserDestroyed->Invoke(wrapper);
            delete wrapper;
        }
    };

    void CefAppUnmanagedWrapper::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
    {
        auto wrapper = FindBrowserWrapper(browser->GetIdentifier(), true);

        if (wrapper->JavascriptRootObject != nullptr)
        {
            auto window = context->GetGlobal();

            wrapper->JavascriptRootObjectWrapper = gcnew JavascriptRootObjectWrapper(wrapper->JavascriptRootObject, wrapper->BrowserProcess);

            wrapper->JavascriptRootObjectWrapper->V8Value = window;
            wrapper->JavascriptRootObjectWrapper->Bind();
        }
    };

    void CefAppUnmanagedWrapper::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
    { 
        auto wrapper = FindBrowserWrapper(browser->GetIdentifier(), true);

        if (wrapper->JavascriptRootObjectWrapper != nullptr)
        {
            delete wrapper->JavascriptRootObjectWrapper;
            wrapper->JavascriptRootObjectWrapper = nullptr;
        }
    };

    CefBrowserWrapper^ CefAppUnmanagedWrapper::FindBrowserWrapper(int browserId, bool mustExist)
    {
        CefBrowserWrapper^ wrapper = nullptr;

        _browserWrappers->TryGetValue(browserId, wrapper);

        if (mustExist && wrapper == nullptr)
        {
            throw gcnew InvalidOperationException(String::Format("Failed to identify BrowserWrapper in OnContextCreated. : {0}", browserId));
        }

        return wrapper;
    }

    bool CefAppUnmanagedWrapper::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId sourceProcessId, CefRefPtr<CefProcessMessage> message)
    {
        auto wrapper = FindBrowserWrapper(browser->GetIdentifier(), false);

        if (wrapper == nullptr)
        {
            return false;
        }
        
        return _onProcessMessageReceived->Invoke(wrapper, (ProcessId)sourceProcessId, gcnew CefProcessMessageWrapper(message));
        
        auto handled = false;
        auto name = message->GetName();
        auto argList = message->GetArgumentList();

        auto browserWrapper = FindBrowserWrapper(browser->GetIdentifier(), false);

        //Error handling for missing/closed browser
        if (browserWrapper == nullptr)
        {
            if (name == StringUtils::ToNative(Messages::JavascriptCallbackDestroyRequest))
            {
                //If we can't find the browser wrapper then we'll just
                //ignore this as it's likely already been disposed of
                return true;
            }

            CefString responseName;
            if (name == StringUtils::ToNative(Messages::EvaluateJavascriptRequest))
            {
                responseName = StringUtils::ToNative(Messages::EvaluateJavascriptResponse);
            }
            else if (name == StringUtils::ToNative(Messages::JavascriptCallbackRequest))
            {
                responseName = StringUtils::ToNative(Messages::JavascriptCallbackResponse);
            }
            else
            {
                //TODO: Should be throw an exception here? It's likely that only a CefSharp developer would see this
                // when they added a new message and havn't yet implemented the render process functionality.
                throw gcnew Exception("Unsupported message type");
            }

            auto callbackId = GetInt64(argList, 1);
            auto response = CefProcessMessage::Create(responseName);
            auto responseArgList = response->GetArgumentList();
            auto errorMessage = String::Format("Request BrowserId : {0} not found it's likely the browser is already closed", browser->GetIdentifier());

            //success: false
            responseArgList->SetBool(0, false);
            SetInt64(callbackId, responseArgList, 1);
            responseArgList->SetString(2, StringUtils::ToNative(errorMessage));
            browser->SendProcessMessage(sourceProcessId, response);

            return true;
        }
    
        //these messages are roughly handled the same way
        if (name == StringUtils::ToNative(Messages::EvaluateJavascriptRequest)
         || name == StringUtils::ToNative(Messages::JavascriptCallbackRequest))
        {
            bool success;
            CefRefPtr<CefV8Value> result;
            CefString errorMessage;
            CefRefPtr<CefProcessMessage> response;
            //both messages have the callbackid stored at index 1
            int64 callbackId = GetInt64(argList, 1);

            if (name == StringUtils::ToNative(Messages::EvaluateJavascriptRequest))
            {
                auto frameId = GetInt64(argList, 0);
                auto script = argList->GetString(2);

                response = CefProcessMessage::Create(StringUtils::ToNative(Messages::EvaluateJavascriptResponse));

                auto frame = browser->GetFrame(frameId);
                if (frame.get())
                {
                    auto context = frame->GetV8Context();
                    
                    if (context.get() && context->Enter())
                    {
                        try
                        {
                            CefRefPtr<CefV8Exception> exception;
                            success = context->Eval(script, result, exception);
                            
                            //we need to do this here to be able to store the v8context
                            if (success)
                            {
                                auto responseArgList = response->GetArgumentList();
                                SerializeV8Object(result, responseArgList, 2, browserWrapper->CallbackRegistry);
                            }
                            else
                            {
                                errorMessage = exception->GetMessage();
                            }
                        }
                        finally
                        {
                            context->Exit();
                        }
                    }
                    else
                    {
                        errorMessage = "Unable to Enter Context";
                    }
                }
                else
                {
                    errorMessage = "Unable to Get Frame matching Id";
                }
            }
            else
            {
                auto jsCallbackId = GetInt64(argList, 0);
                auto parameterList = argList->GetList(2);
                CefV8ValueList params;
                for (CefV8ValueList::size_type i = 0; i < parameterList->GetSize(); i++)
                {
                    params.push_back(DeserializeV8Object(parameterList, static_cast<int>(i)));
                }

                response = CefProcessMessage::Create(StringUtils::ToNative(Messages::JavascriptCallbackResponse));

                auto callbackRegistry = browserWrapper->CallbackRegistry;
                auto callbackWrapper = callbackRegistry->FindWrapper(jsCallbackId);
                auto context = callbackWrapper->GetContext();
                auto value = callbackWrapper->GetValue();
                
                if (context.get() && context->Enter())
                {
                    try
                    {
                        result = value->ExecuteFunction(nullptr, params);
                        success = result.get() != nullptr;
                        
                        //we need to do this here to be able to store the v8context
                        if (success)
                        {
                            auto responseArgList = response->GetArgumentList();
                            SerializeV8Object(result, responseArgList, 2, browserWrapper->CallbackRegistry);
                        }
                        else
                        {
                            auto exception = value->GetException();
                            if(exception.get())
                            {
                                errorMessage = exception->GetMessage();
                            }
                        }
                    }
                    finally
                    {
                        context->Exit();
                    }
                }
                else
                {
                    errorMessage = "Unable to Enter Context";			
                }                
            }

            if (response.get())
            {
                auto responseArgList = response->GetArgumentList();
                responseArgList->SetBool(0, success);
                SetInt64(callbackId, responseArgList, 1);
                if (!success)
                {
                    responseArgList->SetString(2, errorMessage);
                }
                browser->SendProcessMessage(sourceProcessId, response);
            }

            handled = true;
        }
        else if (name == StringUtils::ToNative(Messages::JavascriptCallbackDestroyRequest))
        {
            auto jsCallbackId = GetInt64(argList, 0);
            browserWrapper->CallbackRegistry->Deregister(jsCallbackId);

            handled = true;
        }

        return handled;
    };
}
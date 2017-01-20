/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/std/typetraits/is_base_of.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            template<typename Class, typename ContextType>
            void CallProcessorBinder::BindToCall(ProcessingResult(Class::*Func)(ContextType& context) const)
            {
                AZ_STATIC_ASSERT((AZStd::is_base_of<CallProcessorBinder, Class>::value), 
                    "CallProcessorBinder can only bind to classes derived from it.");
                AZ_STATIC_ASSERT((AZStd::is_base_of<ICallContext, ContextType>::value), 
                    "Only arguments derived from ICallContext are accepted by CallProcessorBinder");

                using Binder = ConstFunctionBindingTemplate<Class, ContextType>;
                m_bindings.emplace_back(new Binder(Func));
            }

            template<typename Class, typename ContextType>
            void CallProcessorBinder::BindToCall(ProcessingResult(Class::*Func)(ContextType& context))
            {
                AZ_STATIC_ASSERT((AZStd::is_base_of<CallProcessorBinder, Class>::value),
                    "CallProcessorBinder can only bind to classes derived from it.");
                AZ_STATIC_ASSERT((AZStd::is_base_of<ICallContext, ContextType>::value),
                    "Only arguments derived from ICallContext are accepted by CallProcessorBinder");

                using Binder = FunctionBindingTemplate<Class, ContextType>;
                m_bindings.emplace_back(new Binder(Func));
            }

            // ConstFunctionBindingTemplate
            template<typename Class, typename ContextType>
            CallProcessorBinder::ConstFunctionBindingTemplate<Class, ContextType>::ConstFunctionBindingTemplate(Function function)
                : m_function(function)
            {
            }

            template<typename Class, typename ContextType>
            ProcessingResult CallProcessorBinder::ConstFunctionBindingTemplate<Class, ContextType>::Process(
                CallProcessorBinder* thisPtr, ICallContext* context)
            {
                if (context && context->RTTI_GetType() == ContextType::TYPEINFO_Uuid())
                {
                    ContextType* arg = azrtti_cast<ContextType*>(context);
                    if (arg)
                    {
                        AZ_Assert(arg, "CallProcessorBinder failed to cast in context for unknown reasons.");
                        return (reinterpret_cast<Class*>(thisPtr)->*(m_function))(*arg);
                    }
                }
                return ProcessingResult::Ignored;
            }

            //FunctionBindingTemplate
            template<typename Class, typename ContextType>
            CallProcessorBinder::FunctionBindingTemplate<Class, ContextType>::FunctionBindingTemplate(Function function)
                : m_function(function)
            {
            }

            template<typename Class, typename ContextType>
            ProcessingResult CallProcessorBinder::FunctionBindingTemplate<Class, ContextType>::Process(
                CallProcessorBinder* thisPtr, ICallContext* context)
            {
                if (context && context->RTTI_GetType() == ContextType::TYPEINFO_Uuid())
                {
                    ContextType* arg = azrtti_cast<ContextType*>(context);
                    if (arg)
                    {
                        AZ_Assert(arg, "CallProcessorBinder failed to cast in contetx for unknown reasons.");
                        return (reinterpret_cast<Class*>(thisPtr)->*(m_function))(*arg);
                    }
                }
                return ProcessingResult::Ignored;
            }
        } // Events
    } // SceneAPI
} // AZ

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
#pragma once

#include <QtCore/QString>
#include <QtWidgets/QApplication>

#include <AzCore/Component/Component.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string.h>
#include <AzToolsFramework/UI/UICore/StylesheetPreprocessor.hxx>

namespace AzToolsFramework
{
    namespace Components
    {
        class QtRequests
            : public AZ::EBusTraits
        {
        public:
            //////////////////////////////////////////////////////////////////////////
            // EBusTraits overrides
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
            //////////////////////////////////////////////////////////////////////////

            //! Lock and execute the Qt Event loop
            virtual int Exec() = 0;
            virtual AZ::Outcome<void, AZStd::string> RefreshStylesheet() = 0;
        };
        using QtRequestBus = AZ::EBus<QtRequests>;

        class QtComponent
            : public AZ::Component
            , public QtRequestBus::Handler
        {
        public:
            AZ_COMPONENT(QtComponent, "{51A48724-04C7-4933-868F-FB4C4F8E330A}")

            //////////////////////////////////////////////////////////////////////////
            //! ComponentDescriptor
            static void Reflect(AZ::ReflectContext* context);
            //////////////////////////////////////////////////////////////////////////

            QtComponent()
                : m_stylesheetPreprocessor(nullptr)
            { }
            ~QtComponent() override = default;

            void SetArgs(int argc, char** argv);

            //////////////////////////////////////////////////////////////////////////
            //! AZ::Component
            void Activate() override;
            void Deactivate() override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            //! QtComponent
            int Exec() override;
            AZ::Outcome<void, AZStd::string> RefreshStylesheet() override;
            //////////////////////////////////////////////////////////////////////////

        private:
            static QString AppendToCWD(const char subpath[]);

            int m_argc;
            char** m_argv;
            QApplication* m_qApp;
            AzToolsFramework::StylesheetPreprocessor m_stylesheetPreprocessor;
        };
    }
}

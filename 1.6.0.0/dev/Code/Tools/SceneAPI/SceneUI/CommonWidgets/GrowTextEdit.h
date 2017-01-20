#pragma once

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


#include <QTextEdit>
#include <AzCore/std/string/string.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            class GrowTextEdit : public QTextEdit
            {
                Q_OBJECT
            public:
                AZ_CLASS_ALLOCATOR_DECL

                explicit GrowTextEdit(QWidget* parent = nullptr);

                void SetText(const AZStd::string& text);
                AZStd::string GetText() const;

                void setVisible(bool visible) override;
                QSize sizeHint() const override;

            private:
                static const int s_padding;
            };
        }
    }
}
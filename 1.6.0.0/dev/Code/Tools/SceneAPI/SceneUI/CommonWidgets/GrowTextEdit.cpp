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

#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneUI/CommonWidgets/GrowTextEdit.h>
#include <QTextBlock>

namespace AZ
{
    namespace SceneAPI
    {
        namespace UI
        {
            const int GrowTextEdit::s_padding = 10;

            AZ_CLASS_ALLOCATOR_IMPL(GrowTextEdit, SystemAllocator, 0)

            GrowTextEdit::GrowTextEdit(QWidget* parent)
                : QTextEdit(parent)
            {
                setSizePolicy(QSizePolicy::Policy::Ignored, QSizePolicy::Policy::Maximum);
                setMinimumHeight(60);

                connect(this, &GrowTextEdit::textChanged, 
                    [this]()
                    {
                        if (isVisible())
                        {
                            updateGeometry();
                        }
                    }
                );
            }

            void GrowTextEdit::SetText(const AZStd::string& text)
            {
                setPlainText(text.c_str());

                document()->adjustSize();
                updateGeometry();
            }

            AZStd::string GrowTextEdit::GetText() const
            {
                return AZStd::string(toPlainText().toLatin1());
            }

            void GrowTextEdit::setVisible(bool visible)
            {
                QTextEdit::setVisible(visible);
                if (visible)
                {
                    updateGeometry();
                }
            }

            QSize GrowTextEdit::sizeHint() const
            {
                QSize sizeHint = QTextEdit::sizeHint();
                QSize documentSize = document()->size().toSize();
                sizeHint.setHeight(documentSize.height() + s_padding);

                return sizeHint;
            }
        }
    }
}

#include <CommonWidgets/GrowTextEdit.moc>
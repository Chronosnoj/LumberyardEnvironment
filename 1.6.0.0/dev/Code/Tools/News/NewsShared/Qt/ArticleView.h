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

#include <QWidget>

namespace Ui
{
    class ArticleViewWidget;
}

class QBetterLabel;

namespace News
{
    class ArticleDescriptor;
    class ResourceManifest;

    class ArticleView
        : public QWidget
    {
        Q_OBJECT
    public:
        ArticleView(QWidget* parent,
            const ArticleDescriptor& article,
            const ResourceManifest& manifest);
        ~ArticleView();

        void Update();
        const ArticleDescriptor* GetArticle() const;
        void SetStylesheet(const char* css) const;

    Q_SIGNALS:
        void articleSelectedSignal(QString resourceId);
        void linkActivatedSignal(const QString& link);

    protected:
        void mousePressEvent(QMouseEvent* event);

    private:
        static const QSize ICON_SIZE;

        QScopedPointer<Ui::ArticleViewWidget> m_ui;
        ArticleDescriptor* m_pArticle;
        const ResourceManifest& m_manifest;
        QBetterLabel* m_icon;

        void RemoveIcon();

    private Q_SLOTS:
        void linkActivatedSlot(const QString& link);
        void articleSelectedSlot();
    };
}

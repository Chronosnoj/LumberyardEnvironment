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

#include "NewsShared/LogType.h"

#include <QWidget>
#include <QList>

class QLabel;

namespace Ui
{
    class ArticleViewContainerWidget;
}

namespace News
{
    class Resource;
    class ArticleView;
    class ArticleDescriptor;
    class ResourceManifest;
    class ArticleErrorView;

    class ArticleViewContainer
        : public QWidget
    {
        Q_OBJECT
    public:
        explicit ArticleViewContainer(QWidget* parent, ResourceManifest& manifest);
        ~ArticleViewContainer();

        virtual void PopulateArticles();
        ArticleView* FindById(const QString& id);
        void AddArticleView(Resource* pResource);
        void DeleteArticleView(ArticleView* pResource);
        void ScrollToView(ArticleView* view) const;
        void UpdateArticleOrder(ArticleView* view, bool direction) const;
        void AddLoadingMessage();
        void AddErrorMessage();
        void Clear();

    Q_SIGNALS:
        void articleSelectedSignal(QString resourceId);
        void addArticle(Resource* article);
        void logSignal(QString text, LogType logType = LogInfo);
        void scrolled(); 
        void linkActivatedSignal(const QString& link);

    private:
        QScopedPointer<Ui::ArticleViewContainerWidget> m_ui;
        QList<ArticleView*> m_articles;
        ResourceManifest& m_manifest;
        QLabel* m_loadingLabel;
        ArticleErrorView* m_errorMessage;

        void ClearError();

    private Q_SLOTS:
        virtual void articleSelectedSlot(QString id);
    };
}
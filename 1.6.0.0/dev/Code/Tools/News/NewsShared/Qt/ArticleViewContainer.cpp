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

#include "ArticleViewContainer.h"
#include "ArticleView.h"
#include "NewsShared/ResourceManagement/ResourceManifest.h"
#include "NewsShared/ResourceManagement/ArticleDescriptor.h"
#include "NewsShared/ResourceManagement/Resource.h"
#include "NewsShared/Qt/ArticleErrorView.h"
#include "NewsShared/Qt/ui_ArticleViewContainer.h"

#include <QLabel>

using namespace News;

ArticleViewContainer::ArticleViewContainer(QWidget* parent, ResourceManifest& manifest)
    : QWidget(parent)
    , m_ui(new Ui::ArticleViewContainerWidget)
    , m_manifest(manifest)
    , m_loadingLabel(nullptr)
    , m_errorMessage(nullptr)
{
    m_ui->setupUi(this);
    connect(m_ui->previewArea, &QBetterScrollArea::scrolled, 
        this, &ArticleViewContainer::scrolled);
    AddLoadingMessage();
}

ArticleViewContainer::~ArticleViewContainer() {}

void ArticleViewContainer::PopulateArticles()
{
    Clear();

    bool articlesFound = false;
    for (auto id : m_manifest.GetOrder())
    {
        auto pResource = m_manifest.FindById(id);
        if (pResource && pResource->GetType().compare("article") == 0)
        {
            AddArticleView(pResource);
            articlesFound = true;
        }
    }

    if (!articlesFound)
    {
        AddErrorMessage();
    }

    qApp->processEvents();
}

ArticleView* ArticleViewContainer::FindById(const QString& id)
{
    auto it = std::find_if(
            m_articles.begin(),
            m_articles.end(),
            [id](ArticleView* articleView) -> bool
    {
        if (!articleView)
        {
            return false;
        }
        return articleView->GetArticle()->GetResource().GetId().compare(id) == 0;
    });
    if (it == m_articles.end())
    {
        return nullptr;
    }
    return *it;
}

void ArticleViewContainer::AddArticleView(Resource* pResource)
{
    ClearError();

    auto view = new ArticleView(
            m_ui->articleViewContents,
            ArticleDescriptor(*pResource),
            m_manifest);
    m_articles.append(view);
    connect(view, &ArticleView::articleSelectedSignal,
            this, &ArticleViewContainer::articleSelectedSlot);
    connect(view, &ArticleView::linkActivatedSignal,
        this, &ArticleViewContainer::linkActivatedSignal);

    auto layout = static_cast<QVBoxLayout*>(m_ui->articleViewContents->layout());
    layout->insertWidget(layout->count() - 1, view);
    qApp->processEvents();
}

void ArticleViewContainer::DeleteArticleView(ArticleView* view)
{
    m_articles.removeAll(view);
    m_ui->articleViewContents->layout()->removeWidget(view);
    delete view;
}

void ArticleViewContainer::ScrollToView(ArticleView* view) const
{
    m_ui->previewArea->ensureWidgetVisible(view);
}

void ArticleViewContainer::ClearError()
{
    //delete loading label
    if (m_loadingLabel)
    {
        delete m_loadingLabel;
        m_loadingLabel = nullptr;
    }

    //delete error message
    if (m_errorMessage)
    {
        delete m_errorMessage;
        m_errorMessage = nullptr;
    }
}

void ArticleViewContainer::articleSelectedSlot(QString id)
{
    emit articleSelectedSignal(id);
}

void ArticleViewContainer::UpdateArticleOrder(ArticleView* view, bool direction) const
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_ui->articleViewContents->layout());

    const int index = layout->indexOf(view);

    if (direction && index == 0)
    {
        return;
    }

    if (!direction && index == layout->count() - 2)
    {
        return;
    }

    const int newIndex = direction ? index - 1 : index + 1;
    layout->removeWidget(view);
    layout->insertWidget(newIndex, view);
}

void ArticleViewContainer::AddLoadingMessage()
{
    if (m_errorMessage)
    {
        delete m_errorMessage;
        m_errorMessage = nullptr;
    }

    if (!m_loadingLabel)
    {
        m_loadingLabel = new QLabel(this);
        m_loadingLabel->setText("Retrieving news...");
        auto layout = static_cast<QVBoxLayout*>(m_ui->articleViewContents->layout());
        layout->insertWidget(0, m_loadingLabel);
    }
}

void ArticleViewContainer::AddErrorMessage()
{
    if (m_loadingLabel)
    {
        delete m_loadingLabel;
        m_loadingLabel = nullptr;
    }

    if (!m_errorMessage)
    {
        m_errorMessage = new ArticleErrorView(this);
        auto layout = static_cast<QVBoxLayout*>(m_ui->articleViewContents->layout());
        layout->insertWidget(0, m_errorMessage);
    }
}

void ArticleViewContainer::Clear()
{
    ClearError();

    for (auto articleView : m_articles)
    {
        delete articleView;
    }

    m_articles.clear();
}

#include "NewsShared/Qt/ArticleViewContainer.moc"

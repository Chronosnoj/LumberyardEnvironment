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

#include "ArticleView.h"
#include "NewsShared/ResourceManagement/ArticleDescriptor.h"
#include "NewsShared/Qt/ui_ArticleView.h"
#include "NewsShared/ResourceManagement/ResourceManifest.h"
#include "NewsShared/ResourceManagement/Resource.h"
#include "NewsShared/Qt/QBetterLabel.h"

#include <QDesktopServices>
#include <QUrl>

using namespace News;

const QSize ArticleView::ICON_SIZE(160, 90);

ArticleView::ArticleView(
    QWidget* parent,
    const ArticleDescriptor& article,
    const ResourceManifest& manifest)
    : QWidget(parent)
    , m_ui(new Ui::ArticleViewWidget())
    , m_pArticle(new ArticleDescriptor(article))
    , m_manifest(manifest)
    , m_icon(nullptr)
{
    m_ui->setupUi(this);

    Update();

    connect(m_ui->titleLabel, &QLabel::linkActivated, this, &ArticleView::linkActivatedSlot);
    connect(m_ui->bodyLabel, &QLabel::linkActivated, this, &ArticleView::linkActivatedSlot);
    connect(m_ui->titleLabel, &QBetterLabel::clicked, this, &ArticleView::articleSelectedSlot);
    connect(m_ui->bodyLabel, &QBetterLabel::clicked, this, &ArticleView::articleSelectedSlot);
}

ArticleView::~ArticleView()
{
    delete m_pArticle;
}

void ArticleView::Update()
{
    auto pResource = m_manifest.FindById(m_pArticle->GetResource().GetId());
    delete m_pArticle;
    if (pResource)
    {
        m_pArticle = new ArticleDescriptor(*pResource);
        m_ui->titleLabel->setText(m_pArticle->GetTitle());
        m_ui->bodyLabel->setText(m_pArticle->GetBody());
        auto pImageResource = m_manifest.FindById(m_pArticle->GetImageId());
        if (pImageResource)
        {
            QPixmap pixmap;
            if (pixmap.loadFromData(pImageResource->GetData()))
            {
                if (!m_icon)
                {
                    m_icon = new QBetterLabel(this);
                    m_icon->setMinimumSize(ICON_SIZE);
                    m_icon->setStyleSheet("border: none;");
                    m_icon->setAlignment(Qt::AlignCenter);
                    static_cast<QVBoxLayout*>(
                        m_ui->frame->layout())->insertWidget(0, m_icon);
                    connect(m_icon, &QBetterLabel::clicked, this, &ArticleView::articleSelectedSlot);
                }
                m_icon->setPixmap(pixmap);
            }
            else
            {
                RemoveIcon();
            }
        }
        else
        {
            RemoveIcon();
        }
    }
}

void ArticleView::mousePressEvent(QMouseEvent* event)
{
    articleSelectedSlot();
}

void ArticleView::RemoveIcon()
{
    if (m_icon)
    {
        delete m_icon;
        m_icon = nullptr;
    }
}

void ArticleView::linkActivatedSlot(const QString& link)
{
    QDesktopServices::openUrl(QUrl(link));
    emit linkActivatedSignal(link);
}

void ArticleView::articleSelectedSlot()
{
    emit articleSelectedSignal(m_pArticle->GetResource().GetId());
}

const ArticleDescriptor* ArticleView::GetArticle() const
{
    return m_pArticle;
}

void ArticleView::SetStylesheet(const char* css) const
{
    m_ui->widget->setStyleSheet(css);
}

#include "NewsShared/Qt/ArticleView.moc"

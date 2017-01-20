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

#include "ArticleDescriptor.h"
#include "Resource.h"

#include <QJsonArray>
#include <QJsonDocument>

using namespace News;

ArticleDescriptor::ArticleDescriptor(
    Resource& resource)
    : JsonDescriptor(resource)
    , m_imageId(m_json["image"].toString())
    , m_title(m_json["title"].toString())
    , m_body(m_json["body"].toString())
    , m_order(m_json["order"].toInt())
{}

void ArticleDescriptor::Update() const
{
    QJsonObject json;
    json["image"] = m_imageId;
    json["title"] = m_title;
    json["body"] = m_body;
    json["order"] = m_order;
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact).toStdString().data();
    m_resource.SetData(data);
}

const QString& ArticleDescriptor::GetImageId() const
{
    return m_imageId;
}

void ArticleDescriptor::SetImageId(const QString& imageId)
{
    m_imageId = imageId;
}

const QString& ArticleDescriptor::GetTitle() const
{
    return m_title;
}

void ArticleDescriptor::SetTitle(const QString& title)
{
    m_title = title;
}

const QString& ArticleDescriptor::GetBody() const
{
    return m_body;
}

void ArticleDescriptor::SetBody(const QString& body)
{
    m_body = body;
}

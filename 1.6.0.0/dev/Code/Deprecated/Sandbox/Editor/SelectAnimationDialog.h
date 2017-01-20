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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#ifndef CRYINCLUDE_EDITOR_SELECTANIMATIONDIALOG_H
#define CRYINCLUDE_EDITOR_SELECTANIMATIONDIALOG_H
#pragma once


#include "GenericSelectItemDialog.h"

struct ICharacterInstance;

class CSelectAnimationDialog : public CGenericSelectItemDialog
{
	Q_OBJECT

public:
	CSelectAnimationDialog(QWidget* pParent = nullptr);   // standard constructor
	virtual ~CSelectAnimationDialog() {}

	void SetCharacterInstance(ICharacterInstance* pCharacterInstance);
	QString GetSelectedItem() override;

protected:
	void OnInitDialog() override;

	// Derived Dialogs should override this
	virtual void GetItems(std::vector<SItem>& outItems);

	// Called whenever an item gets selected
	virtual void ItemSelected();

	ICharacterInstance* m_pCharacterInstance;
};

#endif // CRYINCLUDE_EDITOR_SELECTANIMATIONDIALOG_H

/** @file OutputPreviewWidget.cpp
	@brief Custom widget for texture output previews
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#include "StdAfx.h"

#if defined(USE_SUBSTANCE)

#include "OutputPreviewWidget.h"
#include "QProceduralMaterialEditorMainWindow.h"

static const int PREVIEW_WIDGET_SIZE_SCALAR = 145;
static const QSize PREVIEW_WIDGET_SIZE(PREVIEW_WIDGET_SIZE_SCALAR, PREVIEW_WIDGET_SIZE_SCALAR);

//----------------------------------------------------------------------------------------------------
COutputEnabledUndoCommand::COutputEnabledUndoCommand(QOutputPreviewWidget* pWidget, QProceduralMaterialEditorMainWindow* pMainWindow, bool enabled)
	: QUndoCommand()
	, m_MainWindow(pMainWindow)
	, m_NewValue(enabled)
{
	IGraphOutput* pOutput = pWidget->GetOutput();
	IGraphInstance* pGraphInstance = pOutput->GetGraphInstance();

	m_GraphInstanceID = pGraphInstance->GetGraphInstanceID();
	m_GraphOutputID = pOutput->GetGraphOutputID();
}

void COutputEnabledUndoCommand::undo()
{
	if (IGraphInstance* pGraph = CommitValue(!m_NewValue))
	{
		m_MainWindow->DecrementMaterialModified(pGraph->GetProceduralMaterial());
	}
}

void COutputEnabledUndoCommand::redo()
{
	if (IGraphInstance* pGraph = CommitValue(m_NewValue))
	{
		m_MainWindow->IncrementMaterialModified(pGraph->GetProceduralMaterial());
	}
}

IGraphInstance* COutputEnabledUndoCommand::CommitValue(bool enabled)
{
	if (QOutputPreviewWidget* pWidget = m_MainWindow->GetOutputPreviewWidget(m_GraphOutputID))
	{
		pWidget->GetOutput()->SetEnabled(enabled);
		pWidget->SyncUndoValue();

		return pWidget->GetOutput()->GetGraphInstance();
	}
	else
	{
		IGraphInstance* pGraph = nullptr;
		EBUS_EVENT_RESULT(pGraph, SubstanceRequestBus, GetGraphInstance, m_GraphInstanceID);
		if (pGraph)
		{
			for (int i = 0; i < pGraph->GetOutputCount(); i++)
			{
				IGraphOutput* pOutput = pGraph->GetOutput(i);
				if (pOutput->GetGraphOutputID() == m_GraphOutputID)
				{
					pOutput->SetEnabled(enabled);
					return pGraph;
				}
			}
		}
	}

	return nullptr;
}

//----------------------------------------------------------------------------------------------------
QOutputPreviewWidget::QOutputPreviewWidget(IGraphOutput* pOutput)
	: QWidget()
	, m_Output(pOutput)
{
	setLayout(new QVBoxLayout);

	m_PreviewWidget = new QLabel;
	m_PreviewWidget->setFrameShape(QFrame::Box);
	m_PreviewWidget->setFrameShadow(QFrame::Raised);
	m_PreviewWidget->setMinimumSize(PREVIEW_WIDGET_SIZE);
	m_PreviewWidget->setMaximumSize(PREVIEW_WIDGET_SIZE);
	layout()->addWidget(m_PreviewWidget);

	m_EnabledWidget = new QCheckBox;
	m_EnabledWidget->setText(pOutput->GetLabel());
	m_EnabledWidget->setCheckState(pOutput->IsEnabled() ? Qt::Checked : Qt::Unchecked);
	layout()->addWidget(m_EnabledWidget);
	connect(m_EnabledWidget, &QCheckBox::stateChanged, this, &QOutputPreviewWidget::OnEnabledWidgetStateChanged);
}

void QOutputPreviewWidget::OnEnabledWidgetStateChanged(int checkstate)
{
	//add undo command
	QWidget* main = parentWidget();
	while (main->parentWidget())
	{
		main = main->parentWidget();
	}

	if (QProceduralMaterialEditorMainWindow* mainWindow = qobject_cast<QProceduralMaterialEditorMainWindow*>(main))
	{
		bool enabled = (checkstate == Qt::Checked) ? true : false;
		mainWindow->GetUndoStack()->push(new COutputEnabledUndoCommand(this, mainWindow, enabled));
	}
}

void QOutputPreviewWidget::OnOutputChanged()
{
	SGraphOutputEditorPreview preview;
	if (m_Output->GetEditorPreview(preview))
	{
		m_PreviewImage = QImage(preview.Width, preview.Height, QImage::Format_RGB32);

		for (int y = 0;y < preview.Height;y++)
		{
			QRgb* pScanline = (QRgb*)m_PreviewImage.scanLine(y);

			for (int x = 0; x < preview.Width;x++)
			{
				uchar* psB = (uchar*)(preview.Data) + (x*preview.BytesPerPixel) + (y*preview.BytesPerPixel*preview.Width);
				uchar* psG = psB + 1;
				uchar* psR = psG + 1;

				*(pScanline + x) = qRgb(*psR, *psG, *psB);
			}
		}

		QPixmap pixMap = QPixmap::fromImage(m_PreviewImage);
		pixMap = pixMap.scaled(PREVIEW_WIDGET_SIZE, Qt::KeepAspectRatio);
		m_PreviewWidget->setPixmap(pixMap);
	}
}

void QOutputPreviewWidget::SyncUndoValue()
{
	m_EnabledWidget->blockSignals(true);
	m_EnabledWidget->setChecked(m_Output->IsEnabled());
	m_EnabledWidget->blockSignals(false);
}
#endif // USE_SUBSTANCE

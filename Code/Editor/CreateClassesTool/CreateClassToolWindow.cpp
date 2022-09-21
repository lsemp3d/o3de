/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "EditorDefs.h"

#include "CreateClassToolWindow.h"

 // Qt
#if !defined(Q_MOC_RUN)
#include <QMessageBox>
#include <QListWidget>
#include <QFileDialog>
#endif

// AzCore
#include <AzCore/UserSettings/UserSettingsComponent.h>
#include <AzCore/Utils/Utils.h>

// AzFramework
#include <AzFramework/Gem/GemInfo.h>

// AzToolsFramework
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/API/EditorPythonRunnerRequestsBus.h>

#include <AzQtComponents/Components/FilteredSearchWidget.h>

// Editor
#include "LyViewPaneNames.h"

#include <CreateClassesTool/ui_CreateClassToolWindow.h>

#pragma optimize("", off)


// DESIGN.MD
//
// > Need to talk to core/docs about embedding Component documentation into all components, this will
// > enable us to query the docs data and provide it so that folks wanting to make a derived
// > class know what the base class does
//
//
//
//





CreateClassToolWindow::CreateClassToolWindow(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::CreateClassToolWindow())
{
    m_ui->setupUi(this);

    m_componentClassModel = new QStringListModel(this);

    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel();
    proxyModel->setSourceModel(m_componentClassModel);

    InitializeComponentCreation();

    connect(m_ui->componentName, &QLineEdit::textChanged, this, [this]() { m_componentName = m_ui->componentName->text().toUtf8().data(); });
    connect(m_ui->componentList, &QListWidget::itemSelectionChanged, this, &CreateClassToolWindow::OnBaseComponentClassSelected);

    m_componentClassFilterModel.reset(new ComponentClassFilterModel(this, 0));
    m_componentClassFilterModel->setSourceModel(m_componentClassModel);

    connect(m_ui->fileFilteredSearchWidget,
        &AzQtComponents::FilteredSearchWidget::TextFilterChanged,
        m_componentClassFilterModel.data(),
        static_cast<void (QSortFilterProxyModel::*)(const QString&)>(&ComponentClassFilterModel::FilterChanged));

    if (auto settingsRegistry = AZ::Interface<AZ::SettingsRegistryInterface>::Get(); settingsRegistry != nullptr)
    {
        AZ::IO::Path gemSourceAssetDirectories;
        AZStd::vector<AzFramework::GemInfo> gemInfos;
        if (AzFramework::GetGemsInfo(gemInfos, *settingsRegistry))
        {
            for (auto& it : gemInfos)
            {
                m_ui->gemsList->addItem(it.m_gemName.c_str());

                m_gemsInfo[it.m_gemName.c_str()] = it;

                if (m_selectedGem.empty())
                {
                    m_selectedGem = it.m_gemName;
                }
            }
        }

    }

    connect(m_ui->gemsList, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &CreateClassToolWindow::OnGemChange);
    connect(m_ui->browse, &QPushButton::clicked, this, &CreateClassToolWindow::BrowseForPath);



    //
    connect(m_ui->buttonBox, &QDialogButtonBox::clicked, this, &CreateClassToolWindow::DialogButtonClicked);

}

void CreateClassToolWindow::DialogButtonClicked(const QAbstractButton* button)
{
    if (button == m_ui->buttonBox->button(QDialogButtonBox::Ok))
    {
        AZ::IO::Path componentPath;
        componentPath = m_selectedGemPath;
        componentPath /= m_componentName;

        AZStd::string gemReplace = AZStd::string::format("${GemName} %s", m_selectedGem.c_str());

        AZStd::vector<AZStd::string_view> pythonArgs{ "create-from-template", "-dn", m_componentName, "-dp", componentPath.c_str(), "-tn", "DefaultComponent", "-r", "${GemName}", m_componentName, "${BaseComponent}", m_selectedBaseComponent };

        AzToolsFramework::EditorPythonRunnerRequestBus::Broadcast(
            &AzToolsFramework::EditorPythonRunnerRequestBus::Events::ExecuteByFilenameWithArgs,
            "@engroot@/scripts/o3de.py", pythonArgs);

        //accept();
        
    }
    else
    {
        //reject();
    }
}

void CreateClassToolWindow::BrowseForPath()
{
    QFileDialog* fileDialog = new QFileDialog;
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setWindowModality(Qt::WindowModality::ApplicationModal);
    fileDialog->setViewMode(QFileDialog::Detail);
    fileDialog->setWindowTitle(tr("Select base path"));
    fileDialog->setLabelText(QFileDialog::Accept, "Select");

    auto& gemInfo = m_gemsInfo[m_selectedGem];

    for (const AZ::IO::Path& gemAbsoluteSourcePath : gemInfo.m_absoluteSourcePaths)
    {
        AZ::IO::Path gemInfoAssetFilePath = gemAbsoluteSourcePath;
        gemInfoAssetFilePath /= "Code";

        fileDialog->setDirectory(gemInfoAssetFilePath.c_str());
    }

    connect(fileDialog, &QDialog::accepted, [this, fileDialog]()
        {
            m_ui->componentPath->setText(fileDialog->directory().absolutePath());
        });

    fileDialog->open();
}

void CreateClassToolWindow::OnGemChange()
{
    m_selectedGem = m_ui->gemsList->currentText().toUtf8().data();

    auto& gemInfo = m_gemsInfo[m_selectedGem];
    for (const AZ::IO::Path& gemAbsoluteSourcePath : gemInfo.m_absoluteSourcePaths)
    {
        AZ::IO::Path gemInfoAssetFilePath = gemAbsoluteSourcePath;
        m_selectedGemPath = gemInfoAssetFilePath.c_str();
    }

}

void CreateClassToolWindow::OnBaseComponentClassSelected()
{
    QListWidgetItem* item = m_ui->componentList->currentItem();
    m_selectedBaseComponent = item->text().toUtf8().data();


}

CreateClassToolWindow::~CreateClassToolWindow()
{
}

void CreateClassToolWindow::RegisterViewClass()
{
    AzToolsFramework::ViewPaneOptions options;
    options.preferedDockingArea = Qt::LeftDockWidgetArea;
    options.showOnToolsToolbar = true;
    options.toolbarIcon = ":/Menu/asset_editor.svg";
    AzToolsFramework::RegisterViewPane<CreateClassToolWindow>(LyViewPane::CreateClassesTool, LyViewPane::CategoryTools, options);
}

void CreateClassToolWindow::InitializeComponentCreation()
{
    // Enumerate all Component base classes
    // Add into list view

    AZ::SerializeContext* serializeContext = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
    AZ_Assert(serializeContext, "Serialization context must exist");

    serializeContext->EnumerateDerived<AZ::Component>(
        [this](const AZ::SerializeContext::ClassData* componentClass, const AZ::Uuid&) -> bool
        {

            AZ::ComponentDescriptor* componentDescriptor = nullptr;
            AZ::ComponentDescriptorBus::EventResult(componentDescriptor, componentClass->m_typeId, &AZ::ComponentDescriptor::GetDescriptor);

            if (componentDescriptor)
            {
                AZ::ComponentDescriptor::DependencyArrayType providedServices;
                componentDescriptor->GetProvidedServices(providedServices, nullptr);

                QListWidgetItem* item = new QListWidgetItem(componentDescriptor->GetName(), m_ui->componentList);
                m_ui->componentList->addItem(item);
            }

            return true;
        });

}

#include <CreateClassesTool/moc_CreateClassToolWindow.cpp>

#pragma optimize("", on)

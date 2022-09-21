/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#if !defined(Q_MOC_RUN)
#include <AzCore/UserSettings/UserSettings.h>
#include <QWidget>
#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QAbstractButton>
#endif

namespace Ui
{
    class CreateClassToolWindow;
}

class ComponentClassFilterModel
    : public QSortFilterProxyModel
{
public:
    ComponentClassFilterModel(QObject* parent, int displayNameCol)
        : QSortFilterProxyModel(parent)
        , m_displayNameCol(displayNameCol)
    {}

    void FilterChanged(const QString& newFilter)
    {
        setFilterRegExp(newFilter.toLower());
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (!sourceParent.isValid() && sourceRow == -1)
        {
            return false;
        }

        
        return true;
    }

    //bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

    int m_displayNameCol = 0;
};


namespace AzFramework
{
    struct GemInfo;
}

/**
* Window pane wrapper for the Create Class Tool
*/
class CreateClassToolWindow
    : public QWidget
{
    Q_OBJECT

public:
    AZ_CLASS_ALLOCATOR(CreateClassToolWindow, AZ::SystemAllocator, 0);

    explicit CreateClassToolWindow(QWidget* parent = nullptr);
    ~CreateClassToolWindow() override;

    static void RegisterViewClass();

protected:

    void InitializeComponentCreation();
    void OnBaseComponentClassSelected();

    void OnGemChange();
    void BrowseForPath();
    void DialogButtonClicked(const QAbstractButton*);

private:
    QScopedPointer<Ui::CreateClassToolWindow> m_ui;


    QStringListModel* m_componentClassModel;
    QSortFilterProxyModel* m_proxyModel;

    AZStd::string m_selectedGem;
    AZStd::string m_selectedGemPath;
    AZStd::string m_selectedBaseComponent;
    AZStd::string m_selectedBaseComponentDescription;

    AZStd::string m_componentName;


    AZStd::unordered_map<AZStd::string, AzFramework::GemInfo> m_gemsInfo;

    QSharedPointer<ComponentClassFilterModel> m_componentClassFilterModel;
};

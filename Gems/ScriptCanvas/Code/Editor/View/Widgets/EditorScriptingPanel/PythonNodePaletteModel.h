/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/string/string.h>

#include <AzFramework/Asset/AssetSystemBus.h>

#include <AzToolsFramework/AssetBrowser/AssetBrowserFilterModel.h>

#include <GraphCanvas/Widgets/NodePalette/TreeItems/NodePaletteTreeItem.h>
#include <GraphCanvas/Widgets/GraphCanvasTreeCategorizer.h>

#include <ScriptEvents/ScriptEventsAsset.h>

#include <Editor/Nodes/NodeCreateUtils.h>

#include <Editor/View/Widgets/NodePalette/NodePaletteModelBus.h>

#include <ScriptCanvas/Asset/RuntimeAsset.h>
#include <ScriptCanvas/Core/Core.h>

#include <Editor/Include/ScriptCanvas/Bus/EditorScriptCanvasBus.h>

#include "../NodePalette/NodePaletteModel.h"

namespace ScriptCanvasEditor
{
    // Move these down into GraphCanvas for more general re-use
    struct PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonNodePaletteModelInformation, "{6741A328-186C-4173-92EF-2239AF5F6B33}");
        AZ_CLASS_ALLOCATOR(PythonNodePaletteModelInformation, AZ::SystemAllocator, 0);

        virtual ~PythonNodePaletteModelInformation() = default;

        void PopulateTreeItem(GraphCanvas::NodePaletteTreeItem& treeItem) const;

        ScriptCanvas::NodeTypeIdentifier m_nodeIdentifier{};

        AZStd::string                    m_displayName;
        AZStd::string                    m_toolTip;

        AZStd::string                    m_categoryPath;
        AZStd::string                    m_styleOverride;
        AZStd::string                    m_titlePaletteOverride;
    };

    struct PythonDataDrivenNodeModelInformation : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonDataDrivenNodeModelInformation, "{E9215DEE-5B6E-4961-8F05-4828F8A6C05D}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonDataDrivenNodeModelInformation, AZ::SystemAllocator, 0);

        Nodes::DataDrivenNodeCreationData m_nodeData;
    };

    struct PythonCategoryInformation
    {
        AZStd::string m_styleOverride;
        AZStd::string m_paletteOverride = GraphCanvas::NodePaletteTreeItem::DefaultNodeTitlePalette;

        AZStd::string m_tooltip;
    };
    class PythonNodePaletteModel : public NodePaletteModel
    {
    public:
        typedef AZStd::unordered_map< ScriptCanvas::NodeTypeIdentifier, PythonNodePaletteModelInformation* > NodePaletteRegistry;

        AZ_CLASS_ALLOCATOR(PythonNodePaletteModel, AZ::SystemAllocator, 0);

        void RepopulateModel() override;

    //    PythonNodePaletteModel();
    //    ~PythonNodePaletteModel();

    //    NodePaletteId GetNotificationId() const;

    //    void AssignAssetModel(AzToolsFramework::AssetBrowser::AssetBrowserFilterModel* assetModel);

    //    void RepopulateModel();

    //    void RegisterDataDrivenNode(PythonDataDrivenNodeModelInformation* nodePaletteItemInformation);

    //    //void RegisterCustomNode(const AZ::SerializeContext::ClassData* classData, const AZStd::string& categoryPath = "Nodes");
    //    void RegisterClassNode(const AZStd::string& categoryPath, const AZStd::string& methodClass, const AZStd::string& methodName, const AZ::BehaviorMethod* behaviorMethod, const AZ::BehaviorContext* behaviorContext, ScriptCanvas::PropertyStatus propertyStatus, bool isOverload);
    //    void RegisterGlobalMethodNode(const AZ::BehaviorContext& behaviorContext, const AZ::BehaviorMethod& behaviorMethod);
    //    void RegisterGlobalConstant(const AZ::BehaviorContext& behaviorContext, const AZ::BehaviorProperty* behaviorProperty, const AZ::BehaviorMethod& behaviorMethod);


    //    void RegisterEBusHandlerNodeModelInformation(AZStd::string_view categoryPath, AZStd::string_view busName, AZStd::string_view eventName, const ScriptCanvas::EBusBusId& busId, const AZ::BehaviorEBusHandler::BusForwarderEvent& forwardEvent);
    //    void RegisterEBusSenderNodeModelInformation(AZStd::string_view categoryPath, AZStd::string_view busName, AZStd::string_view eventName, const ScriptCanvas::EBusBusId& busId, const ScriptCanvas::EBusEventId& eventId, ScriptCanvas::PropertyStatus propertyStatus, bool isOverload);

    //    // Asset Based Registrations
    //    AZStd::vector<ScriptCanvas::NodeTypeIdentifier> RegisterScriptEvent(ScriptEvents::ScriptEventsAsset* scriptEventAsset);

    //    void RegisterDefaultCateogryInformation();
    //    void RegisterCategoryInformation(const AZStd::string& category, const PythonCategoryInformation& categoryInformation);
    //    const PythonCategoryInformation* FindCategoryInformation(const AZStd::string& categoryStyle) const;
    //    const PythonCategoryInformation* FindBestCategoryInformation(AZStd::string_view categoryView) const;

    //    const PythonNodePaletteModelInformation* FindNodePaletteInformation(const ScriptCanvas::NodeTypeIdentifier& nodeTypeIdentifier) const;

    //    const NodePaletteRegistry& GetNodeRegistry() const;

    //    // GraphCanvas::CategorizerInterface
    //    GraphCanvas::GraphCanvasTreeItem* CreateCategoryNode(AZStd::string_view categoryPath, AZStd::string_view categoryName, GraphCanvas::GraphCanvasTreeItem* treeItem) const override;
    //    ////

    //    void OnUpgradeStart() override
    //    {
    //        DisconnectLambdas();
    //    }

    //    // Asset Node Support
    //    void OnRowsInserted(const QModelIndex& parentIndex, int first, int last);
    //    void OnRowsAboutToBeRemoved(const QModelIndex& parentIndex, int first, int last);

    //    void TraverseTree(QModelIndex index = QModelIndex());
    //    ////

    //private:

    //    AZStd::vector<ScriptCanvas::NodeTypeIdentifier> ProcessAsset(AzToolsFramework::AssetBrowser::AssetBrowserEntry* entry);
    //    void RemoveAsset(const AZ::Data::AssetId& assetId);

    //    void ClearRegistry();

    //    void ConnectLambdas();
    //    void DisconnectLambdas();

    //    AzToolsFramework::AssetBrowser::AssetBrowserFilterModel* m_assetModel = nullptr;
    //    AZStd::vector< QMetaObject::Connection > m_lambdaConnections;

    //    AZStd::unordered_map< AZStd::string, PythonCategoryInformation > m_categoryInformation;
    //    NodePaletteRegistry m_registeredNodes;

    //    AZStd::unordered_multimap<AZ::Data::AssetId, ScriptCanvas::NodeTypeIdentifier> m_assetMapping;

    //    NodePaletteId m_paletteId;

    //    AZStd::recursive_mutex m_mutex;
    };

    // Concrete Sub Classes with whatever extra data is required [ScriptCanvas Only]
    struct PythonCustomNodeModelInformation
        : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonCustomNodeModelInformation, "{B138A8F3-88C9-440E-B658-64CC94206B80}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonCustomNodeModelInformation, AZ::SystemAllocator, 0);

        AZ::Uuid m_typeId = AZ::Uuid::CreateNull();
    };

    struct PythonMethodNodeModelInformation
        : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonMethodNodeModelInformation, "{8BDB6787-4E0F-47AC-9FFD-F8C3C2FA6846}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonMethodNodeModelInformation, AZ::SystemAllocator, 0);

        bool m_isOverload{};
        AZStd::string m_classMethod;
        AZStd::string m_methodName;
        ScriptCanvas::PropertyStatus m_propertyStatus = ScriptCanvas::PropertyStatus::None;
    };

    struct PythonGlobalMethodNodeModelInformation
        : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonGlobalMethodNodeModelInformation, "{B6FA37D8-7FD2-4FFB-8309-8A0520A164BB}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonGlobalMethodNodeModelInformation, AZ::SystemAllocator, 0);

        AZStd::string m_methodName;
        bool m_isProperty;
    };

    struct PythonEBusHandlerNodeModelInformation
        : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonEBusHandlerNodeModelInformation, "{0C505812-9E2A-4398-8EA5-C590D381CDFB}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonEBusHandlerNodeModelInformation, AZ::SystemAllocator, 0);

        AZStd::string m_busName;
        AZStd::string m_eventName;
        bool m_isOverload{};

        ScriptCanvas::EBusBusId m_busId;
        ScriptCanvas::EBusEventId m_eventId;
    };

    struct PythonEBusSenderNodeModelInformation
        : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonEBusSenderNodeModelInformation, "{5C928640-155C-4F0C-82EC-20A065426C02}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonEBusSenderNodeModelInformation, AZ::SystemAllocator, 0);

        bool m_isOverload{};

        AZStd::string m_busName;
        AZStd::string m_eventName;

        ScriptCanvas::EBusBusId m_busId;
        ScriptCanvas::EBusEventId m_eventId;
        ScriptCanvas::PropertyStatus m_propertyStatus = ScriptCanvas::PropertyStatus::None;
    };

    struct PythonScriptEventHandlerNodeModelInformation
        : public PythonEBusHandlerNodeModelInformation
    {
        AZ_RTTI(PythonScriptEventHandlerNodeModelInformation, "{C48F3E96-2E38-4EC6-9C13-E6BAFFE3345A}", PythonEBusHandlerNodeModelInformation);
        AZ_CLASS_ALLOCATOR(PythonScriptEventHandlerNodeModelInformation, AZ::SystemAllocator, 0);
    };

    struct PythonScriptEventSenderNodeModelInformation
        : public PythonEBusSenderNodeModelInformation
    {
        AZ_RTTI(PythonScriptEventSenderNodeModelInformation, "{BF53933A-9872-4B7D-BA58-69F99FB631F8}", PythonEBusSenderNodeModelInformation);
        AZ_CLASS_ALLOCATOR(PythonScriptEventSenderNodeModelInformation, AZ::SystemAllocator, 0);
    };

    //! FunctionNodeModelInformation refers to function graph assets, not methods
    struct PythonFunctionNodeModelInformation
        : public PythonNodePaletteModelInformation
    {
        AZ_RTTI(PythonFunctionNodeModelInformation, "{F734E8DB-EBE3-4191-89D5-EE8CF4EBA342}", PythonNodePaletteModelInformation);
        AZ_CLASS_ALLOCATOR(PythonFunctionNodeModelInformation, AZ::SystemAllocator, 0);

        AZ::Color         m_functionColor;
        AZ::Data::AssetId m_functionAssetId;
    };

}

/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContextUtilities.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/AssetBrowser/Entries/ProductAssetBrowserEntry.h>

#include <Editor/View/Widgets/EditorScriptingPanel/PythonNodePaletteModel.h>

#include <Editor/Include/ScriptCanvas/Bus/RequestBus.h>
#include <Editor/GraphCanvas/GraphCanvasEditorNotificationBusId.h>
#include <Editor/Nodes/NodeUtils.h>
#include <Editor/Settings.h>
#include <Editor/Translation/TranslationHelper.h>

#include <ScriptCanvas/Data/DataRegistry.h>
#include <ScriptCanvas/Libraries/Libraries.h>
#include <ScriptCanvas/Libraries/Core/GetVariable.h>
#include <ScriptCanvas/Libraries/Core/Method.h>
#include <ScriptCanvas/Libraries/Core/SetVariable.h>
#include <ScriptCanvas/Utils/NodeUtils.h>

#include <ScriptCanvas/Data/Traits.h>
#include <StaticLib/GraphCanvas/Styling/definitions.h>

AZ_DEFINE_BUDGET(PythonNodePaletteModel);

namespace
{
    static constexpr char DefaultGlobalConstantCategory[] = "Global Constants";
    static constexpr char DefaultGlobalMethodCategory[] = "Global Methods";
    static constexpr char DefaultClassMethodCategory[] = "Class Methods";
    static constexpr char DefaultEbusHandlerCategory[] = "Event Handlers";
    static constexpr char DefaultEbusEventCategory[] = "Events";

    // Various Helper Methods
    bool IsDeprecated(const AZ::AttributeArray& attributes)
    {
        bool isDeprecated{};

        if (auto isDeprecatedAttributePtr = AZ::FindAttribute(AZ::Script::Attributes::Deprecated, attributes))
        {
            AZ::AttributeReader(nullptr, isDeprecatedAttributePtr).Read<bool>(isDeprecated);
        }

        return isDeprecated;
    }

    bool ShouldExcludeFromNodeList(const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>* excludeAttributeData, const AZ::Uuid& typeId)
    {
        if (excludeAttributeData)
        {
            AZ::u64 exclusionFlags = AZ::Script::Attributes::ExcludeFlags::List | AZ::Script::Attributes::ExcludeFlags::ListOnly;

            if (typeId == AzToolsFramework::Components::EditorComponentBase::TYPEINFO_Uuid())
            {
                return true;
            }

            return (static_cast<AZ::u64>(excludeAttributeData->Get(nullptr)) & exclusionFlags) != 0; // warning C4800: 'AZ::u64': forcing value to bool 'true' or 'false' (performance warning)
        }

        return false;
    }

    bool HasExcludeFromNodeListAttribute(const AZ::SerializeContext* serializeContext, const AZ::Uuid& typeId)
    {
        const AZ::SerializeContext::ClassData* classData = serializeContext->FindClassData(typeId);
        if (classData && classData->m_editData)
        {
            if (auto editorElementData = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
            {
                if (auto excludeAttribute = editorElementData->FindAttribute(AZ::Script::Attributes::ExcludeFrom))
                {
                    auto excludeAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(excludeAttribute);
                    return excludeAttributeData && ShouldExcludeFromNodeList(excludeAttributeData, typeId);
                }
            }
        }

        return false;
    }

    // Checks for and returns the Category attribute from an AZ::AttributeArray
    AZStd::string GetCategoryPath(const AZ::AttributeArray& attributes, const AZ::BehaviorContext& behaviorContext)
    {
        AZStd::string retVal;
        AZ::Attribute* categoryAttribute = AZ::FindAttribute(AZ::Script::Attributes::Category, attributes);

        if (categoryAttribute)
        {
            AZ::AttributeReader(nullptr, categoryAttribute).Read<AZStd::string>(retVal, behaviorContext);
        }

        return retVal;
    }

    bool IsExplicitOverload(const AZ::BehaviorMethod& method)
    {
        return AZ::FindAttribute(AZ::ScriptCanvasAttributes::ExplicitOverloadCrc, method.m_attributes) != nullptr;
    }

    void RegisterMethod
        ( ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel
        , const AZ::BehaviorContext& behaviorContext
        , const AZStd::string& categoryPath
        , const AZ::BehaviorClass* behaviorClass
        , const AZStd::string& name
        , const AZ::BehaviorMethod& method
        , ScriptCanvas::PropertyStatus propertyStatus
        , bool isOverloaded)
    {

        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "RegisterMethod");

        if (IsDeprecated(method.m_attributes))
        {
            return;
        }

        if (behaviorClass && !isOverloaded)
        {
            auto excludeMethodAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, method.m_attributes));
            if (ShouldExcludeFromNodeList(excludeMethodAttributeData, behaviorClass->m_azRtti ? behaviorClass->m_azRtti->GetTypeId() : behaviorClass->m_typeId))
            {
                return;
            }
        }

        if (!AZ::Internal::IsInScope(behaviorClass->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
        {
            return;
        }

        const auto isExposableOutcome = ScriptCanvas::IsExposable(method);
        if (!isExposableOutcome.IsSuccess())
        {
            AZ_Warning("ScriptCanvas", false, "Unable to expose method: %s to ScriptCanvas because: %s", method.m_name.data(), isExposableOutcome.GetError().data());
            return;
        }

        nodePaletteModel.RegisterClassNode(categoryPath, behaviorClass ? behaviorClass->m_name : "", name, &method, &behaviorContext, propertyStatus, isOverloaded);
    }

    void RegisterGlobalMethod(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel, const AZ::BehaviorContext& behaviorContext,
        const AZ::BehaviorMethod& behaviorMethod)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "RegisterGlobalMethod");

        const auto isExposableOutcome = ScriptCanvas::IsExposable(behaviorMethod);
        if (!isExposableOutcome.IsSuccess())
        {
            AZ_Warning("ScriptCanvas", false, "Unable to expose method: %s to ScriptCanvas because: %s",
                behaviorMethod.m_name.c_str(), isExposableOutcome.GetError().data());
            return;
        }

        if (!AZ::Internal::IsInScope(behaviorMethod.m_attributes, AZ::Script::Attributes::ScopeFlags::Common))
        {
            return; // skip this method
        }

        nodePaletteModel.RegisterGlobalMethodNode(behaviorContext, behaviorMethod);
    }

    //! Retrieve the list of EBuses t hat should not be exposed in the ScriptCanvasEditor Node Palette
    AZStd::unordered_set<AZ::Crc32> GetEBusExcludeSet(const AZ::BehaviorContext& behaviorContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "GetEBusExcludeSet");

        // We will skip buses that are ONLY registered on classes that derive from EditorComponentBase,
        // because they don't have a runtime implementation. Buses such as the TransformComponent which
        // is implemented by both an EditorComponentBase derived class and a Component derived class
        // will still appear
        AZStd::unordered_set<AZ::Crc32> skipBuses;
        AZStd::unordered_set<AZ::Crc32> potentialSkipBuses;
        AZStd::unordered_set<AZ::Crc32> nonSkipBuses;
        for (const auto& classIter : behaviorContext.m_classes)
        {
            const AZ::BehaviorClass* behaviorClass = classIter.second;

            if (IsDeprecated(behaviorClass->m_attributes))
            {
                continue;
            }

            // Only bind Behavior Classes marked with the Scope type of Launcher
            if (!AZ::Internal::IsInScope(behaviorClass->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
            {
                continue; // skip this class
            }

            // Check for "ExcludeFrom" attribute for ScriptCanvas
            auto excludeClassAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(
                AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorClass->m_attributes));

            // We don't want to show any components, since there isn't anything we can do with them
            // from ScriptCanvas since we use buses to communicate to everything.
            if (ShouldExcludeFromNodeList(excludeClassAttributeData, behaviorClass->m_azRtti ? behaviorClass->m_azRtti->GetTypeId() : behaviorClass->m_typeId))
            {
                for (const auto& requestBus : behaviorClass->m_requestBuses)
                {
                    skipBuses.insert(AZ::Crc32(requestBus.c_str()));
                }

                continue;
            }

            auto baseClass = AZStd::find(behaviorClass->m_baseClasses.begin(),
                behaviorClass->m_baseClasses.end(),
                AzToolsFramework::Components::EditorComponentBase::TYPEINFO_Uuid());

            if (baseClass != behaviorClass->m_baseClasses.end())
            {
                for (const auto& requestBus : behaviorClass->m_requestBuses)
                {
                    potentialSkipBuses.insert(AZ::Crc32(requestBus.c_str()));
                }
            }
            // If the Ebus does not inherit from EditorComponentBase then do not skip it
            else
            {
                for (const auto& requestBus : behaviorClass->m_requestBuses)
                {
                    nonSkipBuses.insert(AZ::Crc32(requestBus.c_str()));
                }
            }
        }

        // Add buses which are not on the non-skip list to the skipBuses set
        for (auto potentialSkipBus : potentialSkipBuses)
        {
            if (nonSkipBuses.find(potentialSkipBus) == nonSkipBuses.end())
            {
                skipBuses.insert(potentialSkipBus);
            }
        }

        return skipBuses;
    }

    //! Register all nodes populated into the ScriptCanvas NodeRegistry
    void PopulateScriptCanvasDerivedNodes(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::SerializeContext& serializeContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateScriptCanvasDerivedNodes");

        nodePaletteModel.RegisterDefaultCateogryInformation();

        ScriptCanvas::NodeRegistry* registry = ScriptCanvas::NodeRegistry::GetInstance();
        for (auto nodeId : registry->m_nodes)
        {
            if (HasExcludeFromNodeListAttribute(&serializeContext, nodeId))
            {
                continue;
            }


            // Pass in the associated class data so we can do more intensive lookups?
            const AZ::SerializeContext::ClassData* nodeClassData = serializeContext.FindClassData(nodeId);

            if (nodeClassData == nullptr)
            {
                continue;
            }

            // Skip over some of our more dynamic nodes that we want to populate using different means
            else if (nodeClassData->m_azRtti && nodeClassData->m_azRtti->IsTypeOf<ScriptCanvas::Nodes::Core::GetVariableNode>())
            {
                continue;
            }
            else if (nodeClassData->m_azRtti && nodeClassData->m_azRtti->IsTypeOf<ScriptCanvas::Nodes::Core::SetVariableNode>())
            {
                continue;
            }
            else
            {
                nodePaletteModel.RegisterCustomNode(nodeClassData);
            }
        }
    }

    void PopulateScriptCanvasDerivedNodesDeprecated(
        ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel, const AZ::SerializeContext& serializeContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateScriptCanvasDerivedNodes");

        // Get all the types.
        auto EnumerateLibraryDefintionNodes = [&nodePaletteModel,
                                               &serializeContext](const AZ::SerializeContext::ClassData* classData, const AZ::Uuid&) -> bool
        {
            ScriptCanvasEditor::CategoryInformation categoryInfo;

            AZStd::string categoryPath = classData->m_editData ? classData->m_editData->m_name : classData->m_name;

            if (classData->m_editData)
            {
                auto editorElementData = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData);
                if (editorElementData)
                {
                    if (auto categoryAttribute = editorElementData->FindAttribute(AZ::Edit::Attributes::Category))
                    {
                        if (auto categoryAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(categoryAttribute))
                        {
                            categoryPath = categoryAttributeData->Get(nullptr);
                        }
                    }

                    if (auto categoryStyleAttribute = editorElementData->FindAttribute(AZ::Edit::Attributes::CategoryStyle))
                    {
                        if (auto categoryAttributeData =
                                azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(categoryStyleAttribute))
                        {
                            categoryInfo.m_styleOverride = categoryAttributeData->Get(nullptr);
                        }
                    }

                    if (auto titlePaletteAttribute = editorElementData->FindAttribute(ScriptCanvas::Attributes::Node::TitlePaletteOverride))
                    {
                        if (auto categoryAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(titlePaletteAttribute))
                        {
                            categoryInfo.m_paletteOverride = categoryAttributeData->Get(nullptr);
                        }
                    }
                }
            }

            nodePaletteModel.RegisterCategoryInformation(categoryPath, categoryInfo);

            // Children
            for (auto& node : ScriptCanvas::Library::LibraryDefinition::GetNodes(classData->m_typeId))
            {
                if (HasExcludeFromNodeListAttribute(&serializeContext, node.first))
                {
                    continue;
                }

                // Pass in the associated class data so we can do more intensive lookups?
                const AZ::SerializeContext::ClassData* nodeClassData = serializeContext.FindClassData(node.first);

                if (nodeClassData == nullptr)
                {
                    continue;
                }

                // Skip over some of our more dynamic nodes that we want to populate using different means
                else if (nodeClassData->m_azRtti && nodeClassData->m_azRtti->IsTypeOf<ScriptCanvas::Nodes::Core::GetVariableNode>())
                {
                    continue;
                }
                else if (nodeClassData->m_azRtti && nodeClassData->m_azRtti->IsTypeOf<ScriptCanvas::Nodes::Core::SetVariableNode>())
                {
                    continue;
                }
                else
                {
                    nodePaletteModel.RegisterCustomNode(nodeClassData, categoryPath);
                }
            }

            return true;
        };

        const AZ::TypeId& libraryDefTypeId = azrtti_typeid<ScriptCanvas::Library::LibraryDefinition>();
        serializeContext.EnumerateDerived(EnumerateLibraryDefintionNodes, libraryDefTypeId, libraryDefTypeId);
    }

    void PopulateVariablePalette()
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateVariablePalette");

        auto dataRegistry = ScriptCanvas::GetDataRegistry();

        for (auto& type : dataRegistry->m_creatableTypes)
        {
            if (!type.second.m_isTransient)
            {
                ScriptCanvasEditor::VariablePaletteRequestBus::Broadcast(&ScriptCanvasEditor::VariablePaletteRequests::RegisterVariableType, type.first);
            }
        }
    }

    void PopulateBehaviorContextGlobalMethods(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextGlobalMethods");

        // BehaviorMethods are not associated with a class
        // therefore the Uuid is set to Null
        const AZ::Uuid behaviorMethodUuid = AZ::Uuid::CreateNull();
        for (const auto& [methodName, behaviorMethod] : behaviorContext.m_methods)
        {
            // Skip behavior methods that are deprecated
            if (behaviorMethod == nullptr || IsDeprecated(behaviorMethod->m_attributes))
            {
                continue;
            }

            // Check for "ExcludeFrom" attribute for ScriptCanvas
            auto excludeMethodAttributeData = azrtti_cast<const AZ::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(
                AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorMethod->m_attributes));


            if (ShouldExcludeFromNodeList(excludeMethodAttributeData, behaviorMethodUuid))
            {
                continue;
            }

            if (!AZ::Internal::IsInScope(behaviorMethod->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
            {
                continue;
            }

            RegisterGlobalMethod(nodePaletteModel, behaviorContext, *behaviorMethod);
        }
    }

    //! Iterates over all Properties directly reflected to the BehaviorContext instance
    //! and registers there Getter/Setter methods to the PythonNodePaletteModel
    void PopulateBehaviorContextGlobalProperties(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextGlobalProperties");

        const AZ::Uuid behaviorMethodUuid = AZ::Uuid::CreateNull();
        for (const auto& [propertyName, behaviorProperty] : behaviorContext.m_properties)
        {
            // Skip behavior properties that are deprecated
            if (behaviorProperty == nullptr || IsDeprecated(behaviorProperty->m_attributes))
            {
                continue;
            }

            // Check for "ExcludeFrom" attribute for ScriptCanvas
            auto excludePropertyAttributeData = azrtti_cast<const AZ::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(
                AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorProperty->m_attributes));


            if (ShouldExcludeFromNodeList(excludePropertyAttributeData, behaviorMethodUuid))
            {
                continue;
            }

            if (!AZ::Internal::IsInScope(behaviorProperty->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
            {
                continue;
            }

            if (behaviorProperty->m_getter && !behaviorProperty->m_setter)
            {
                nodePaletteModel.RegisterGlobalConstant(behaviorContext, behaviorProperty , *behaviorProperty->m_getter);
            }
            else
            {
                if (behaviorProperty->m_getter)
                {
                    RegisterGlobalMethod(nodePaletteModel, behaviorContext, *behaviorProperty->m_getter);
                }

                if (behaviorProperty->m_setter)
                {
                    RegisterGlobalMethod(nodePaletteModel, behaviorContext, *behaviorProperty->m_setter);
                }
            }

        }
    }

    void PopulateBehaviorContextClassMethods(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextClassMethods");

        AZ::SerializeContext* serializeContext{};
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);

        for (const auto& classIter : behaviorContext.m_classes)
        {
            const AZ::BehaviorClass* behaviorClass = classIter.second;

            if (IsDeprecated(behaviorClass->m_attributes))
            {
                continue;
            }

            if (auto excludeFromPointer = AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorClass->m_attributes))
            {
                AZ::Script::Attributes::ExcludeFlags excludeFlags{};
                AZ::AttributeReader(nullptr, excludeFromPointer).Read<AZ::Script::Attributes::ExcludeFlags>(excludeFlags);

                if ((excludeFlags & (AZ::Script::Attributes::ExcludeFlags::List | AZ::Script::Attributes::ExcludeFlags::ListOnly)) != 0)
                {
                    continue;
                }
            }

            if (!AZ::Internal::IsInScope(behaviorClass->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
            {
                continue;
            }

            // Objects and Object methods
            {
                AZStd::string categoryPath;

                GraphCanvas::TranslationKey key;
                key << ScriptCanvasEditor::TranslationHelper::AssetContext::BehaviorClassContext << behaviorClass->m_name.c_str() << "details";

                GraphCanvas::TranslationRequests::Details details;
                GraphCanvas::TranslationRequestBus::BroadcastResult(details, &GraphCanvas::TranslationRequests::GetDetails, key, details);

                categoryPath = details.m_category;

                if (categoryPath.empty())
                {
                    categoryPath = GetCategoryPath(behaviorClass->m_attributes, behaviorContext);
                }

                auto dataRegistry = ScriptCanvas::GetDataRegistry();
                ScriptCanvas::Data::Type type = dataRegistry->m_typeIdTraitMap[ScriptCanvas::Data::eType::BehaviorContextObject].m_dataTraits.GetSCType(behaviorClass->m_typeId);

                if (type.IsValid())
                {
                    if (dataRegistry->m_creatableTypes.contains(type))
                    {
                        ScriptCanvasEditor::VariablePaletteRequestBus::Broadcast(&ScriptCanvasEditor::VariablePaletteRequests::RegisterVariableType, type);
                    }
                }

                AZStd::string classNamePretty(classIter.first);

                AZ::Attribute* prettyNameAttribute = AZ::FindAttribute(AZ::ScriptCanvasAttributes::PrettyName, behaviorClass->m_attributes);

                if (prettyNameAttribute)
                {
                    AZ::AttributeReader(nullptr, prettyNameAttribute).Read<AZStd::string>(classNamePretty, behaviorContext);
                }

                if (categoryPath.empty())
                {
                    if (classNamePretty.empty())
                    {
                        categoryPath = classNamePretty;
                    }
                    else
                    {
                        categoryPath = DefaultClassMethodCategory;
                    }
                }

                categoryPath.append("/");

                if (details.m_name.empty())
                {
                    categoryPath.append(classNamePretty.c_str());
                }
                else
                {
                    categoryPath.append(details.m_name.c_str());
                }

                for (auto property : behaviorClass->m_properties)
                {
                    if (!AZ::Internal::IsInScope(property.second->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
                    {
                        continue;
                    }

                    if (property.second->m_getter)
                    {
                        RegisterMethod(nodePaletteModel, behaviorContext, categoryPath, behaviorClass, property.first, *property.second->m_getter, ScriptCanvas::PropertyStatus::Getter, behaviorClass->IsMethodOverloaded(property.first));
                    }

                    if (property.second->m_setter)
                    {
                        RegisterMethod(nodePaletteModel, behaviorContext, categoryPath, behaviorClass, property.first, *property.second->m_setter, ScriptCanvas::PropertyStatus::Setter, behaviorClass->IsMethodOverloaded(property.first));
                    }
                }

                for (auto methodIter : behaviorClass->m_methods)
                {
                    if (!AZ::Internal::IsInScope(methodIter.second->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
                    {
                        continue;
                    }

                    if (!IsExplicitOverload(*methodIter.second))
                    {
                        // Respect the exclusion flags
                        auto attributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, methodIter.second->m_attributes));
                        if (ShouldExcludeFromNodeList(attributeData , {}))
                        {
                            continue;
                        }

                        RegisterMethod(nodePaletteModel, behaviorContext, categoryPath, behaviorClass, methodIter.first, *methodIter.second, ScriptCanvas::PropertyStatus::None, behaviorClass->IsMethodOverloaded(methodIter.first));
                    }
                }
            }
        }
    }

    void PopulateBehaviorContextOverloadedMethods(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextOverloadedMethods");


        for (const AZ::ExplicitOverloadInfo& explicitOverload : behaviorContext.m_explicitOverloads)
        {
            RegisterMethod(nodePaletteModel, behaviorContext, explicitOverload.m_categoryPath, nullptr, explicitOverload.m_name, *explicitOverload.m_overloads.begin()->first, ScriptCanvas::PropertyStatus::None, true);
        }
    }

    void PopulateBehaviorContextEBusHandler(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext, const AZ::BehaviorEBus& behaviorEbus)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextEBusHandler");

        if (AZ::ScopedBehaviorEBusHandler handler{ behaviorEbus }; handler)
        {
            auto excludeEbusAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(
                AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorEbus.m_attributes));
            if (ShouldExcludeFromNodeList(excludeEbusAttributeData, handler->RTTI_GetType()))
            {
                return;
            }

            // Only bind Behavior Buses marked with the Scope type of Common
            if (!AZ::Internal::IsInScope(behaviorEbus.m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
            {
                return; // skip this bus
            }

            const AZ::BehaviorEBusHandler::EventArray& events(handler->GetEvents());
            if (!events.empty())
            {

                GraphCanvas::TranslationKey key;
                key << ScriptCanvasEditor::TranslationHelper::AssetContext::EBusHandlerContext << behaviorEbus.m_name.c_str() << "details";

                GraphCanvas::TranslationRequests::Details details;
                GraphCanvas::TranslationRequestBus::BroadcastResult(details, &GraphCanvas::TranslationRequests::GetDetails, key, details);

                AZStd::string categoryPath = details.m_category.empty() ? GetCategoryPath(behaviorEbus.m_attributes, behaviorContext) : details.m_category;

                // Treat the EBusHandler name as a Category key in order to allow multiple buses to be merged into a single Category.
                {
                    if (!categoryPath.empty())
                    {
                        categoryPath.append("/");
                    }
                    else
                    {
                        categoryPath = AZStd::string::format("%s/", DefaultEbusHandlerCategory);
                    }

                    if (!details.m_name.empty())
                    {
                        categoryPath.append(details.m_name.c_str());
                    }
                    else if (categoryPath.contains(DefaultEbusHandlerCategory))
                    {
                        // Use the BehaviorEBus name to categorize within the default ebus handler category
                        categoryPath.append(behaviorEbus.m_name.c_str());
                    }
                }

                for (const auto& event : events)
                {
                    nodePaletteModel.RegisterEBusHandlerNodeModelInformation(categoryPath.c_str(), behaviorEbus.m_name, event.m_name, ScriptCanvas::EBusBusId(behaviorEbus.m_name), event);
                }
            }
        }
    }

    void PopulateBehaviorContextEBusEventMethods(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext, const AZ::BehaviorEBus& behaviorEbus)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextEBusEventMethods");

        if (!behaviorEbus.m_events.empty())
        {
            GraphCanvas::TranslationKey key;
            key << ScriptCanvasEditor::TranslationHelper::AssetContext::EBusSenderContext << behaviorEbus.m_name.c_str() << "details";

            GraphCanvas::TranslationRequests::Details details;
            GraphCanvas::TranslationRequestBus::BroadcastResult(details, &GraphCanvas::TranslationRequests::GetDetails, key, details);

            AZStd::string categoryPath = details.m_category.empty() ? GetCategoryPath(behaviorEbus.m_attributes, behaviorContext) : details.m_category;

            // Parent
            AZStd::string displayName = details.m_name;

            // Treat the EBus name as a Category key in order to allow multiple buses to be merged into a single Category.
            if (!categoryPath.empty())
            {
                categoryPath.append("/");
            }
            else
            {
                categoryPath = AZStd::string::format("%s/", DefaultEbusEventCategory);
            }

            if (!details.m_name.empty())
            {
                categoryPath.append(details.m_name.c_str());
            }
            else if (categoryPath.contains(DefaultEbusEventCategory))
            {
                // Use the behavior EBus name to categorize within the ebus event category
                categoryPath.append(behaviorEbus.m_name.c_str());
            }

            ScriptCanvasEditor::CategoryInformation ebusCategoryInformation;

            ebusCategoryInformation.m_tooltip = details.m_tooltip;

            nodePaletteModel.RegisterCategoryInformation(categoryPath, ebusCategoryInformation);

            for (auto event : behaviorEbus.m_events)
            {
                if (IsDeprecated(event.second.m_attributes))
                {
                    continue;
                }

                // Only bind Behavior Buses marked with the Scope type of Common
                if (!AZ::Internal::IsInScope(event.second.m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
                {
                    continue; // skip this bus
                }

                auto excludeEventAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, event.second.m_attributes));
                if (ShouldExcludeFromNodeList(excludeEventAttributeData, AZ::Uuid::CreateNull()))
                {
                    continue; // skip this event
                }

                const bool isOverload{ false }; // overloaded events are not trivially supported
                nodePaletteModel.RegisterEBusSenderNodeModelInformation(categoryPath, behaviorEbus.m_name, event.first, ScriptCanvas::EBusBusId(behaviorEbus.m_name.c_str()), ScriptCanvas::EBusEventId(event.first.c_str()), ScriptCanvas::PropertyStatus::None, isOverload);
            }
        }
    }

    void PopulateBehaviorContextEBuses(ScriptCanvasEditor::PythonNodePaletteModel& nodePaletteModel,
        const AZ::BehaviorContext& behaviorContext)
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "PopulateBehaviorContextEBuses");
        AZStd::unordered_set<AZ::Crc32> skipBuses = GetEBusExcludeSet(behaviorContext);

        for (const auto& [ebusName, behaviorEbus] : behaviorContext.m_ebuses)
        {
            if (behaviorEbus == nullptr)
            {
                continue;
            }

            auto skipBusIterator = skipBuses.find(AZ::Crc32(ebusName));
            if (skipBusIterator != skipBuses.end())
            {
                continue;
            }

            // Skip buses mapped by their deprecated name (usually duplicates)
            if (ebusName == behaviorEbus->m_deprecatedName)
            {
                continue;
            }

            // Only bind Behavior Buses marked with the Scope type of Common
            if (!AZ::Internal::IsInScope(behaviorEbus->m_attributes, AZ::Script::Attributes::ScopeFlags::Automation))
            {
                continue; // skip this bus
            }

            if (IsDeprecated(behaviorEbus->m_attributes))
            {
                continue;
            }

            auto excludeEbusAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(
                AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorEbus->m_attributes));
            if (ShouldExcludeFromNodeList(excludeEbusAttributeData, AZ::Uuid::CreateNull()))
            {
                continue;
            }

            if (auto runtimeEbusAttributePtr = AZ::FindAttribute(AZ::RuntimeEBusAttribute, behaviorEbus->m_attributes))
            {
                bool isRuntimeEbus = false;
                AZ::AttributeReader(nullptr, runtimeEbusAttributePtr).Read<bool>(isRuntimeEbus);

                if (isRuntimeEbus)
                {
                    continue;
                }
            }

            // EBus Handler
            PopulateBehaviorContextEBusHandler(nodePaletteModel, behaviorContext, *behaviorEbus);

            // EBus Sender
            PopulateBehaviorContextEBusEventMethods(nodePaletteModel, behaviorContext, *behaviorEbus);
        }
    }

}

namespace ScriptCanvasEditor
{

    ////////////////////////////////
    // NodePaletteModelInformation
    ////////////////////////////////
    void PythonNodePaletteModelInformation::PopulateTreeItem(GraphCanvas::NodePaletteTreeItem& treeItem) const
    {
        if (!m_toolTip.empty())
        {
            treeItem.SetToolTip(m_toolTip.c_str());
        }

        if (!m_styleOverride.empty())
        {
            treeItem.SetStyleOverride(m_styleOverride.c_str());
        }

        if (!m_titlePaletteOverride.empty())
        {
            const bool forceSet = true;
            treeItem.SetTitlePalette(m_titlePaletteOverride.c_str(), forceSet);
        }
    }

    void PythonNodePaletteModel::RepopulateModel()
    {
        AZ_PROFILE_SCOPE(PythonNodePaletteModel, "RepopulateModel");

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);

        AZ::BehaviorContext* behaviorContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);

        AZ_Assert(serializeContext, "Could not find SerializeContext. Aborting Palette Creation.");
        AZ_Assert(behaviorContext, "Could not find BehaviorContext. Aborting Palette Creation.");

        if (serializeContext == nullptr || behaviorContext == nullptr)
        {
            return;
        }


        for (auto & mapPair : m_registeredNodes)
        {
            delete mapPair.second;
        }

        m_registeredNodes.clear();

        m_categoryInformation.clear();

        // Populates the NodePalette with Behavior Class method nodes 
        PopulateBehaviorContextClassMethods(*this, *behaviorContext);

        PopulateBehaviorContextEBuses(*this, *behaviorContext);

        PopulateBehaviorContextGlobalMethods(*this, *behaviorContext);

        PopulateScriptCanvasDerivedNodes(*this, *serializeContext);
    }


}

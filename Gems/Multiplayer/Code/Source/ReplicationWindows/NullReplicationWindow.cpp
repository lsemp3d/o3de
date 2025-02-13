/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Source/ReplicationWindows/NullReplicationWindow.h>
#include <Source/AutoGen/Multiplayer.AutoPackets.h>

namespace Multiplayer
{
    NullReplicationWindow::NullReplicationWindow(AzNetworking::IConnection* connection)
        : m_connection(connection)
    {
        ;
    }

    bool NullReplicationWindow::ReplicationSetUpdateReady()
    {
        return true;
    }

    const ReplicationSet& NullReplicationWindow::GetReplicationSet() const
    {
        return m_emptySet;
    }

    uint32_t NullReplicationWindow::GetMaxProxyEntityReplicatorSendCount() const
    {
        return 0;
    }

    bool NullReplicationWindow::IsInWindow([[maybe_unused]] const ConstNetworkEntityHandle& entityHandle, NetEntityRole& outNetworkRole) const
    {
        outNetworkRole = NetEntityRole::InvalidRole;
        return false;
    }

    bool NullReplicationWindow::AddEntity([[maybe_unused]] AZ::Entity* entity)
    {
        return false;
    }

    void NullReplicationWindow::RemoveEntity([[maybe_unused]] AZ::Entity* entity)
    {
        ;
    }

    void NullReplicationWindow::UpdateWindow()
    {
        ;
    }

    AzNetworking::PacketId NullReplicationWindow::SendEntityUpdateMessages(NetworkEntityUpdateVector& entityUpdateVector)
    {
        MultiplayerPackets::EntityUpdates entityUpdatePacket;
        entityUpdatePacket.SetHostTimeMs(GetNetworkTime()->GetHostTimeMs());
        entityUpdatePacket.SetHostFrameId(GetNetworkTime()->GetHostFrameId());
        entityUpdatePacket.SetEntityMessages(entityUpdateVector);
        return m_connection->SendUnreliablePacket(entityUpdatePacket);
    }

    void NullReplicationWindow::SendEntityRpcs(NetworkEntityRpcVector& entityRpcVector, bool reliable)
    {
        MultiplayerPackets::EntityRpcs entityRpcsPacket;
        entityRpcsPacket.SetEntityRpcs(entityRpcVector);
        if (reliable)
        {
            m_connection->SendReliablePacket(entityRpcsPacket);
        }
        else
        {
            m_connection->SendUnreliablePacket(entityRpcsPacket);
        }
    }

    void NullReplicationWindow::SendEntityResets(const NetEntityIdSet& resetIds)
    {
        MultiplayerPackets::RequestReplicatorReset entityResetPacket;
        for (NetEntityId entityId : resetIds)
        {
            if (entityResetPacket.GetEntityIds().full())
            {
                m_connection->SendUnreliablePacket(entityResetPacket);
                entityResetPacket.ModifyEntityIds().clear();
            }
            entityResetPacket.ModifyEntityIds().push_back(entityId);
        }

        if (!entityResetPacket.GetEntityIds().empty())
        {
            m_connection->SendUnreliablePacket(entityResetPacket);
        }
    }

    void NullReplicationWindow::DebugDraw() const
    {
        // Nothing to draw
    }
}

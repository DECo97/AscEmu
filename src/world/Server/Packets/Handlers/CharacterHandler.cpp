/*
Copyright (c) 2014-2018 AscEmu Team <http://www.ascemu.org>
This file is released under the MIT license. See README-MIT for more information.
*/

#include "StdAfx.h"
#include "Server/WorldSession.h"
#include "Server/Packets/CmsgSetFactionAtWar.h"
#include "Server/Packets/CmsgSetFactionInactive.h"
#include "Units/Players/Player.h"
#include "Server/Packets/CmsgCharDelete.h"
#include "Server/Packets/SmsgCharDelete.h"

using namespace AscEmu::Packets;

void WorldSession::handleSetFactionAtWarOpcode(WorldPacket& recvPacket)
{
    CmsgSetFactionAtWar recv_packet;
    if (!recv_packet.deserialise(recvPacket))
        return;

    GetPlayer()->SetAtWar(recv_packet.id, recv_packet.state == 1);
}

void WorldSession::handleSetFactionInactiveOpcode(WorldPacket& recvPacket)
{
    CmsgSetFactionInactive recv_packet;
    if (!recv_packet.deserialise(recvPacket))
        return;

    GetPlayer()->SetFactionInactive(recv_packet.id, recv_packet.state == 1);
}

void WorldSession::handleCharDeleteOpcode(WorldPacket& recvPacket)
{
    CmsgCharDelete recv_packet;
    if (!recv_packet.deserialise(recvPacket))
        return;

    const uint8_t deleteResult = DeleteCharacter(recv_packet.guid.getGuidLow());
    SendPacket(SmsgCharDelete(deleteResult).serialise().get());
}

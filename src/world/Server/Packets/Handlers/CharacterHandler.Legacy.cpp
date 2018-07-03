/*
 * AscEmu Framework based on ArcEmu MMORPG Server
 * Copyright (c) 2014-2018 AscEmu Team <http://www.ascemu.org>
 * Copyright (C) 2008-2012 ArcEmu Team <http://www.ArcEmu.org/>
 * Copyright (C) 2005-2007 Ascent Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "StdAfx.h"
#include "git_version.h"
#include "AuthCodes.h"
#include "Management/Battleground/Battleground.h"
#include "Storage/MySQLDataStore.hpp"
#include "Server/MainServerDefines.h"
#include "Data/WoWPlayer.h"
#include "Server/Packets/SmsgCharEnum.h"
#include "Server/Packets/SmsgCharacterLoginFailed.h"
#include "Server/Packets/CmsgPlayerLogin.h"
#include "Server/Packets/SmsgCharCustomize.h"
#include "Server/Packets/CmsgCharCustomize.h"
#include "Map/MapMgr.h"
#if VERSION_STRING == Cata
#include "GameCata/Management/GuildMgr.h"
#else
#include "Spell/Definitions/PowerType.h"
#endif

using namespace AscEmu::Packets;

//MIT start
#if VERSION_STRING <= WotLK
void WorldSession::CharacterEnumProc(QueryResult* result)
{
    std::vector<CharEnumData> enumData;

    has_dk = false;
    _side = -1;

    uint32_t numchar = 0;
    uint8_t char_real_count = 0;

    if (result)
        numchar = result->GetRowCount();

    auto startTime = Util::TimeNow();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            CharEnumData charEnum;

            charEnum.guid = fields[0].GetUInt64();
            charEnum.level = fields[1].GetUInt8();
            charEnum.race = fields[2].GetUInt8();
            charEnum.Class = fields[3].GetUInt8();

            if (!isClassRaceCombinationPossible(charEnum.Class, charEnum.race))
            {
                LogDebugFlag(LF_OPCODE, "Class %u and race %u is not a valid combination for Version %u - skipped", charEnum.Class, charEnum.race, VERSION_STRING);
                continue;
            }

            charEnum.gender = fields[4].GetUInt8();
            charEnum.bytes = fields[5].GetUInt32();
            charEnum.bytes2 = fields[6].GetUInt32();
            charEnum.name = fields[7].GetString();
            charEnum.x = fields[8].GetFloat();
            charEnum.y = fields[9].GetFloat();
            charEnum.z = fields[10].GetFloat();
            charEnum.mapId = fields[11].GetUInt32();
            charEnum.zoneId = fields[12].GetUInt32();
            charEnum.banned = fields[13].GetUInt32();

            charEnum.deathState = fields[15].GetUInt32();
            charEnum.loginFlags = fields[16].GetUInt32();
            charEnum.flags = fields[17].GetUInt32();
            charEnum.guildId = fields[18].GetUInt32();

            if (_side < 0)
                _side = getSideByRace(charEnum.race);

#if VERSION_STRING >= WotLK
            has_level_55_char = has_level_55_char || (charEnum.level >= 55);
            has_dk = has_dk || (charEnum.Class == DEATHKNIGHT);
#endif

            charEnum.char_flags = 0;

            if (charEnum.banned && (charEnum.banned < 10 || charEnum.banned >(uint32_t)UNIXTIME))
                charEnum.char_flags |= CHARACTER_SCREEN_FLAG_BANNED;
            if (charEnum.deathState != 0)
                charEnum.char_flags |= CHARACTER_SCREEN_FLAG_DEAD;
            if (charEnum.flags & PLAYER_FLAG_NOHELM)
                charEnum.char_flags |= CHARACTER_SCREEN_FLAG_HIDE_HELM;
            if (charEnum.flags & PLAYER_FLAG_NOCLOAK)
                charEnum.char_flags |= CHARACTER_SCREEN_FLAG_HIDE_CLOAK;
            if (charEnum.loginFlags == 1)
                charEnum.char_flags |= CHARACTER_SCREEN_FLAG_FORCED_RENAME;

#if VERSION_STRING >= WotLK
            switch (charEnum.loginFlags)
            {
                case LOGIN_CUSTOMIZE_LOOKS:
                    charEnum.customization_flag = CHAR_CUSTOMIZE_FLAG_CUSTOMIZE;
                    break;
                case LOGIN_CUSTOMIZE_RACE:
                    charEnum.customization_flag = CHAR_CUSTOMIZE_FLAG_RACE;
                    break;
                case LOGIN_CUSTOMIZE_FACTION:
                    charEnum.customization_flag = CHAR_CUSTOMIZE_FLAG_FACTION;
                    break;
                default:
                    charEnum.customization_flag = CHAR_CUSTOMIZE_FLAG_NONE;
            }
#endif

            CreatureProperties const* petInfo = nullptr;
            uint32_t petLevel = 0;

            if (charEnum.Class == WARLOCK || charEnum.Class == HUNTER)
            {
                QueryResult* player_pet_db_result = CharacterDatabase.Query("SELECT entry, level FROM playerpets WHERE ownerguid = %u AND MOD(active, 10) = 1 AND alive = TRUE;", Arcemu::Util::GUID_LOPART(charEnum.guid));
                if (player_pet_db_result)
                {
                    petLevel = player_pet_db_result->Fetch()[1].GetUInt32();
                    petInfo = sMySQLStore.getCreatureProperties(player_pet_db_result->Fetch()[0].GetUInt32());
                    delete player_pet_db_result;
                }
            }

            charEnum.pet_data.display_id = 0;
            charEnum.pet_data.level = 0;
            charEnum.pet_data.family = 0;

            if (petInfo != nullptr)
            {
                charEnum.pet_data.display_id = petInfo->Male_DisplayID;
                charEnum.pet_data.level = petLevel;
                charEnum.pet_data.family = petInfo->Family;
            }

            QueryResult* item_db_result = CharacterDatabase.Query("SELECT slot, entry, enchantments FROM playeritems WHERE ownerguid=%u AND containerslot = '-1' AND slot BETWEEN '0' AND '20'", Arcemu::Util::GUID_LOPART(charEnum.guid));
#if VERSION_STRING >= WotLK
            memset(charEnum.player_items, 0, sizeof(PlayerItem) * INVENTORY_SLOT_BAG_END);
#else
            memset(charEnum.player_items, 0, sizeof(PlayerItem) * 20);
#endif

            if (item_db_result)
            {
                do
                {
                    uint32_t enchantid;

                    int8_t item_slot = item_db_result->Fetch()[0].GetInt8();
                    ItemProperties const* itemProperties = sMySQLStore.getItemProperties(item_db_result->Fetch()[1].GetUInt32());
                    if (itemProperties)
                    {
                        charEnum.player_items[item_slot].displayId = itemProperties->DisplayInfoID;
                        charEnum.player_items[item_slot].inventoryType = static_cast<uint8>(itemProperties->InventoryType);

                        if (item_slot == EQUIPMENT_SLOT_MAINHAND || item_slot == EQUIPMENT_SLOT_OFFHAND)
                        {
                            const char* enchant_field = item_db_result->Fetch()[2].GetString();
                            if (sscanf(enchant_field, "%u,0,0;", (unsigned int*)&enchantid) == 1 && enchantid > 0)
                            {
                                DBC::Structures::SpellItemEnchantmentEntry const* spell_item_enchant = sSpellItemEnchantmentStore.LookupEntry(enchantid);
                                if (spell_item_enchant != nullptr)
                                    charEnum.player_items[item_slot].enchantmentId = spell_item_enchant->visual;
                            }
                        }
                    }
                } while (item_db_result->NextRow());
                delete item_db_result;
            }

            // save data to serialize it in packet serialisation SmsgCharEnum.
            enumData.push_back(charEnum);

            ++char_real_count;
        } while (result->NextRow());
    }

    LogDebugFlag(LF_OPCODE, "Character Enum Built in %u ms.", Util::GetTimeDifferenceToNow(startTime));
    SendPacket(AscEmu::Packets::SmsgCharEnum(char_real_count, enumData).serialise().get());
}
#elif VERSION_STRING == Cata
void WorldSession::CharacterEnumProc(QueryResult* result)
{
    has_dk = false;
    _side = -1;

    auto startTime = Util::TimeNow();

    WorldPacket data(SMSG_CHAR_ENUM, 270);
    ByteBuffer buffer;

    data.writeBits(0, 23);
    data.writeBit(1);
    data.writeBits(result ? result->GetRowCount() : 0, 17);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            CharEnumData charEnum;

            uint32_t dbGuid = fields[0].GetUInt32();
            ObjectGuid guid = MAKE_NEW_GUID(dbGuid, 0, 0x000);
            charEnum.level = fields[1].GetUInt8();
            charEnum.race = fields[2].GetUInt8();
            charEnum.Class = fields[3].GetUInt8();
            charEnum.gender = fields[4].GetUInt8();

            charEnum.bytes = fields[5].GetUInt32();
            uint8_t skin = uint8_t(charEnum.bytes & 0xFF);
            uint8_t face = uint8_t((charEnum.bytes >> 8) & 0xFF);
            uint8_t hairStyle = uint8_t((charEnum.bytes >> 16) & 0xFF);
            uint8_t hairColor = uint8_t((charEnum.bytes >> 24) & 0xFF);
            uint8_t facialHair = uint8_t(charEnum.bytes & 0xFF);

            charEnum.bytes2 = fields[6].GetUInt32();
            charEnum.name = fields[7].GetString();
            charEnum.x = fields[8].GetFloat();
            charEnum.y = fields[9].GetFloat();
            charEnum.z = fields[10].GetFloat();
            charEnum.mapId = fields[11].GetUInt32();
            charEnum.zoneId = fields[12].GetUInt32();
            charEnum.banned = fields[13].GetUInt32();

            charEnum.deathState = fields[15].GetUInt32();
            charEnum.loginFlags = fields[16].GetUInt32();
            charEnum.flags = fields[17].GetUInt32();
            charEnum.guildId = fields[18].GetUInt32();
            ObjectGuid guildGuid = MAKE_NEW_GUID(charEnum.guildId, 0, HIGHGUID_TYPE_GUILD);

            if (_side < 0)
                _side = getSideByRace(charEnum.race);

            has_level_55_char = has_level_55_char || (charEnum.level >= 55);
            has_dk = has_dk || (charEnum.Class == DEATHKNIGHT);

            uint32_t char_flags = 0;

            if (charEnum.banned && (charEnum.banned < 10 || charEnum.banned >(uint32_t)UNIXTIME))
                char_flags |= CHARACTER_SCREEN_FLAG_BANNED;
            if (charEnum.deathState != 0)
                char_flags |= CHARACTER_SCREEN_FLAG_DEAD;
            if (charEnum.flags & PLAYER_FLAG_NOHELM)
                char_flags |= CHARACTER_SCREEN_FLAG_HIDE_HELM;
            if (charEnum.flags & PLAYER_FLAG_NOCLOAK)
                char_flags |= CHARACTER_SCREEN_FLAG_HIDE_CLOAK;
            if (charEnum.loginFlags == 1)
                char_flags |= CHARACTER_SCREEN_FLAG_FORCED_RENAME;

            data.writeBit(guid[3]);
            data.writeBit(guildGuid[1]);
            data.writeBit(guildGuid[7]);
            data.writeBit(guildGuid[2]);
            data.writeBits(uint32_t(charEnum.name.length()), 7);
            data.writeBit(guid[4]);
            data.writeBit(guid[7]);
            data.writeBit(guildGuid[3]);
            data.writeBit(guid[5]);
            data.writeBit(guildGuid[6]);
            data.writeBit(guid[1]);
            data.writeBit(guildGuid[5]);
            data.writeBit(guildGuid[4]);
            data.writeBit(charEnum.loginFlags & 0x20); // 0x20 = AT_LOGIN_FIRST
            data.writeBit(guid[0]);
            data.writeBit(guid[2]);
            data.writeBit(guid[6]);
            data.writeBit(guildGuid[0]);


            CreatureProperties const* petInfo = nullptr;
            uint32_t petDisplayId;
            uint32_t petLevel = 0;
            uint32_t petFamily;

            if (charEnum.Class == WARLOCK || charEnum.Class == HUNTER)
            {
                QueryResult* player_pet_db_result = CharacterDatabase.Query("SELECT entry, level FROM playerpets WHERE ownerguid = %u AND MOD(active, 10) = 1 AND alive = TRUE;", dbGuid);
                if (player_pet_db_result)
                {
                    petLevel = player_pet_db_result->Fetch()[1].GetUInt32();
                    petInfo = sMySQLStore.getCreatureProperties(player_pet_db_result->Fetch()[0].GetUInt32());
                    delete player_pet_db_result;
                }
            }

            if (petInfo != nullptr)
            {
                petDisplayId = petInfo->Male_DisplayID;
                petLevel = petLevel;
                petFamily = petInfo->Family;
            }
            else
            {
                petDisplayId = 0;
                petLevel = 0;
                petFamily = 0;
            }

            buffer << uint8_t(charEnum.Class);

            QueryResult* item_db_result = CharacterDatabase.Query("SELECT containerslot, slot, entry, enchantments FROM playeritems WHERE ownerguid=%u AND containerslot = '-1' AND slot < 23", dbGuid);

            memset(charEnum.player_items, 0, sizeof(charEnum.player_items));

            if (item_db_result)
            {
                do
                {
                    uint32_t enchantid;
                    uint32_t container_slot = item_db_result->Fetch()[0].GetInt32();
                    int8_t item_slot = item_db_result->Fetch()[1].GetInt8();
                    if (container_slot == -1 && item_slot < INVENTORY_SLOT_BAG_END && item_slot >= EQUIPMENT_SLOT_START)
                    {
                        ItemProperties const* itemProperties = sMySQLStore.getItemProperties(item_db_result->Fetch()[2].GetUInt32());
                        if (itemProperties)
                        {
                            if (!(item_slot == EQUIPMENT_SLOT_HEAD && (charEnum.flags & (uint32_t)PLAYER_FLAG_NOHELM) != 0) &&
                                !(item_slot == EQUIPMENT_SLOT_BACK && (charEnum.flags & (uint32_t)PLAYER_FLAG_NOCLOAK) != 0))
                            {
                                charEnum.player_items[item_slot].displayId = itemProperties->DisplayInfoID;
                                charEnum.player_items[item_slot].inventoryType = static_cast<uint8>(itemProperties->InventoryType);

                                if (item_slot == EQUIPMENT_SLOT_MAINHAND || item_slot == EQUIPMENT_SLOT_OFFHAND)
                                {
                                    const char* enchant_field = item_db_result->Fetch()[3].GetString();
                                    if (sscanf(enchant_field, "%u,0,0;", (unsigned int*)&enchantid) == 1 && enchantid > 0)
                                    {
                                        DBC::Structures::SpellItemEnchantmentEntry const* spell_item_enchant = sSpellItemEnchantmentStore.LookupEntry(enchantid);
                                        if (spell_item_enchant != nullptr)
                                            charEnum.player_items[item_slot].enchantmentId = spell_item_enchant->visual;
                                        else
                                            charEnum.player_items[item_slot].enchantmentId = 0;
                                    }
                                }
                            }
                        }
                        else
                        {
                            LOG_ERROR("Table player_items includes invalid item entry %u!", item_db_result->Fetch()[1].GetUInt32());
                        }
                    }
                } while (item_db_result->NextRow());
                delete item_db_result;
            }

            for (uint8_t i = 0; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                buffer << uint8_t(charEnum.player_items[i].inventoryType);
                buffer << uint32_t(charEnum.player_items[i].displayId);
                buffer << uint32_t(charEnum.player_items[i].enchantmentId);
            }

            buffer << uint32_t(petFamily);
            buffer.WriteByteSeq(guildGuid[2]);
            buffer << uint8_t(0);
            buffer << uint8_t(hairStyle);
            buffer.WriteByteSeq(guildGuid[3]);
            buffer << uint32_t(petDisplayId);
            buffer << uint32_t(char_flags);
            buffer << uint8_t(hairColor);
            buffer.WriteByteSeq(guid[4]);
            buffer << uint32_t(charEnum.mapId);
            buffer.WriteByteSeq(guildGuid[5]);
            buffer << float(charEnum.z);
            buffer.WriteByteSeq(guildGuid[6]);
            buffer << uint32_t(petLevel);
            buffer.WriteByteSeq(guid[3]);
            buffer << float(charEnum.y);

            switch (charEnum.loginFlags)
            {
                case LOGIN_CUSTOMIZE_LOOKS:
                    buffer << uint32_t(CHAR_CUSTOMIZE_FLAG_CUSTOMIZE);    //Character recustomization flag
                    break;
                case LOGIN_CUSTOMIZE_RACE:
                    buffer << uint32_t(CHAR_CUSTOMIZE_FLAG_RACE);         //Character recustomization + race flag
                    break;
                case LOGIN_CUSTOMIZE_FACTION:
                    buffer << uint32_t(CHAR_CUSTOMIZE_FLAG_FACTION);      //Character recustomization + race + faction flag
                    break;
                default:
                    buffer << uint32_t(CHAR_CUSTOMIZE_FLAG_NONE);         //Character recustomization no flag set
            }

            buffer << uint8_t(facialHair);
            buffer.WriteByteSeq(guid[7]);
            buffer << uint8_t(charEnum.gender);
            buffer.append(charEnum.name.c_str(), charEnum.name.length());
            buffer << uint8_t(face);
            buffer.WriteByteSeq(guid[0]);
            buffer.WriteByteSeq(guid[2]);
            buffer.WriteByteSeq(guildGuid[1]);
            buffer.WriteByteSeq(guildGuid[7]);
            buffer << float(charEnum.x);
            buffer << uint8_t(skin);
            buffer << uint8_t(charEnum.race);
            buffer << uint8_t(charEnum.level);
            buffer.WriteByteSeq(guid[6]);
            buffer.WriteByteSeq(guildGuid[4]);
            buffer.WriteByteSeq(guildGuid[0]);
            buffer.WriteByteSeq(guid[5]);
            buffer.WriteByteSeq(guid[1]);
            buffer << uint32_t(charEnum.zoneId);

        } while (result->NextRow());

        data.flushBits();
        data.append(buffer);
    }

    LogDebugFlag(LF_OPCODE, "Character Enum Built in %u ms.", Util::GetTimeDifferenceToNow(startTime));
    SendPacket(&data);
}
#endif

//MIT end

LoginErrorCode VerifyName(const char* name, size_t nlen)
{
    const char* p;
    size_t i;

    static const char* bannedCharacters = "\t\v\b\f\a\n\r\\\"\'\? <>[](){}_=+-|/!@#$%^&*~`.,0123456789\0";
    static const char* allowedCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (worldConfig.server.enableLimitedNames)
    {
        if (nlen == 0)
            return E_CHAR_NAME_NO_NAME;
        else if (nlen < 2)
            return E_CHAR_NAME_TOO_SHORT;
        else if (nlen > 12)
            return E_CHAR_NAME_TOO_LONG;

        for (i = 0; i < nlen; ++i)
        {
            p = allowedCharacters;
            for (; *p != 0; ++p)
            {
                if (name[i] == *p)
                    goto cont;
            }
            return E_CHAR_NAME_INVALID_CHARACTER;
        cont:
            continue;
        }
    }
    else
    {
        for (i = 0; i < nlen; ++i)
        {
            p = bannedCharacters;
            while (*p != 0 && name[i] != *p && name[i] != 0)
                ++p;

            if (*p != 0)
                return E_CHAR_NAME_INVALID_CHARACTER;
        }
    }

    return E_CHAR_NAME_SUCCESS;
}

void WorldSession::HandleCharEnumOpcode(WorldPacket& /*recvData*/)
{
    AsyncQuery* q = new AsyncQuery(new SQLClassCallbackP1<World, uint32>(World::getSingletonPtr(), &World::sendCharacterEnumToAccountSession, GetAccountId()));

    q->AddQuery("SELECT guid, level, race, class, gender, bytes, bytes2, name, positionX, positionY, positionZ, mapId, zoneId, banned, restState, deathstate, login_flags, player_flags, guild_data.guildid FROM characters LEFT JOIN guild_data ON characters.guid = guild_data.playerid WHERE acct=%u ORDER BY guid LIMIT 10", GetAccountId());

    CharacterDatabase.QueueAsyncQuery(q);
}

void WorldSession::LoadAccountDataProc(QueryResult* result)
{
    size_t len;
    const char* data;
    char* d;

    if (!result)
    {
        CharacterDatabase.Execute("INSERT INTO account_data VALUES(%u, '', '', '', '', '', '', '', '', '')", _accountId);
        return;
    }

    for (uint8 i = 0; i < 7; ++i)
    {
        data = result->Fetch()[1 + i].GetString();
        len = data ? strlen(data) : 0;
        if (len > 1)
        {
            d = new char[len + 1];
            memcpy(d, data, len + 1);
            SetAccountData(i, d, true, (uint32)len);
        }
    }
}

#if VERSION_STRING != Cata
void WorldSession::FullLogin(Player* plr)
{
    LOG_DEBUG("Fully loading player %u", plr->getGuidLow());

    SetPlayer(plr);
    m_MoverWoWGuid.Init(plr->getGuid());

    MapMgr* mgr = sInstanceMgr.GetInstance(plr);
    if (mgr && mgr->m_battleground)
    {
        // Don't allow player to login into a bg that has ended or is full
        if (mgr->m_battleground->HasEnded() == true ||
                mgr->m_battleground->HasFreeSlots(plr->GetTeamInitial(), mgr->m_battleground->GetType() == false))
        {
            mgr = NULL;
        }
    }

    // Trying to log to an instance that doesn't exists anymore?
    if (!mgr)
    {
        if (!IS_INSTANCE(plr->m_bgEntryPointMap))
        {
            plr->m_position.x = plr->m_bgEntryPointX;
            plr->m_position.y = plr->m_bgEntryPointY;
            plr->m_position.z = plr->m_bgEntryPointZ;
            plr->m_position.o = plr->m_bgEntryPointO;
            plr->m_mapId = plr->m_bgEntryPointMap;
        }
        else
        {
            plr->m_position.x = plr->GetBindPositionX();
            plr->m_position.y = plr->GetBindPositionY();
            plr->m_position.z = plr->GetBindPositionZ();
            plr->m_position.o = 0;
            plr->m_mapId = plr->GetBindMapId();
        }
    }

    // copy to movement array
    movement_packet[0] = m_MoverWoWGuid.GetNewGuidMask();
    memcpy(&movement_packet[1], m_MoverWoWGuid.GetNewGuid(), m_MoverWoWGuid.GetNewGuidLen());

    // world preload
    uint32 VMapId;
    float VO;
    float VX;
    float VY;
    float VZ;

    // GMs should start on GM Island and be bound there
    if (HasGMPermissions() && plr->m_FirstLogin && worldConfig.gm.isStartOnGmIslandEnabled)
    {
        VMapId = 1;
        VO = 0;
        VX = 16222.6f;
        VY = 16265.9f;
        VZ = 14.2085f;

        plr->m_position.x = VX;
        plr->m_position.y = VY;
        plr->m_position.z = VZ;
        plr->m_position.o = VO;
        plr->m_mapId = VMapId;

        plr->SetBindPoint(plr->GetPositionX(), plr->GetPositionY(), plr->GetPositionZ(), plr->GetMapId(), plr->GetZoneId());
    }
    else
    {
        VMapId = plr->GetMapId();
        VO = plr->GetOrientation();
        VX = plr->GetPositionX();
        VY = plr->GetPositionY();
        VZ = plr->GetPositionZ();
    }

    plr->SendLoginVerifyWorld(VMapId, VX, VY, VZ, VO);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    // send voicechat state - active/inactive
    //
    // {SERVER} Packet: (0x03C7) UNKNOWN PacketSize = 2
    // |------------------------------------------------|----------------|
    // |00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F |0123456789ABCDEF|
    // |------------------------------------------------|----------------|
    // |02 01                                            |..              |
    // -------------------------------------------------------------------
    //
    //
    // Old packetdump is OLD. This is probably from 2.2.0 (that was the patch when it was added to wow)!
    //
    //////////////////////////////////////////////////////////////////////////////////////////////////////


    //StackWorldPacket<20> datab(SMSG_FEATURE_SYSTEM_STATUS);
    //datab.Initialize(SMSG_FEATURE_SYSTEM_STATUS);
    //datab << uint8(2); // Voice Chat
    //datab << uint8(0); // Can use it?
    //SendPacket(&datab);

#if VERSION_STRING > TBC
    WorldPacket dataldm(SMSG_LEARNED_DANCE_MOVES, 4 + 4);

    dataldm << uint32(0);
    dataldm << uint32(0);

    SendPacket(&dataldm);
#endif
    plr->UpdateAttackSpeed();

    // Make sure our name exists (for premade system)
    PlayerInfo* info = objmgr.GetPlayerInfo(plr->getGuidLow());

    if (info == NULL)
    {
        info = new PlayerInfo;
        info->cl = plr->getClass();
        info->gender = plr->getGender();
        info->guid = plr->getGuidLow();
        info->name = strdup(plr->getName().c_str());
        info->lastLevel = plr->getLevel();
        info->lastOnline = UNIXTIME;
        info->lastZone = plr->GetZoneId();
        info->race = plr->getRace();
        info->team = plr->GetTeam();
        info->guild = NULL;
        info->guildRank = NULL;
        info->guildMember = NULL;
        info->m_Group = NULL;
        info->subGroup = 0;
        objmgr.AddPlayerInfo(info);
    }
    plr->m_playerInfo = info;
    if (plr->m_playerInfo->guild)
    {
        plr->SetGuildId(plr->m_playerInfo->guild->getGuildId());
        plr->SetGuildRank(plr->m_playerInfo->guildRank->iId);
    }

    info->m_loggedInPlayer = plr;

    // account data == UI config
    SendAccountDataTimes(PER_CHARACTER_CACHE_MASK);

    // Set TIME OF LOGIN
    CharacterDatabase.Execute("UPDATE characters SET online = 1 WHERE guid = %u" , plr->getGuidLow());

    bool enter_world = true;

    auto data = plr->getXp();

    // Find our transporter and add us if we're on one.
    if (plr->obj_movement_info.transport_data.transportGuid != 0)
    {
        Transporter* pTrans = objmgr.GetTransporter(Arcemu::Util::GUID_LOPART(plr->obj_movement_info.transport_data.transportGuid));
        if (pTrans)
        {
            if (plr->IsDead())
            {
                plr->ResurrectPlayer();
                plr->setHealth(plr->getMaxHealth());
                plr->SetPower(POWER_TYPE_MANA, plr->GetMaxPower(POWER_TYPE_MANA));
            }

            float c_tposx = pTrans->GetPositionX() + plr->GetTransPositionX();
            float c_tposy = pTrans->GetPositionY() + plr->GetTransPositionY();
            float c_tposz = pTrans->GetPositionZ() + plr->GetTransPositionZ();

            if (plr->GetMapId() != pTrans->GetMapId())       // loaded wrong map
            {
                plr->SetMapId(pTrans->GetMapId());

                StackWorldPacket<20> dataw(SMSG_NEW_WORLD);

                dataw << pTrans->GetMapId();
                dataw << c_tposx;
                dataw << c_tposy;
                dataw << c_tposz;
                dataw << plr->GetOrientation();

                SendPacket(&dataw);

                // shit is sent in worldport ack.
                enter_world = false;
            }

            plr->SetPosition(c_tposx, c_tposy, c_tposz, plr->GetOrientation(), false);
            pTrans->AddPassenger(plr);
        }
    }

    LOG_DEBUG("Player %s logged in.", plr->getName().c_str());

    sWorld.incrementPlayerCount(plr->GetTeam());

    if (plr->m_FirstLogin && !worldConfig.player.skipCinematics)
    {
#if VERSION_STRING > TBC
        if (const auto charEntry = sChrClassesStore.LookupEntry(plr->getClass()))
        {
            if (charEntry->cinematic_id != 0)
                OutPacket(SMSG_TRIGGER_CINEMATIC, 4, &charEntry->cinematic_id);
            else if (const auto raceEntry = sChrRacesStore.LookupEntry(plr->getRace()))
                OutPacket(SMSG_TRIGGER_CINEMATIC, 4, &raceEntry->cinematic_id);
        }
#else
        if (const auto raceEntry = sChrRacesStore.LookupEntry(plr->getRace()))
            OutPacket(SMSG_TRIGGER_CINEMATIC, 4, &raceEntry->cinematic_id);
#endif
    }

    LOG_DETAIL("WORLD: Created new player for existing players (%s)", plr->getName().c_str());

    // Login time, will be used for played time calc
    plr->m_playedtime[2] = uint32(UNIXTIME);

    //Issue a message telling all guild members that this player has signed on
    if (plr->IsInGuild())
    {
        plr->SendGuildMOTD();
        plr->m_playerInfo->guild->LogGuildEvent(GE_SIGNED_ON, 1, plr->getName().c_str());
    }

    // Send online status to people having this char in friendlist
    _player->Social_TellFriendsOnline();
    // send friend list (for ignores)
    _player->Social_SendFriendList(7);

#if VERSION_STRING != TBC
    plr->SendDungeonDifficulty();
    plr->SendRaidDifficulty();

    plr->SendEquipmentSetList();
#endif

#ifndef GM_TICKET_MY_MASTER_COMPATIBLE
    GM_Ticket* ticket = objmgr.GetGMTicketByPlayer(_player->getGuid());
    if (ticket != NULL)
    {
        //Send status change to gm_sync_channel
        Channel* chn = channelmgr.GetChannel(sWorld.getGmClientChannel().c_str(), _player);
        if (chn)
        {
            std::stringstream ss;
            ss << "GmTicket:" << GM_TICKET_CHAT_OPCODE_ONLINESTATE;
            ss << ":" << ticket->guid;
            ss << ":1";
            chn->Say(_player, ss.str().c_str(), NULL, true);
        }
    }
#endif

    if (Config.MainConfig.getBoolDefault("Server", "SendStatsOnJoin", false))
    {
#ifdef WIN32
        _player->BroadcastMessage("Server: %sAscEmu - %s-Windows-%s", MSG_COLOR_WHITE, CONFIG, ARCH);
#else
        _player->BroadcastMessage("Server: %sAscEmu - %s-%s", MSG_COLOR_WHITE, PLATFORM_TEXT, ARCH);
#endif

        // Revision
        _player->BroadcastMessage("Build hash: %s%s", MSG_COLOR_CYAN, BUILD_HASH_STR);
        // Shows Online players, and connection peak
        _player->BroadcastMessage("Online Players: %s%u |rPeak: %s%u|r Accepted Connections: %s%u",
            MSG_COLOR_SEXGREEN, sWorld.getSessionCount(), MSG_COLOR_SEXBLUE, sWorld.getPeakSessionCount(), MSG_COLOR_SEXBLUE, sWorld.getAcceptedConnections());

        // Shows Server uptime
        _player->BroadcastMessage("Server Uptime: |r%s", sWorld.getWorldUptimeString().c_str());
    }

    // server Message Of The Day
    //SendMOTD();
    plr->SendDungeonDifficulty();

    //Set current RestState
    if (plr->m_isResting)
        // We are resting at an inn , turn on Zzz
        plr->ApplyPlayerRestState(true);

    //Calculate rest bonus if there is time between lastlogoff and now
    if (plr->m_timeLogoff > 0 && plr->getLevel() < plr->getMaxLevel())    // if timelogoff = 0 then it's the first login
    {
        uint32 currenttime = uint32(UNIXTIME);
        uint32 timediff = currenttime - plr->m_timeLogoff;

        //Calculate rest bonus
        if (timediff > 0)
            plr->AddCalculatedRestXP(timediff);
    }

    if (info->m_Group)
        info->m_Group->Update();

    if (enter_world && !_player->GetMapMgr())
        plr->AddToWorld();

    sHookInterface.OnFullLogin(_player);

    objmgr.AddPlayer(_player);
}
#endif


void WorldSession::HandleDeclinedPlayerNameOpcode(WorldPacket& recv_data)
{
    uint32_t error = 0;     // 0 = success, 1 = error

    uint64_t guid;
    std::string name;

    recv_data >> guid;
    recv_data >> name;

    //\todo check utf8 and cyrillic chars
    // check declined names

    WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
    data << uint32_t(error);
    data << uint64_t(guid);
    SendPacket(&data);
}

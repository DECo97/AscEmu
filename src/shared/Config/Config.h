/*
 * AscEmu Framework based on ArcEmu MMORPG Server
 * Copyright (C) 2014-2017 AscEmu Team <http://www.ascemu.org>
 * Copyright (C) 2008-2012 ArcEmu Team <http://www.ArcEmu.org/>
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

#ifndef CONFIG_H
#define CONFIG_H

#include "Common.hpp"

#ifndef EOL
#ifdef WIN32
#define EOL "\r\n"
#else
#define EOL "\n"
#endif
#endif

#ifndef EOL_SIZE
#ifdef WIN32
#define EOL_SIZE 2
#else
#define EOL_SIZE 1
#endif
#endif

struct SERVER_DECL ConfigSetting
{
    std::string AsString;
    bool AsBool;
    int AsInt;
    float AsFloat;
};

typedef std::map<uint32, ConfigSetting> ConfigBlock;

class SERVER_DECL ConfigFile
{
    public:

        ConfigFile();
        ~ConfigFile();

        bool SetSource(const char* file, bool ignorecase = true);
        ConfigSetting* GetSetting(std::string Block, std::string Setting);

        bool GetString(std::string block, std::string name, std::string* value);
        std::string GetStringDefault(std::string block, std::string name, const char* def);
        bool GetString(const char* block, char* buffer, const char* name, const char* def, uint32 len);

        bool GetBool(const char* block, const char* name, bool* value);
        bool GetBoolDefault(const char* block, const char* name, const bool def);

        bool GetInt(std::string block, std::string name, int* value);
        int GetIntDefault(std::string block, std::string name, const int def);

        bool GetFloat(std::string block, std::string name, float* value);
        float GetFloatDefault(std::string block, std::string name, const float def);

    private:

        std::map<uint32, ConfigBlock> m_settings;
};


class SERVER_DECL ConfigMgr
{
        // Mainly used for WS, others will probably only have one.
    public:

        ConfigFile MainConfig;
        ConfigFile ClusterConfig;
};

extern SERVER_DECL ConfigMgr Config;

#endif      //CONFIG_H

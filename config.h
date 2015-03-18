
/*
    config.h, part of bv-tapi a SIP-TAPI bridge.
    Copyright (C) 2011  Nick Knight

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/


#include <string>
#include <map>

typedef std::map<std::string, std::string> strstrmap;
typedef std::pair<std::string, std::string> strstrpair;


void loadConfig(void);
void saveConfig(void);
std::string getConfigValue(std::string key);
bool setConfigValue(std::string key, std::string value);
bool setProxyServer(std::string key, std::string value);

strstrmap getLines(void);
strstrpair getLineByIndex(int pos);
bool addLine(std::string name, std::string url);
void removeLine(std::string name);

/*
 Copyright (C) 2019 Fredrik Öhrström

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

#include"config.h"
#include"meters.h"

#include<vector>
#include<string>

using namespace std;

pair<string,string> getNextKeyValue(vector<char> &buf, vector<char>::iterator &i)
{
    bool eof, err;
    string key, value;
    key = eatToSkipWhitespace(buf, i, '=', 64, &eof, &err);
    if (eof || err) goto nomore;
    value = eatToSkipWhitespace(buf, i, '\n', 4096, &eof, &err);
    if (err) goto nomore;

    return { key, value };

    nomore:

    return { "", "" };
}

void parseMeterConfig(CommandLine *c, vector<char> &buf, string file)
{
    auto i = buf.begin();
    string name;
    string type;
    string id;
    string key;

    for (;;) {
        auto p = getNextKeyValue(buf, i);

        if (p.first == "") break;

        if (p.first == "name") name = p.second;
        if (p.first == "type") type = p.second;
        if (p.first == "id") id = p.second;
        if (p.first == "key") key = p.second;
    }

    MeterType mt = toMeterType(type);
    if (mt == UNKNOWN_METER) error("Not a valid meter type \"%s\"\n", type.c_str());
    if (!isValidId(id)) error("Not a valid meter id \"%s\"\n", id.c_str());
    if (!isValidKey(key)) error("Not a valid meter key \"%s\"\n", key.c_str());

    c->meters.push_back(MeterInfo(name, type, id, key));

    return;

}

void handleLoglevel(CommandLine *c, string loglevel)
{
    if (loglevel == "verbose") { c->verbose = true; }
    else if (loglevel == "debug") { c->debug = true; }
    else if (loglevel == "silent") { c->silence = true; }
    else if (loglevel == "normal") { }
    else {
        warning("No such log level: \"%s\"\n", loglevel.c_str());
    }
}

void handleDevice(CommandLine *c, string device)
{
    c->usb_device = device;
}

void handleLogtelegrams(CommandLine *c, string logtelegrams)
{
    if (logtelegrams == "true") { c->logtelegrams = true; }
    else if (logtelegrams == "false") { c->logtelegrams = false;}
    else {
        warning("No such logtelegrams setting: \"%s\"\n", logtelegrams.c_str());
    }
}

void handleMeterfilesdir(CommandLine *c, string meterfilesdir)
{
    if (meterfilesdir.length() > 0)
    {
        c->meterfiles_dir = meterfilesdir;
        c->meterfiles = true;
        if (!checkIfDirExists(c->meterfiles_dir.c_str())) {
            warning("Cannot write meter files into dir \"%s\"\n", c->meterfiles_dir.c_str());
        }
    }
}

void handleRobot(CommandLine *c, string robot)
{
    if (robot == "json")
    {
        c->json = true;
        c->fields = false;
    }
    else if (robot == "fields")
    {
        c->json = false;
        c->fields = true;
        c->separator = ';';
    } else {
        warning("Unknown output format: \"%s\"\n", robot.c_str());
    }
}

void handleSeparator(CommandLine *c, string s)
{
    if (s.length() == 1) {
        c->separator = s[0];
    } else {
        warning("Separator must be a single character.\n");
    }
}

unique_ptr<CommandLine> loadConfiguration(string root)
{
    CommandLine *c = new CommandLine;

    vector<char> global_conf;
    loadFile(root+"/etc/wmbusmeters.conf", &global_conf);

    auto i = global_conf.begin();

    for (;;) {
        auto p = getNextKeyValue(global_conf, i);

        if (p.first == "") break;
        if (p.first == "loglevel") handleLoglevel(c, p.second);
        else if (p.first == "device") handleDevice(c, p.second);
        else if (p.first == "logtelegrams") handleLogtelegrams(c, p.second);
        else if (p.first == "meterfilesdir") handleMeterfilesdir(c, p.second);
        else if (p.first == "robot") handleRobot(c, p.second);
        else if (p.first == "separator") handleSeparator(c, p.second);
        else
        {
            warning("No such key: %s\n", p.first.c_str());
        }
    }

    vector<string> meters;
    listFiles(root+"/etc/wmbusmeters.d", &meters);

    for (auto& f : meters)
    {
        vector<char> meter_conf;
        string file = root+"/etc/wmbusmeters.d/"+f;
        loadFile(file.c_str(), &meter_conf);
        parseMeterConfig(c, meter_conf, file);
    }

    return unique_ptr<CommandLine>(c);
}

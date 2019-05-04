/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

#include<assert.h>
#include<map>
#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

using namespace std;

struct MeterApator162 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterApator162(WMBus *bus, string& name, string& id, string& key);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();
    double maxFlow(Unit u);
    bool  hasMaxFlow();
    double flowTemperature(Unit u);
    bool  hasFlowTemperature();
    double externalTemperature(Unit u);
    bool  hasExternalTemperature();

    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    double total_water_consumption_ {};
};

MeterApator162::MeterApator162(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::APATOR162, MANUFACTURER_APA, LinkMode::T1)
{
    addMedia(0x06);
    addMedia(0x07);
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MeterApator162::totalWaterConsumption(Unit u)
{
    return total_water_consumption_;
}

unique_ptr<WaterMeter> createApator162(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MeterApator162(bus,name,id,key));
}

void MeterApator162::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) telegram for %s %02x%02x%02x%02x\n", "apator162",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    t->expectVersion("apator162", 0x05);

    if (t->isEncrypted() && !useAes() && !t->isSimulated()) {
        warning("(apator162) warning: telegram is encrypted but no key supplied!\n");
    }
    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode5_AES_CBC(t, aeskey);
    } else {
        t->content = t->payload;
    }
    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", "apator162");
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        snprintf(log_prefix, 255, "(%s)", "apator162");
        t->explainParse(log_prefix, content_start);
    }
    triggerUpdate(t);
}

void MeterApator162::processContent(Telegram *t)
{
    // Meter record:

    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    // Unfortunately, the at-wmbus-16-2 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram. Thus the parsing above ends
    // immediately with a 0x0f dif which means: from now on, its vendor specific
    // data structures.

    // By examining some telegrams though, it looks like the total consumption
    // counter is on offset 25. So we can fake a parse here, to make it easier
    // to extract using the existing tools.
    map<string,pair<int,DVEntry>> vendor_values;

    string total;
    strprintf(total, "%02x%02x%02x%02x", t->content[25], t->content[26], t->content[27], t->content[28]);
    vendor_values["0413"] = { 25, DVEntry(0x13, 0, 0, 0, total) };
    int offset;
    string key;
    if(findKey(ValueInformation::Volume, 0, &key, &vendor_values)) {
        extractDVdouble(&vendor_values, key, &offset, &total_water_consumption_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);
    }
}

void MeterApator162::printMeter(Telegram *t,
                                  string *human_readable,
                                  string *fields, char separator,
                                  string *json,
                                  vector<string> *envs)
{
    char buf[65536];
    buf[65535] = 0;

    snprintf(buf, sizeof(buf)-1,
             "%s\t"
             "%s\t"
             "% 3.3f m3\t"
             "%s",
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(Unit::M3),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1,
             "%s%c"
             "%s%c"
             "%f%c"
             "%s",
             name().c_str(), separator,
             t->id.c_str(), separator,
             totalWaterConsumption(Unit::M3), separator,
            datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,%s)
             QS(meter,apator162)
             QS(name,%s)
             QS(id,%s)
             Q(total_m3,%f)
             QSE(timestamp,%s)
             "}",
             mediaTypeJSON(t->a_field_device_type).c_str(),
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(Unit::M3),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=apator162"));
    envs->push_back(string("METER_ID=")+t->id);
    envs->push_back(string("METER_TOTAL_M3=")+to_string(totalWaterConsumption(Unit::M3)));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}

bool MeterApator162::hasTotalWaterConsumption()
{
    return true;
}

double MeterApator162::targetWaterConsumption(Unit u)
{
    return 0.0;
}

bool MeterApator162::hasTargetWaterConsumption()
{
    return false;
}

double MeterApator162::maxFlow(Unit u)
{
    return 0.0;
}

bool MeterApator162::hasMaxFlow()
{
    return false;
}

double MeterApator162::flowTemperature(Unit u)
{
    return 127;
}

bool MeterApator162::hasFlowTemperature()
{
    return false;
}

double MeterApator162::externalTemperature(Unit u)
{
    return 127;
}

bool MeterApator162::hasExternalTemperature()
{
    return false;
}

string MeterApator162::statusHumanReadable()
{
    return "";
}

string MeterApator162::status()
{
    return "";
}

string MeterApator162::timeDry()
{
    return "";
}

string MeterApator162::timeReversed()
{
    return "";
}

string MeterApator162::timeLeaking()
{
    return "";
}

string MeterApator162::timeBursting()
{
    return "";
}

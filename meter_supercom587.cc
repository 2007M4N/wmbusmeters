/*
 Copyright (C) 2017-2018 Fredrik Öhrström

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

struct MeterSupercom587 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterSupercom587(WMBus *bus, const char *name, const char *id, const char *key);

    // Total water counted through the meter
    double totalWaterConsumption();
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption();
    bool  hasTargetWaterConsumption();
    double maxFlow();
    bool  hasMaxFlow();

    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

    void printMeterHumanReadable(FILE *output);
    void printMeterFields(FILE *output, char separator);
    void printMeterJSON(FILE *output);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    double total_water_consumption_ {};
};

MeterSupercom587::MeterSupercom587(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, SUPERCOM587_METER, MANUFACTURER_SON, 0x16, LinkModeT1)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MeterSupercom587::totalWaterConsumption()
{
    return total_water_consumption_;
}

WaterMeter *createSupercom587(WMBus *bus, const char *name, const char *id, const char *key)
{
    return new MeterSupercom587(bus,name,id,key);
}

void MeterSupercom587::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) telegram for %s %02x%02x%02x%02x\n", "supercom587",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x16) {
        warning("(%s) expected telegram for water media, but got \"%s\"!\n", "supercom587",
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    /*
    if (t->m_field != manufacturer() ||
        t->a_field_version != expected_version_) {
        warning("(%s) expected telegram from SON meter with version 0x%02x, "
                "but got \"%s\" meter with version 0x%02x !\n", meter_name_,
                expected_version_,
                manufacturerFlag(t->m_field).c_str(),
                t->a_field_version);
                }*/

    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptKamstrupC1(t, aeskey);
    } else {
        t->content = t->payload;
    }
    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", "supercom587");
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        snprintf(log_prefix, 255, "(%s)", "supercom587");
        t->explainParse(log_prefix, content_start);
    }
    triggerUpdate(t);
}

void MeterSupercom587::processContent(Telegram *t)
{
}

void MeterSupercom587::printMeterHumanReadable(FILE *output)
{
    fprintf(output,
            "%s\t"
            "%s\t"
            "% 3.3f m3\t"
            "%s\n",
	    name().c_str(),
	    id().c_str(),
	    totalWaterConsumption(),
	    datetimeOfUpdateHumanReadable().c_str());
}

void MeterSupercom587::printMeterFields(FILE *output, char separator)
{
    fprintf(output,
            "%s%c"
            "%s%c"
            "%f%c"
            "%s\n",
            name().c_str(), separator,
            id().c_str(), separator,
            totalWaterConsumption(), separator,
            datetimeOfUpdateRobot().c_str());
}

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

void MeterSupercom587::printMeterJSON(FILE *output)
{
    fprintf(output, "{"
            QS(media,%s)
            QS(meter,supercom587)
            QS(name,%s)
            QS(id,%s)
            Q(total_m3,%f)
            QSE(timestamp,%s)
            "}\n",
            mediaType(manufacturer(), media()).c_str(),
            name().c_str(),
            id().c_str(),
            totalWaterConsumption(),
            datetimeOfUpdateRobot().c_str());
}

bool MeterSupercom587::hasTotalWaterConsumption()
{
    return true;
}

double MeterSupercom587::targetWaterConsumption()
{
    return 0.0;
}

bool MeterSupercom587::hasTargetWaterConsumption()
{
    return false;
}

double MeterSupercom587::maxFlow()
{
    return 0.0;
}

bool MeterSupercom587::hasMaxFlow()
{
    return false;
}

string MeterSupercom587::statusHumanReadable()
{
    return "";
}

string MeterSupercom587::status()
{
    return "";
}

string MeterSupercom587::timeDry()
{
    return "";
}

string MeterSupercom587::timeReversed()
{
    return "";
}

string MeterSupercom587::timeLeaking()
{
    return "";
}

string MeterSupercom587::timeBursting()
{
    return "";
}

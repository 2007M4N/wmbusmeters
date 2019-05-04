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

#ifndef METER_H_
#define METER_H_

#include"util.h"
#include"units.h"
#include"wmbus.h"

#include<string>
#include<vector>

#define LIST_OF_METERS \
    X(amiplus,    T1, Electricity, AMIPLUS,     Amiplus)      \
    X(apator162,  T1, Water,       APATOR162,   Apator162)    \
    X(flowiq3100, C1, Water,       FLOWIQ3100,  Multical21)   \
    X(iperl,      T1, Water,       IPERL,       Iperl)        \
    X(mkradio3,   T1, Water,       MKRADIO3,    MKRadio3)     \
    X(multical21, C1, Water,       MULTICAL21,  Multical21)   \
    X(multical302,C1, Heat,        MULTICAL302, Multical302)  \
    X(omnipower,  C1, Electricity, OMNIPOWER,   Omnipower)    \
    X(qcaloric,   C1, HeatCostAllocation, QCALORIC, QCaloric) \
    X(supercom587,T1, Water,       SUPERCOM587, Supercom587)  \
    X(vario451,   T1, Heat,        VARIO451,    Vario451)     \


enum class MeterType {
#define X(mname,linkmode,info,type,cname) type,
LIST_OF_METERS
#undef X
    UNKNOWN
};

using namespace std;

typedef unsigned char uchar;

struct Meter {
    virtual vector<string> ids() = 0;
    virtual string meterName() = 0;
    virtual string name() = 0;
    virtual MeterType type() = 0;
    virtual vector<int> media() = 0;
    virtual WMBus *bus() = 0;
    virtual LinkMode requiredLinkMode() = 0;

    virtual string datetimeOfUpdateHumanReadable() = 0;
    virtual string datetimeOfUpdateRobot() = 0;

    virtual void onUpdate(function<void(Telegram*t,Meter*)> cb) = 0;
    virtual int numUpdates() = 0;

    virtual void printMeter(Telegram *t,
                            string *human_readable,
                            string *fields, char separator,
                            string *json,
                            vector<string> *envs) = 0;

    virtual bool isTelegramForMe(Telegram *t) = 0;
    virtual bool useAes() = 0;
    virtual vector<uchar> key() = 0;

    // Dynamically access all data received for the meter.
    virtual std::vector<std::string> getRecords() = 0;
    virtual double getRecordAsDouble(std::string record) = 0;
    virtual uint16_t getRecordAsUInt16(std::string record) = 0;

    virtual ~Meter() = default;
};

struct WaterMeter : public virtual Meter {
    virtual double totalWaterConsumption() = 0; // m3
    virtual bool  hasTotalWaterConsumption() = 0;
    virtual double targetWaterConsumption() = 0; // m3
    virtual bool  hasTargetWaterConsumption() = 0;
    virtual double maxFlow() = 0;
    virtual bool  hasMaxFlow() = 0;
    virtual double flowTemperature() = 0; // °C
    virtual bool hasFlowTemperature() = 0;
    virtual double externalTemperature() = 0; // °C
    virtual bool hasExternalTemperature() = 0;

    virtual string statusHumanReadable() = 0;
    virtual string status() = 0;
    virtual string timeDry() = 0;
    virtual string timeReversed() = 0;
    virtual string timeLeaking() = 0;
    virtual string timeBursting() = 0;
};

struct HeatMeter : public virtual Meter {
    virtual double totalEnergyConsumption(Unit u) = 0; // kwh
    virtual double currentPeriodEnergyConsumption(Unit u) = 0; // kwh
    virtual double previousPeriodEnergyConsumption(Unit u) = 0; // kwh
    virtual double currentPowerConsumption(Unit u) = 0; // kw
    virtual double totalVolume(Unit u) = 0; // m3
};

struct ElectricityMeter : public virtual Meter {
    virtual double totalEnergyConsumption() = 0; // kwh
    virtual double currentPowerConsumption() = 0; // kw
    virtual double totalEnergyProduction() = 0; // kwh
    virtual double currentPowerProduction() = 0; // kw
};

struct HeatCostMeter : public virtual Meter {
    virtual double currentConsumption() = 0;
    virtual string setDate() = 0;
    virtual double consumptionAtSetDate() = 0;
};

struct GenericMeter : public virtual Meter {
};

string toMeterName(MeterType mt);
MeterType toMeterType(string& type);
LinkMode toMeterLinkMode(string& type);
unique_ptr<WaterMeter> createMultical21(WMBus *bus, string& name, string& id, string& key);
unique_ptr<WaterMeter> createFlowIQ3100(WMBus *bus, string& name, string& id, string& key);
unique_ptr<HeatMeter> createMultical302(WMBus *bus, string& name, string& id, string& key);
unique_ptr<HeatMeter> createVario451(WMBus *bus, string& name, string& id, string& key);
unique_ptr<ElectricityMeter> createOmnipower(WMBus *bus, string& name, string& id, string& key);
unique_ptr<ElectricityMeter> createAmiplus(WMBus *bus, string& name, string& id, string& key);
unique_ptr<WaterMeter> createSupercom587(WMBus *bus, string& name, string& id, string& key);
unique_ptr<WaterMeter> createMKRadio3(WMBus *bus, string& name, string& id, string& key);
unique_ptr<WaterMeter> createApator162(WMBus *bus, string& name, string& id, string& key);
unique_ptr<WaterMeter> createIperl(WMBus *bus, string& name, string& id, string& key);
unique_ptr<HeatCostMeter> createQCaloric(WMBus *bus, string& name, string& id, string& key);
GenericMeter *createGeneric(WMBus *bus, string& name, string& id, string& key);

#endif

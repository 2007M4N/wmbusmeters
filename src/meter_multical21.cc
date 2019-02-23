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

#define INFO_CODE_DRY 0x01
#define INFO_CODE_DRY_SHIFT (4+0)

#define INFO_CODE_REVERSE 0x02
#define INFO_CODE_REVERSE_SHIFT (4+3)

#define INFO_CODE_LEAK 0x04
#define INFO_CODE_LEAK_SHIFT (4+6)

#define INFO_CODE_BURST 0x08
#define INFO_CODE_BURST_SHIFT (4+9)

struct MeterMultical21 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterMultical21(WMBus *bus, string& name, string& id, string& key, MeterType mt);

    // Total water counted through the meter
    double totalWaterConsumption();
    bool  hasTotalWaterConsumption();

    // Meter sends target water consumption or max flow, depending on meter configuration
    // We can see which was sent inside the wmbus message!

    // Target water consumption: The total consumption at the start of the previous 30 day period.
    double targetWaterConsumption();
    bool  hasTargetWaterConsumption();
    // Max flow during last month or last 24 hours depending on meter configuration.
    double maxFlow();
    bool  hasMaxFlow();
    // Water temperature
    double flowTemperature();
    bool  hasFlowTemperature();
    // Surrounding temperature
    double externalTemperature();
    bool  hasExternalTemperature();

    // statusHumanReadable: DRY,REVERSED,LEAK,BURST if that status is detected right now, followed by
    //                      (dry 15-21 days) which means that, even it DRY is not active right now,
    //                      DRY has been active for 15-21 days during the last 30 days.
    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

    void printMeter(string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    uint16_t info_codes_ {};
    double total_water_consumption_ {};
    bool has_total_water_consumption_ {};
    double target_volume_ {};
    bool has_target_volume_ {};
    double max_flow_ {};
    bool has_max_flow_ {};
    double flow_temperature_ { 127 };
    bool has_flow_temperature_ {};
    double external_temperature_ { 127 };
    bool has_external_temperature_ {};

    const char *meter_name_; // multical21 or flowiq3100
    int expected_version_ {}; // 0x1b for Multical21 and 0x1d for FlowIQ3100
};

MeterMultical21::MeterMultical21(WMBus *bus, string& name, string& id, string& key, MeterType mt) :
    MeterCommonImplementation(bus, name, id, key, mt, MANUFACTURER_KAM, 0x16, LinkModeC1)
{
    if (type() == MULTICAL21_METER) {
        expected_version_ = 0x1b;
        meter_name_ = "multical21";
    } else if (type() == FLOWIQ3100_METER) {
        expected_version_ = 0x1d;
        meter_name_ = "flowiq3100";
    } else {
        assert(0);
    }
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MeterMultical21::totalWaterConsumption()
{
    return total_water_consumption_;
}

bool MeterMultical21::hasTotalWaterConsumption()
{
    return has_total_water_consumption_;
}

double MeterMultical21::targetWaterConsumption()
{
    return target_volume_;
}

bool MeterMultical21::hasTargetWaterConsumption()
{
    return has_target_volume_;
}

double MeterMultical21::maxFlow()
{
    return max_flow_;
}

bool MeterMultical21::hasMaxFlow()
{
    return has_max_flow_;
}

double MeterMultical21::flowTemperature()
{
    return flow_temperature_;
}

bool MeterMultical21::hasFlowTemperature()
{
    return has_flow_temperature_;
}

double MeterMultical21::externalTemperature()
{
    return external_temperature_;
}

bool MeterMultical21::hasExternalTemperature()
{
    return has_external_temperature_;
}

unique_ptr<WaterMeter> createMulticalWaterMeter(WMBus *bus, string& name, string& id, string& key, MeterType mt)
{
    if (mt != MULTICAL21_METER && mt != FLOWIQ3100_METER) {
        error("Internal error! Not a proper meter type when creating a multical21 style meter.\n");
    }
    return unique_ptr<WaterMeter>(new MeterMultical21(bus,name,id,key,mt));
}

unique_ptr<WaterMeter> createMultical21(WMBus *bus, string& name, string& id, string& key)
{
    return createMulticalWaterMeter(bus, name, id, key, MULTICAL21_METER);
}

unique_ptr<WaterMeter> createFlowIQ3100(WMBus *bus, string& name, string& id, string& key)
{
    return createMulticalWaterMeter(bus, name, id, key, FLOWIQ3100_METER);
}

void MeterMultical21::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) telegram for %s %02x%02x%02x%02x\n", meter_name_,
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x16) {
        warning("(%s) expected telegram for water media, but got \"%s\"!\n", meter_name_,
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    if (t->m_field != manufacturer() ||
        t->a_field_version != expected_version_) {
        warning("(%s) expected telegram from KAM meter with version 0x%02x, "
                "but got \"%s\" meter with version 0x%02x !\n", meter_name_,
                expected_version_,
                manufacturerFlag(t->m_field).c_str(),
                t->a_field_version);
    }

    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode1_AES_CTR(t, aeskey);
    } else {
        t->content = t->payload;
    }
    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", meter_name_);
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        snprintf(log_prefix, 255, "(%s)", meter_name_);
        t->explainParse(log_prefix, content_start);
    }
    triggerUpdate(t);
}

void MeterMultical21::processContent(Telegram *t)
{
    // 02 dif (16 Bit Integer/Binary Instantaneous value)
    // FF vif (Kamstrup extension)
    // 20 vife (?)
    // 7100 info codes (DRY(dry 22-31 days))
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 13 vif (Volume l)
    // F8180000 total consumption (6.392000 m3)
    // 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    // 13 vif (Volume l)
    // F4180000 target consumption (6.388000 m3)
    // 61 dif (8 Bit Integer/Binary Minimum value storagenr=1)
    // 5B vif (Flow temperature °C)
    // 7F flow temperature (127.000000 °C)
    // 61 dif (8 Bit Integer/Binary Minimum value storagenr=1)
    // 67 vif (External temperature °C)
    // 17 external temperature (23.000000 °C)

    // 02 dif (16 Bit Integer/Binary Instantaneous value)
    // FF vif (Kamstrup extension)
    // 20 vife (?)
    // 0000 info codes (OK)
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 13 vif (Volume l)
    // 2F4E0000 total consumption (20.015000 m3)
    // 92 dif (16 Bit Integer/Binary Maximum value)
    // 01 dife (subunit=0 tariff=0 storagenr=2)
    // 3B vif (Volume flow l/h)
    // 3D01 max flow (0.317000 m3/h)
    // A1 dif (8 Bit Integer/Binary Minimum value)
    // 01 dife (subunit=0 tariff=0 storagenr=2)
    // 5B vif (Flow temperature °C)
    // 02 flow temperature (2.000000 °C)
    // 81 dif (8 Bit Integer/Binary Instantaneous value)
    // 01 dife (subunit=0 tariff=0 storagenr=2)
    // E7 vif (External temperature °C)
    // FF vife (?)
    // 0F vife (?)
    // 03 external temperature (3.000000 °C)

    vector<uchar>::iterator bytes = t->content.begin();

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(bytes, 2, "%02x%02x payload crc", crc0, crc1);

    int frame_type = t->content[2];
    t->addExplanation(bytes, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    map<string,pair<int,DVEntry>> values;

    if (frame_type == 0x79)
    {
        // This is a "compact frame" in wmbus lingo.
        // (Other such frame_types are Ci=0x69, 0x6a, 0x6b and Ci=0x79, 0x7b, compact frames and format frames)
        // 0,1 = crc for format signature = hash over DRH (Data Record Header)
        // The DRH is the dif(difes)vif(vifes) bytes for all the records...
        // This hash is used to find the suitable format string, that has been previously
        // seen in a long frame telegram.
        uchar ecrc0 = t->content[3];
        uchar ecrc1 = t->content[4];
        t->addExplanation(bytes, 2, "%02x%02x format signature", ecrc0, ecrc1);
        uint16_t format_signature = ecrc1<<8 | ecrc0;

        vector<uchar> format_bytes;
        bool ok = loadFormatBytesFromSignature(format_signature, &format_bytes);
        if (!ok) {
            // We have not yet seen a long frame, but we know the formats for these
            // particular hashes:
            if (format_signature == 0xa8ed)
            {
                hex2bin("02FF2004134413615B6167", &format_bytes);
                debug("(%s) using hard coded format for hash a8ed\n", meter_name_);
            }
            else if (format_signature == 0xc412)
            {
                hex2bin("02FF20041392013BA1015B8101E7FF0F", &format_bytes);
                debug("(%s) using hard coded format for hash c412\n", meter_name_);
            }
            else
            {
                verbose("(%s) ignoring telegram since format signature hash 0x%02x is yet unknown.\n",
                        meter_name_,  format_signature);
                return;
            }
        }
        vector<uchar>::iterator format = format_bytes.begin();

        // 2,3 = crc for payload = hash over both DRH and data bytes. Or is it only over the data bytes?
        int ecrc2 = t->content[5];
        int ecrc3 = t->content[6];
        t->addExplanation(bytes, 2, "%02x%02x data crc", ecrc2, ecrc3);
        parseDV(t, t->content, t->content.begin()+7, t->content.size()-7, &values, &format, format_bytes.size());
    }
    else
    if (frame_type == 0x78)
    {
        parseDV(t, t->content, t->content.begin()+3, t->content.size()-3, &values);
    }
    else
    {
        warning("(%s) warning: unknown frame %02x (did you use the correct encryption key?)\n", meter_name_, frame_type);
        return;
    }

    int offset;
    string key;

    extractDVuint16(&values, "02FF20", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", statusHumanReadable().c_str());

    if(findKey(ValueInformation::Volume, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_water_consumption_);
        has_total_water_consumption_ = true;
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);
    }

    if(findKey(ValueInformation::Volume, 1, &key, &values)) {
        extractDVdouble(&values, key, &offset, &target_volume_);
        has_target_volume_ = true;
        t->addMoreExplanation(offset, " target consumption (%f m3)", target_volume_);
    }

    if(findKey(ValueInformation::VolumeFlow, ANY_STORAGENR, &key, &values)) {
        extractDVdouble(&values, key, &offset, &max_flow_);
        has_max_flow_ = true;
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_);
    }

    if(findKey(ValueInformation::FlowTemperature, ANY_STORAGENR, &key, &values)) {
        has_flow_temperature_ = extractDVdouble(&values, key, &offset, &flow_temperature_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_);
    }

    if(findKey(ValueInformation::ExternalTemperature, ANY_STORAGENR, &key, &values)) {
        has_external_temperature_ = extractDVdouble(&values, key, &offset, &external_temperature_);
        t->addMoreExplanation(offset, " external temperature (%f °C)", external_temperature_);
    }

}

string MeterMultical21::status()
{
    string s;
    if (info_codes_ & INFO_CODE_DRY) s.append("DRY ");
    if (info_codes_ & INFO_CODE_REVERSE) s.append("REVERSED ");
    if (info_codes_ & INFO_CODE_LEAK) s.append("LEAK ");
    if (info_codes_ & INFO_CODE_BURST) s.append("BURST ");
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}

string MeterMultical21::timeDry()
{
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (time_dry) {
        return decodeTime(time_dry);
    }
    return "";
}

string MeterMultical21::timeReversed()
{
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (time_reversed) {
        return decodeTime(time_reversed);
    }
    return "";
}

string MeterMultical21::timeLeaking()
{
    int time_leaking = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (time_leaking) {
        return decodeTime(time_leaking);
    }
    return "";
}

string MeterMultical21::timeBursting()
{
    int time_bursting = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (time_bursting) {
        return decodeTime(time_bursting);
    }
    return "";
}

string MeterMultical21::statusHumanReadable()
{
    string s;
    bool dry = info_codes_ & INFO_CODE_DRY;
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (dry || time_dry) {
        if (dry) s.append("DRY");
        s.append("(dry ");
        s.append(decodeTime(time_dry));
        s.append(") ");
    }

    bool reversed = info_codes_ & INFO_CODE_REVERSE;
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (reversed || time_reversed) {
        if (dry) s.append("REVERSED");
        s.append("(rev ");
        s.append(decodeTime(time_reversed));
        s.append(") ");
    }

    bool leak = info_codes_ & INFO_CODE_LEAK;
    int time_leak = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (leak || time_leak) {
        if (dry) s.append("LEAK");
        s.append("(leak ");
        s.append(decodeTime(time_leak));
        s.append(") ");
    }

    bool burst = info_codes_ & INFO_CODE_BURST;
    int time_burst = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (burst || time_burst) {
        if (dry) s.append("BURST");
        s.append("(burst ");
        s.append(decodeTime(time_burst));
        s.append(") ");
    }
    if (s.length() > 0) {
        s.pop_back();
        return s;
    }
    return "OK";
}

string MeterMultical21::decodeTime(int time)
{
    if (time>7) {
        warning("(%s) warning: Cannot decode time %d should be 0-7.\n", meter_name_, time);
    }
    switch (time) {
    case 0:
        return "0 hours";
    case 1:
        return "1-8 hours";
    case 2:
        return "9-24 hours";
    case 3:
        return "2-3 days";
    case 4:
        return "4-7 days";
    case 5:
        return "8-14 days";
    case 6:
        return "15-21 days";
    case 7:
        return "22-31 days";
    default:
        return "?";
    }
}

void MeterMultical21::printMeter(string *human_readable,
                                 string *fields, char separator,
                                 string *json,
                                 vector<string> *envs)
{
    char buf[65536];
    buf[65535] = 0;

    char ft[10], et[10];
    ft[9] = 0;
    et[9] = 0;

    if (hasFlowTemperature()) {
        snprintf(ft, sizeof(ft)-1, "% 2.0f", flowTemperature());
    } else {
        ft[0] = '-';
        ft[1] = 0;
    }

    if (hasExternalTemperature()) {
        snprintf(et, sizeof(et)-1, "% 2.0f", externalTemperature());
    } else {
        et[0] = '-';
        et[1] = 0;
    }

    snprintf(buf, sizeof(buf)-1, "%s\t%s\t% 3.3f m3\t% 3.3f m3\t% 3.3f m3/h\t%s°C\t%s°C\t%s\t%s",
             name().c_str(),
             id().c_str(),
             totalWaterConsumption(),
             targetWaterConsumption(),
             maxFlow(),
             ft,
             et,
             statusHumanReadable().c_str(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1, "%s%c" "%s%c" "%f%c" "%f%c" "%f%c" "%.0f%c" "%.0f%c" "%s%c" "%s",
             name().c_str(), separator,
             id().c_str(), separator,
             totalWaterConsumption(), separator,
             targetWaterConsumption(), separator,
             maxFlow(), separator,
             flowTemperature(), separator,
             externalTemperature(), separator,
             statusHumanReadable().c_str(), separator,
             datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,%s)
             QS(meter,%s)
             QS(name,%s)
             QS(id,%s)
             Q(total_m3,%f)
             Q(target_m3,%f)
             Q(max_flow_m3h,%f)
             Q(flow_temperature,%.0f)
             Q(external_temperature,%.0f)
             QS(current_status,%s)
             QS(time_dry,%s)
             QS(time_reversed,%s)
             QS(time_leaking,%s)
             QS(time_bursting,%s)
             QSE(timestamp,%s)
             "}",
             mediaType(manufacturer(), media()).c_str(),
             meter_name_,
             name().c_str(),
             id().c_str(),
             totalWaterConsumption(),
             targetWaterConsumption(),
             maxFlow(),
             flowTemperature(),
             externalTemperature(),
             status().c_str(), // DRY REVERSED LEAK BURST
             timeDry().c_str(),
             timeReversed().c_str(),
             timeLeaking().c_str(),
             timeBursting().c_str(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=")+meter_name_);
    envs->push_back(string("METER_ID=")+id());
    envs->push_back(string("METER_TOTAL_M3=")+to_string(totalWaterConsumption()));
    envs->push_back(string("METER_TARGET_M3=")+to_string(targetWaterConsumption()));
    envs->push_back(string("METER_MAX_FLOW_M3H=")+to_string(maxFlow()));
    envs->push_back(string("METER_FLOW_TEMPERATURE=")+to_string(flowTemperature()));
    envs->push_back(string("METER_EXTERNAL_TEMPERATURE=")+to_string(externalTemperature()));
    envs->push_back(string("METER_STATUS=")+status());
    envs->push_back(string("METER_TIME_DRY=")+timeDry());
    envs->push_back(string("METER_TIME_REVERSED=")+timeReversed());
    envs->push_back(string("METER_TIME_LEAKING=")+timeLeaking());
    envs->push_back(string("METER_TIME_BURSTING=")+timeBursting());
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}

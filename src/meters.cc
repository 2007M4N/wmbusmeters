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

#include"meters.h"
#include"meters_common_implementation.h"

#include<memory.h>

MeterCommonImplementation::MeterCommonImplementation(WMBus *bus, string& name, string& id, string& key,
                                                     MeterType type, int manufacturer,
                                                     LinkMode required_link_mode) :
    type_(type), name_(name), bus_(bus),
    required_link_mode_(required_link_mode)
{
    use_aes_ = true;
    ids_ = splitIds(id);
    if (key.length() == 0) {
        use_aes_ = false;
    } else {
        hex2bin(key, &key_);
    }
    if (manufacturer) {
        manufacturers_.insert(manufacturer);
    }
}

MeterType MeterCommonImplementation::type()
{
    return type_;
}

vector<int> MeterCommonImplementation::media()
{
    return media_;
}

void MeterCommonImplementation::addMedia(int m)
{
    media_.push_back(m);
}

void MeterCommonImplementation::addManufacturer(int m)
{
    manufacturers_.insert(m);
}

vector<string> MeterCommonImplementation::ids()
{
    return ids_;
}

string MeterCommonImplementation::name()
{
    return name_;
}

WMBus *MeterCommonImplementation::bus()
{
    return bus_;
}

LinkMode MeterCommonImplementation::requiredLinkMode()
{
    return required_link_mode_;
}

void MeterCommonImplementation::onUpdate(function<void(Telegram*,Meter*)> cb)
{
    on_update_.push_back(cb);
}

int MeterCommonImplementation::numUpdates()
{
    return num_updates_;
}

string MeterCommonImplementation::datetimeOfUpdateHumanReadable()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M.%S", localtime(&datetime_of_update_));
    return string(datetime);
}

string MeterCommonImplementation::datetimeOfUpdateRobot()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    // This is the date time in the Greenwich timezone (Zulu time), dont get surprised!
    strftime(datetime, sizeof(datetime), "%FT%TZ", gmtime(&datetime_of_update_));
    return string(datetime);
}

MeterType toMeterType(string& type)
{
    if (type == "multical21") return MULTICAL21_METER;
    if (type == "flowiq3100") return FLOWIQ3100_METER;
    if (type == "multical302") return MULTICAL302_METER;
    if (type == "omnipower") return OMNIPOWER_METER;
    if (type == "amiplus") return AMIPLUS_METER;
    if (type == "supercom587") return SUPERCOM587_METER;
    if (type == "iperl") return IPERL_METER;
    if (type == "qcaloric") return QCALORIC_METER;
    if (type == "apator162") return APATOR162_METER;
    if (type == "mkradio3") return MKRADIO3_METER;
    return UNKNOWN_METER;
}

LinkMode toMeterLinkMode(string& type)
{
    if (type == "multical21") return LinkMode::C1;
    if (type == "flowiq3100") return LinkMode::C1;
    if (type == "multical302") return LinkMode::C1;
    if (type == "omnipower") return LinkMode::C1;
    if (type == "amiplus") return LinkMode::T1;
    if (type == "supercom587") return LinkMode::T1;
    if (type == "iperl") return LinkMode::T1;
    if (type == "qcaloric") return LinkMode::C1;
    if (type == "apator162") return LinkMode::T1;
    if (type == "mkradio3") return LinkMode::T1;

    return LinkMode::UNKNOWN;
}


bool MeterCommonImplementation::isTelegramForMe(Telegram *t)
{
    debug("(meter) %s: for me? %s\n", name_.c_str(), t->id.c_str());

    bool id_match = false;
    for (auto id : ids_)
    {
        if (id == t->id) {
            id_match = true;
            break;
        }
        if (id == "*") {
            id_match = true;
            break;
        }
    }

    if (!id_match) {
        // The id must match.
        debug("(meter) %s: not for me: not my id\n", name_.c_str());
        return false;
    }

    if (manufacturers_.count(t->m_field) == 0) {
        // We are not that strict for the manufacturer.
        // Simply warn.
        warning("(meter) %s: probably not for me since manufacturer differs\n", name_.c_str());
    }

    bool media_match = false;
    for (auto m : media_) {
        if (m == t->a_field_device_type) {
            media_match = true;
            break;
        }
    }

    if (!media_match) {
        warning("(meter) %s: probably not for me since media does not match\n", name_.c_str());
    }

    debug("(meter) %s: yes for me\n", name_.c_str());
    return true;
}

bool MeterCommonImplementation::useAes()
{
    return use_aes_;
}

vector<uchar> MeterCommonImplementation::key()
{
    return key_;
}

vector<string> MeterCommonImplementation::getRecords()
{
    vector<string> recs;
    for (auto& p : values_)
    {
        recs.push_back(p.first);
    }
    return recs;
}

double MeterCommonImplementation::getRecordAsDouble(string record)
{
    return 0.0;
}

uint16_t MeterCommonImplementation::getRecordAsUInt16(string record)
{
    return 0;
}

void MeterCommonImplementation::triggerUpdate(Telegram *t)
{
    datetime_of_update_ = time(NULL);
    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(t, this);
    t->handled = true;
}

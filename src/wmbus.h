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

#ifndef WMBUS_H
#define WMBUS_H

#include"manufacturers.h"
#include"serial.h"
#include"util.h"

#include<inttypes.h>
#include<map>

#define LIST_OF_LINK_MODES \
    X(Any,any,--anylinkmode,0xffff)             \
    X(C1,c1,--c1,0x1)                           \
    X(S1,s1,--s1,0x2)                           \
    X(S1m,s1m,--s1m,0x4)                       \
    X(T1,t1,--t1,0x8)                          \
    X(N1a,n1a,--n1a,0x10)                      \
    X(N1b,n1b,--n1b,0x20)                      \
    X(N1c,n1c,--n1c,0x40)                      \
    X(N1d,n1d,--n1d,0x80)                      \
    X(N1e,n1e,--n1e,0x100)                     \
    X(N1f,n1f,--n1f,0x200)                     \
    X(UNKNOWN,unknown,----,0x0)

// In link mode T1, the meter transmits a telegram every few seconds or minutes.
// Suitable for drive-by/walk-by collection of meter values.

// Link mode C1 is like T1 but uses less energy when transmitting due to
// a different radio encoding.

enum class LinkMode {
#define X(name,lcname,option,val) name,
LIST_OF_LINK_MODES
#undef X
};

enum LinkModeBits {
#define X(name,lcname,option,val) name##_bit = val,
LIST_OF_LINK_MODES
#undef X
};

LinkMode isLinkMode(const char *arg);
LinkMode isLinkModeOption(const char *arg);

struct LinkModeSet
{
    // Add the link mode to the set of link modes.
    void addLinkMode(LinkMode lm);
    void unionLinkModeSet(LinkModeSet lms);
    void disjunctionLinkModeSet(LinkModeSet lms);
    // Does this set support listening to the given link mode set?
    // If this set is C1 and T1 and the supplied set contains just C1,
    // then supports returns true.
    // Or if this set is just T1 and the supplied set contains just C1,
    // then supports returns false.
    // Or if this set is just C1 and the supplied set contains C1 and T1,
    // then supports returns true.
    // Or if this set is S1 and T1, and the supplied set contains C1 and T1,
    // then supports returns true.
    //
    // It will do a bitwise and of the linkmode bits. If the result
    // of the and is not zero, then support returns true.
    bool supports(LinkModeSet lms);
    // Check if this set contains the given link mode.
    bool has(LinkMode lm);
    // Check if all link modes are supported.
    bool hasAll(LinkModeSet lms);

    int bits() { return set_; }

    // Return a human readable string.
    std::string hr();

    LinkModeSet() { }
    LinkModeSet(int s) : set_(s) {}

private:

    int set_ {};
};

LinkModeSet parseLinkModes(string modes);

enum class CONNECTION
{
    MBUS, WMBUS, BOTH
};

enum class CI_TYPE
{
    ELL, NWL, AFL, TPL
};

enum class TPL_LENGTH
{
    NONE, SHORT, LONG
};

#define CC_B_BIDIRECTIONAL_BIT 0x80
#define CC_RD_RESPONSE_DELAY_BIT 0x40
#define CC_S_SYNCH_FRAME_BIT 0x20
#define CC_R_RELAYED_BIT 0x10
#define CC_P_HIGH_PRIO_BIT 0x08

// Bits 31-29 in SN, ie 0xc0 of the final byte in the stream,
// since the bytes arrive with the least significant first
// aka little endian.
#define SN_ENC_BITS 0xc0

#define LIST_OF_ELL_SECURITY_MODES \
    X(NoSecurity, 0) \
    X(AES_CTR, 1) \
    X(RESERVED, 2)

enum class ELLSecurityMode {
#define X(name,nr) name,
LIST_OF_ELL_SECURITY_MODES
#undef X
};

int toInt(ELLSecurityMode esm);
const char *toString(ELLSecurityMode esm);
ELLSecurityMode fromIntToELLSecurityMode(int i);

#define LIST_OF_TPL_SECURITY_MODES \
    X(NoSecurity, 0) \
    X(MFCT_SPECIFIC, 1) \
    X(DES_NO_IV_DEPRECATED, 2) \
    X(DES_IV_DEPRECATED, 3) \
    X(SPECIFIC_4, 4) \
    X(AES_CBC_IV, 5) \
    X(RESERVED_6, 6) \
    X(AES_CBC_NO_IV, 7) \
    X(AES_CTR_CMAC, 8) \
    X(AES_CGM, 9) \
    X(AES_CCM, 10) \
    X(RESERVED_11, 11) \
    X(RESERVED_12, 12) \
    X(SPECIFIC_13, 13) \
    X(RESERVED_14, 14) \
    X(SPECIFIC_15, 15) \
    X(SPECIFIC_16_31, 16)

enum class TPLSecurityMode {
#define X(name,nr) name,
LIST_OF_TPL_SECURITY_MODES
#undef X
};

int toInt(TPLSecurityMode tsm);
TPLSecurityMode fromIntToTPLSecurityMode(int i);
const char *toString(TPLSecurityMode tsm);

enum class MeasurementType
{
    Unknown,
    Instantaneous,
    Minimum,
    Maximum,
    AtError
};

struct DVEntry
{
    MeasurementType type {};
    int value_information {};
    int storagenr {};
    int tariff {};
    int subunit {};
    string value;

    DVEntry() {}
    DVEntry(MeasurementType mt, int vi, int st, int ta, int su, string &val) :
    type(mt), value_information(vi), storagenr(st), tariff(ta), subunit(su), value(val) {}
};

using namespace std;

struct MeterKeys
{
    vector<uchar> confidentiality_key;
    vector<uchar> authentication_key;

    bool hasConfidentialityKey() { return confidentiality_key.size() > 0; }
    bool hasAuthenticationKey() { return authentication_key.size() > 0; }
};

struct Telegram
{
    // DLL
    int dll_len {}; // The length of the telegram, 1 byte.
    int dll_c {};   // 1 byte
    int dll_mft {}; // Manufacturer 2 bytes
    vector<uchar> dll_a; // A field 6 bytes
    // The 6 a field bytes are composed of:
    vector<uchar> a_field_address; // Address in BCD = 8 decimal 00000000...99999999 digits.
    string id; // the address as a string.
    int dll_version {}; // 1 byte
    int dll_type {}; // 1 byte

    // ELL
    uchar ell_ci {}; // 1 byte
    uchar ell_cc {}; // 1 byte
    uchar ell_acc {}; // 1 byte
    uchar ell_sn_b[4] {}; // 4 bytes
    int   ell_sn {}; // 4 bytes
    uchar ell_sn_session {}; // 4 bits
    int   ell_sn_time {}; // 25 bits
    uchar ell_sn_sec {}; // 3 bits
    ELLSecurityMode ell_sec_mode {}; // Based on 3 bits from above.
    uchar ell_pl_crc_b[2] {}; // 2 bytes
    uint16_t ell_pl_crc {}; // 2 bytes

    uchar ell_mfct_b[2] {}; // 2 bytes;
    uchar ell_addr_b[6] {}; // 6 bytes;

    // NWL
    int nwl_ci {}; // 1 byte

    // AFL
    int afl_ci {}; // 1 byte

    // TPL
    int tpl_ci {}; // 1 byte
    int tpl_acc {}; // 1 byte
    int tpl_sts {}; // 1 byte
    int tpl_cfg {}; // 2 bytes
    TPLSecurityMode tpl_sec_mode {}; // Based on 5 bits extracted from cfg.

    uchar tpl_id_b[4]; // 4 bytes
    uchar tpl_mft_b[2]; // 2 bytes
    uchar tpl_version; // 1 bytes
    uchar tpl_type; // 1 bytes

    // The format signature is used for compact frames.
    int format_signature {};

    vector<uchar> frame; // Content of frame, potentially decrypted.
    vector<uchar> parsed;  // Parsed bytes with explanations.
    int header_size {}; // Size of headers before the APL content.
    int suffix_size {}; // Size of suffix after the APL content. Usually empty, but can be MACs!
    void extractFrame(vector<uchar> *fr); // Extract to full frame.
    void extractPayload(vector<uchar> *pl); // Extract frame data containing the measurements, after the header and not the suffix.

    bool handled {}; // Set to true, when a meter has accepted the telegram.

    bool parseHeader(vector<uchar> &input_frame);
    bool parse(vector<uchar> &input_frame, MeterKeys *mk);
    void print();
    void verboseFields();

    // A vector of indentations and explanations, to be printed
    // below the raw data bytes to explain the telegram content.
    vector<pair<int,string>> explanations;
    void addExplanationAndIncrementPos(vector<uchar>::iterator &pos, int len, const char* fmt, ...);
    void addMoreExplanation(int pos, const char* fmt, ...);
    void explainParse(string intro, int from);

    bool isSimulated() { return is_simulated_; }

    void expectVersion(const char *info, int v);

    // Extracted mbus values.
    std::map<std::string,std::pair<int,DVEntry>> values;

private:

    bool is_simulated_ {};

    bool parseDLL(std::vector<uchar>::iterator &pos);
    bool parseELL(std::vector<uchar>::iterator &pos, MeterKeys *meter_keys);
    bool parseNWL(std::vector<uchar>::iterator &pos);
    bool parseAFL(std::vector<uchar>::iterator &pos);
    bool parseTPL(std::vector<uchar>::iterator &pos, MeterKeys *meter_keys);

    void printDLL();
    void printELL();
    void printNWL();
    void printAFL();
    void printTPL();

    bool parse_TPL_72(vector<uchar>::iterator &pos);
    bool parse_TPL_78(vector<uchar>::iterator &pos);
    bool parse_TPL_79(vector<uchar>::iterator &pos);
    bool parse_TPL_7A(vector<uchar>::iterator &pos, MeterKeys *meter_keys);

    bool parseTPLConfig(std::vector<uchar>::iterator &pos);
    static string toStringFromELLSN(int sn);
    static string toStringFromTPLConfig(int cfg);
    bool parseShortTPL(std::vector<uchar>::iterator &pos);
    bool parseLongTPL(std::vector<uchar>::iterator &pos);

    bool findFormatBytesFromKnownMeterSignatures(std::vector<uchar> *format_bytes);
};

struct Meter;

struct WMBus {
    virtual bool ping() = 0;
    virtual uint32_t getDeviceId() = 0;
    virtual LinkModeSet getLinkModes() = 0;
    virtual LinkModeSet supportedLinkModes() = 0;
    virtual int numConcurrentLinkModes() = 0;
    virtual bool canSetLinkModes(LinkModeSet lms) = 0;
    virtual void setMeters(vector<unique_ptr<Meter>> *meters) = 0;
    virtual void setLinkModes(LinkModeSet lms) = 0;
    virtual void onTelegram(function<bool(vector<uchar>)> cb) = 0;
    virtual SerialDevice *serial() = 0;
    virtual void simulate() = 0;
    virtual ~WMBus() = 0;
};

#define LIST_OF_MBUS_DEVICES X(DEVICE_CUL)X(DEVICE_D1TC)X(DEVICE_IM871A)X(DEVICE_AMB8465)X(DEVICE_RFMRX2)X(DEVICE_SIMULATOR)X(DEVICE_RTLWMBUS)X(DEVICE_RAWTTY)X(DEVICE_UNKNOWN)

enum MBusDeviceType {
#define X(name) name,
LIST_OF_MBUS_DEVICES
#undef X
};

struct Detected
{
    MBusDeviceType type;  // IM871A, AMB8465 etc
    string devicefile;    // /dev/ttyUSB0 /dev/ttyACM0 stdin simulation_abc.txt telegrams.raw
    int baudrate;         // If the suffix is a number, store the number here.
    // If the override_tty is true, then do not allow the wmbus driver to open the tty,
    // instead open the devicefile first. This is to allow feeding the wmbus drivers using stdin
    // or a file or for internal testing.
    bool override_tty;
};

Detected detectWMBusDeviceSetting(string devicefile, string suffix,
                                  SerialCommunicationManager *manager);

unique_ptr<WMBus> openIM871A(string device, SerialCommunicationManager *manager,
                             unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openAMB8465(string device, SerialCommunicationManager *manager,
                              unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openRawTTY(string device, int baudrate, SerialCommunicationManager *manager,
                             unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openRTLWMBUS(string device, SerialCommunicationManager *manager, std::function<void()> on_exit,
                               unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openCUL(string device, SerialCommunicationManager *manager,
                              unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openD1TC(string device, SerialCommunicationManager *manager,
                           unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openSimulator(string file, SerialCommunicationManager *manager,
                                unique_ptr<SerialDevice> serial_override);

string manufacturer(int m_field);
string manufacturerFlag(int m_field);
string mediaType(int a_field_device_type);
string mediaTypeJSON(int a_field_device_type);
bool isCiFieldOfType(int ci_field, CI_TYPE type);
int ciFieldLength(int ci_field);
string ciType(int ci_field);
string cType(int c_field);
string ccType(int cc_field);
string difType(int dif);
double vifScale(int vif);
string vifKey(int vif); // E.g. temperature energy power mass_flow volume_flow
string vifUnit(int vif); // E.g. m3 c kwh kw MJ MJh
string vifType(int vif); // Long description
string vifeType(int dif, int vif, int vife); // Long description
string formatData(int dif, int vif, int vife, string data);

double extract8bitAsDouble(int dif, int vif, int vife, string data);
double extract16bitAsDouble(int dif, int vif, int vife, string data);
double extract32bitAsDouble(int dif, int vif, int vife, string data);

int difLenBytes(int dif);
MeasurementType difMeasurementType(int dif);

string linkModeName(LinkMode link_mode);
string measurementTypeName(MeasurementType mt);

#endif

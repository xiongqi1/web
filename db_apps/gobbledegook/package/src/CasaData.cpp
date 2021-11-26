
#include "CasaData.h"
#include "Housekeeping.h"
#include "Logger.h"
#include "Gobbledegook.h"

// JSON library from https://github.com/nlohmann/json
#include "json.hpp"

#include <fstream>

// for convenience
using json = nlohmann::json;

//
// Server data values
//

static std::string deviceVersion = "";
static std::string modelNumString = "cfw1314";
static uint8_t deviceFamily = 0;
static uint8_t deviceState=0;
static std::string deviceError = "";
static std::string simApnDetails = "";
static std::string connectivityDetails = "";
static std::string ipAddresses = "";
static std::string tr069Status = "";
static std::string supportedBands = "";
static std::string bandLock = "";
static std::string gpsMagneticData = "";
static std::string batteryData = "";

static std::string connectionParameters = "";
static std::string connectedRat = "";
static uint8_t connectionStatusCode=0;
static uint8_t connectionAntennaBars=0;

static std::string visibleCellList = "";
static std::string visibleCellRequest = "";
static std::string visibleCellInfo = "";

// set visible cell info notification.
static std::string visibleCellInfoNoti = "";

static std::string ledColour = "";
static std::string ledFlash = "";

static std::string bleRssi = "";

static uint8_t speedTestRequest = 0;
static std::string speedTestResult = "";


// Basic utility for generating very poor quality random numbers
float randMToN(float M, float N)
{
    return M + (rand() / ( RAND_MAX / (N-M) ) ) ;
}


//
// Server data management
//

// Called by the server when it wants to retrieve a named value
//
// This method conforms to `GGKServerDataGetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.
const void *dataGetter(const char *pName)
{
    if (nullptr == pName)
    {
        LogError("NULL name sent to server data getter");
        return nullptr;
    }

    std::string strName = pName;

    //
    // Device Information Service
    //
    
    // Device Version Characteristic
    if (strName == "deviceInformationService/deviceVersionCharacteristic") return deviceVersion.c_str();

    // Device Version Characteristic
    else if (strName == "device/model_num") return modelNumString.c_str();

    // Device Family Characteristic
    else if (strName == "deviceInformationService/deviceFamilyCharacteristic") return &deviceFamily;

    // Device State Characteristic
    else if (strName == "deviceInformationService/stateCharacteristic") return &deviceState;
    
    // Device Error Message Characteristic
    else if (strName == "deviceInformationService/errorCharacteristic") return deviceError.c_str();
    
    // SIM and APN Details Characteristic
    else if (strName == "deviceInformationService/simApnCharacteristic") return simApnDetails.c_str();
    
    // Connectivity Characteristic
    else if (strName == "deviceInformationService/connectivityCharacteristic") return connectivityDetails.c_str();
    
    // IP Addresses Characteristic
    else if (strName == "deviceInformationService/ipAddressesCharacteristic") return ipAddresses.c_str();
    
    // TR069 Status Characteristic
    else if (strName == "deviceInformationService/tr069StatusCharacteristic") return tr069Status.c_str();

    // Supported Bands Characteristic
    else if (strName == "deviceInformationService/supportedBandsCharacteristic") return supportedBands.c_str();
    
    // BandLock Characteristic (NB: writable by client, we set modem based on this)
    else if (strName == "deviceInformationService/bandLockCharacteristic") return bandLock.c_str();
    
    // GPS and Magnetic Data Characteristic
    else if (strName == "deviceInformationService/gpsMagneticCharacteristic") return gpsMagneticData.c_str();
    
    // Battery Characteristic
    else if (strName == "deviceInformationService/batteryCharacteristic") return batteryData.c_str();
    
    //
    // Current Connection Service
    //
    
    // Current Connection Details Characteristic
    else if (strName == "currentConnectionService/connectionParametersCharacteristic") return connectionParameters.c_str();
    
    // Connected RAT Characteristic
    else if (strName == "currentConnectionService/connectedRatCharacteristic") return connectedRat.c_str();
    
    // Connection Status Code Characteristic
    else if (strName == "currentConnectionService/connectionStatusCodeCharacteristic") return &connectionStatusCode;
    
    // Antenna Bars Characteristic
    else if (strName == "currentConnectionService/antennaBarsCharacteristic") return &connectionAntennaBars;
    
    
    //
    // Visible Cell Service
    //
    
    // Visible Cell List Characteristic
    else if (strName == "visibleCellService/visibleCellListCharacteristic") return visibleCellList.c_str();
    
    // Cell Detail Request Characteristic (NB: writable by client, we set requested cell characteristic based on this)
    else if (strName == "visibleCellService/visibleCellRequestCharacteristic") return visibleCellRequest.c_str();
    
    // Cell Detail Characteristic
    else if (strName == "visibleCellService/visibleCellInfoCharacteristic") return visibleCellInfo.c_str();
    
    //
    // LED Service
    //
    
    // LED Colour Characteristic
    else if (strName == "ledService/ledColourCharacteristic") return ledColour.c_str();
    
    // LED Flashing Characteristic
    else if (strName == "ledService/ledFlashCharacteristic") return ledFlash.c_str();
    
    //
    // RSSI Service
    //
    
    // BLE RSSI Characteristic
    else if (strName == "rssiService/rssiCharacteristic") return bleRssi.c_str();
    
    //
    // SpeedTest Service
    //
    
    // SpeedTest Request Characteristic (NB: writable by client, we set results if request is newly written to)
    else if (strName == "speedTestService/speedTestRequestCharacteristic") return &speedTestRequest;
    
    // SpeedTest Result Characteristic (only if request has been newly written to)
    else if (strName == "speedTestService/speedTestResultCharacteristic") return speedTestResult.c_str();
    
    LogWarn((std::string("Unknown name for server data getter request: '") + pName + "'").c_str());
    return nullptr;
}

// Called by the server when it wants to update a named value
//
// This method conforms to `GGKServerDataSetter` and is passed to the server via our call to `ggkStart()`.
//
// The server calls this method from its own thread, so we must ensure our implementation is thread-safe. In our case, we're simply
// sending over stored values, so we don't need to take any additional steps to ensure thread-safety.

//
// TODO - sanity checking on inputs of wrong type, and test it
//

int dataSetter(const char *pName, const void *pData)
{
    if (nullptr == pName)
    {
        LogError("NULL name sent to server data setter");
        return 0;
    }
    if (nullptr == pData)
    {
        LogError("NULL pData sent to server data setter");
        return 0;
    }

    std::string strName = pName;

    //
    // Device Information Service
    //
    
    // Device Version Characteristic
    if (strName == "deviceInformationService/deviceVersionCharacteristic")
    {
        deviceVersion = static_cast<const char *>(pData);
        LogDebug((std::string("Device Version: text string set to '") + deviceVersion + "'").c_str());
        return 1;
    }

    // Model Number String Characteristic
    else if (strName == "device/model_num")
    {
        modelNumString = static_cast<const char *>(pData);
        LogDebug((std::string("Model Number String: text string set to '") + modelNumString + "'").c_str());
        return 1;
    }

    // Device Family Characteristic
    if (strName == "deviceInformationService/deviceFamilyCharacteristic")
    {
        deviceFamily = *static_cast<const uint8_t *>(pData);
        LogDebug((std::string("Device Family: uint8_t set to '") + std::to_string(deviceFamily) + "'").c_str());
        return 1;
    }

    // Device State Characteristic
    else if (strName == "deviceInformationService/stateCharacteristic")
    {
        deviceState = *static_cast<const uint8_t *>(pData);
        LogDebug((std::string("State: uint8_t set to '") + std::to_string(deviceState) + "'").c_str());
        return 1;
    }
    
    // Device Error Message Characteristic
    if (strName == "deviceInformationService/errorCharacteristic")
    {
        deviceError = static_cast<const char *>(pData);
        LogDebug((std::string("Device Error: text string set to '") + deviceError + "'").c_str());
        return 1;
    }
    
    // SIM and APN Details Characteristic
    if (strName == "deviceInformationService/simApnCharacteristic")
    {
        simApnDetails = static_cast<const char *>(pData);
        LogDebug((std::string("SIM and APN: text string set to '") + simApnDetails + "'").c_str());
        return 1;
    }
    
    // Connectivity Characteristic
    if (strName == "deviceInformationService/connectivityCharacteristic")
    {
        connectivityDetails = static_cast<const char *>(pData);
        LogDebug((std::string("Connectivity: text string set to '") + connectivityDetails + "'").c_str());
        return 1;
    }
    
    // IP Addresses Characteristic
    if (strName == "deviceInformationService/ipAddressesCharacteristic")
    {
        ipAddresses = static_cast<const char *>(pData);
        LogDebug((std::string("IP Addresses: text string set to '") + ipAddresses + "'").c_str());
        return 1;
    }
    
    // TR069 Status Characteristic
    if (strName == "deviceInformationService/tr069StatusCharacteristic")
    {
        tr069Status = static_cast<const char *>(pData);
        LogDebug((std::string("TR069: text string set to '") + tr069Status + "'").c_str());
        return 1;
    }

    // Supported Bands Characteristic
    if (strName == "deviceInformationService/supportedBandsCharacteristic")
    {
        supportedBands = static_cast<const char *>(pData);
        LogDebug((std::string("Supported Bands: text string set to '") + supportedBands + "'").c_str());
        return 1;
    }

    // BandLock Characteristic (NB: writable by client, we set modem based on this)
    if (strName == "deviceInformationService/bandLockCharacteristic")
    {
        bandLock = static_cast<const char *>(pData);
        LogDebug((std::string("Bandlock: text string set to '") + bandLock + "'").c_str());
        return 1;
    }
    
    // GPS and Magnetic Data Characteristic
    if (strName == "deviceInformationService/gpsMagneticCharacteristic")
    {
        gpsMagneticData = static_cast<const char *>(pData);
        LogDebug((std::string("GPS & Magnetic: text string set to '") + gpsMagneticData + "'").c_str());
        return 1;
    }
    
    // Battery Characteristic
    if (strName == "deviceInformationService/batteryCharacteristic")
    {
        batteryData = static_cast<const char *>(pData);
        LogDebug((std::string("Battery: text string set to '") + batteryData + "'").c_str());
        return 1;
    }
    
    //
    // Current Connection Service
    //
    
    // Current Connection Details Characteristic
    if (strName == "currentConnectionService/connectionParametersCharacteristic")
    {
        connectionParameters = static_cast<const char *>(pData);
        LogDebug((std::string("Connection Parameters: text string set to '") + connectionParameters + "'").c_str());
        return 1;
    }
    
    // Connected RAT Characteristic
    if (strName == "currentConnectionService/connectedRatCharacteristic")
    {
        connectedRat = static_cast<const char *>(pData);
        LogDebug((std::string("Connected RAT: text string set to '") + connectedRat + "'").c_str());
        return 1;
    }
    
    // Connection Status Code Characteristic
    if (strName == "currentConnectionService/connectionStatusCodeCharacteristic")
    {
        connectionStatusCode = *static_cast<const uint8_t *>(pData);
        LogDebug((std::string("Connection Status code: uint8_t set to '") + std::to_string(connectionStatusCode) + "'").c_str());
        return 1;
    }
    
    // Antenna Bars Characteristic
    if (strName == "currentConnectionService/antennaBarsCharacteristic")
    {
        connectionAntennaBars = *static_cast<const uint8_t *>(pData);
        LogDebug((std::string("Antenna Bars: uint8_t set to '") + std::to_string(connectionAntennaBars) + "'").c_str());
        return 1;
    }
    
    //
    // Visible Cell Service
    //
    
    // Visible Cell List Characteristic
    if (strName == "visibleCellService/visibleCellListCharacteristic")
    {
        visibleCellList = static_cast<const char *>(pData);
        LogDebug((std::string("Visible Cell List: text string set to '") + visibleCellList + "'").c_str());
        return 1;
    }
    
    // Cell Detail Request Characteristic (NB: writable by client, we set requested cell characteristic based on this)
    if (strName == "visibleCellService/visibleCellRequestCharacteristic")
    {
        visibleCellRequest = static_cast<const char *>(pData);
        LogDebug((std::string("Visible Cell Request: text string set to '") + visibleCellRequest + "'").c_str());
        return 1;
    }
    
    // Cell Detail Characteristic
    if (strName == "visibleCellService/visibleCellInfoCharacteristic")
    {
        visibleCellInfo = static_cast<const char *>(pData);
        LogDebug((std::string("Visible Cell Info: text string set to '") + visibleCellInfo + "'").c_str());
        return 1;
    }
    
    //
    // LED Service
    //
    
    // LED Colour Characteristic
    if (strName == "ledService/ledColourCharacteristic")
    {
        ledColour = static_cast<const char *>(pData);
        LogDebug((std::string("LED Colour: text string set to '") + ledColour + "'").c_str());
        return 1;
    }
    
    // LED Flashing Characteristic
    if (strName == "ledService/ledFlashCharacteristic")
    {
        ledFlash = static_cast<const char *>(pData);
        LogDebug((std::string("LED Flash: text string set to '") + ledFlash + "'").c_str());
        return 1;
    }
    
    //
    // RSSI Service
    //
    
    // BLE RSSI Characteristic
    if (strName == "rssiService/rssiCharacteristic")
    {
        bleRssi = static_cast<const char *>(pData);
        LogDebug((std::string("BLE RSSI: text string set to '") + bleRssi + "'").c_str());
        return 1;
    }
    
    //
    // SpeedTest Service
    //
    
    // SpeedTest Request Characteristic (NB: writable by client, we set results if request is newly written to)
    if (strName == "speedTestService/speedTestRequestCharacteristic")
    {
        speedTestRequest = *static_cast<const uint8_t *>(pData);
        LogDebug((std::string("Speed Test Request: text string set to '") + std::to_string(speedTestRequest) + "'").c_str());
        return 1;
    }
    
    // SpeedTest Result Characteristic (only if request has been newly written to)
    if (strName == "speedTestService/speedTestResultCharacteristic")
    {
        speedTestResult = static_cast<const char *>(pData);
        LogDebug((std::string("Speed Test Result: text string set to '") + speedTestResult + "'").c_str());
        return 1;
    }
    
    LogWarn((std::string("Unknown name for server data setter request: '") + pName + "'").c_str());

    return 0;
}

//
// Characteristic formatting functions - this is used by randomiser and real data sources alike
//

//
// Device Information Service
//

// Device Version Characteristic
void setDeviceVersion(std::string hardwareVersion, std::string softwareVersion, std::string gattVersion)
{
    json j;
    j["Hardware"] = hardwareVersion;
    j["Software"] = softwareVersion;
    j["GATT"] = gattVersion;
    
    deviceVersion = j.dump();
    LogInfo(("Device Version is " + deviceVersion).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/deviceVersionCharacteristic");
}
// Device Family Characteristic
void setDeviceFamily(uint8_t family)
{
    deviceFamily = family;
    LogInfo((std::string("Device Family is ") + std::to_string(deviceFamily)).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/deviceFamilyCharacteristic");
}
// Device State Characteristic
void setDeviceState(uint8_t state)
{
    deviceState = state;
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/stateCharacteristic");
}
// Device Error Message Characteristic
void setDeviceError(std::string message)
{
    deviceError = message;
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/errorCharacteristic");
}
// SIM and APN Details Characteristic
void setSimApnDetails(std::string simStatus, std::string apn, std::string ipConnectivity)
{
    json j;
    j["SIM Status"] = simStatus;
    j["APN"] = apn;
    j["IP Connectivity"] = ipConnectivity;
    
    simApnDetails = j.dump();
    LogInfo(("SIM/APN Status is " + simApnDetails).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/simApnCharacteristic");
}
// Connectivity Characteristic
void setConnectivityDetails(std::string ethernetLink, std::string cloudConnect)
{
    json j;
    j["Ethernet Link"] = ethernetLink;
    j["CloudConnect"] = cloudConnect;
    
    connectivityDetails = j.dump();
    LogInfo(("Connectivity is " + connectivityDetails).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/connectivityCharacteristic");
}

// IP Addresses Characteristic
void setIpAddresses(std::string wanIp, std::string lanIp, int connectedDevices)
{
    json j;
    j["WAN IP"] = wanIp;
    j["LAN IP"] = lanIp;
    j["Connected Devices"] = connectedDevices;
    
    ipAddresses = j.dump();
    LogInfo(("IP Addresses: " + ipAddresses).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/ipAddressesCharacteristic");
}

// TR069 Status Characteristic
void setTr069Status(std::string acsServer, std::string lastConnect)
{
    json j;
    j["ACS Server"] = acsServer;
    j["Last Connect"] = lastConnect;
    
    tr069Status = j.dump();
    LogInfo(("TR069 Status: " + tr069Status).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/tr069StatusCharacteristic");
}

// Supported Bands Characteristic
void setSupportedBands(std::string const &lte, std::string const &nr1, std::string const &nr2)
{
    json j;
    j["LTE"] = lte;
    j["NR1"] = nr1;
    j["NR2"] = nr2;

    supportedBands = j.dump();
    LogInfo(("Supported Bands: " + supportedBands).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/supportedBandsCharacteristic");
}

// BandLock Characteristic (NB: writable by client, we set modem based on this)
void setBandLock(std::string const &lte, std::string const &nr1, std::string const &nr2)
{
    json j;
    j["LTE"] = lte;
    j["NR1"] = nr1;
    j["NR2"] = nr2;

    bandLock = j.dump();
    LogInfo(("Band Lock: " + bandLock).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/bandLockCharacteristic");
}

// GPS and Magnetic Data Characteristic
void setGpsMagneticData(float latitude, float longitude, int height, int magneticStatus, int azimuth, int downtilt)
{
    json j;
    j["Latitude"] = latitude;
    j["Longitude"] = longitude;
    j["Height"] = height;
    j["MagneticStatus"] = magneticStatus;
    j["Antenna Azimuth"] = azimuth;
    j["Antenna Downtilt"] = downtilt;
    
    gpsMagneticData = j.dump();
    LogInfo(("GPS/Magnetic Data: " + gpsMagneticData).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/gpsMagneticCharacteristic");
}

// Battery Characteristic
void setBatteryData(int level, float voltage)
{
    json j;
    j["Battery Level"] = level;
    j["Battery Voltage"] = voltage;
    
    batteryData = j.dump();
    LogInfo(("Battery Data: " + batteryData).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/deviceInformationService/batteryCharacteristic");
}

//
// Current Connection Service
//

// Current Connection Details Characteristic
void setCurrentConnection(int cqi, int mcs, int rankIndicator, int txPower)
{
    json j;
    j["CQI 0/1"] = cqi;
    j["MCS"] = mcs;
    j["Rank Indicator"] = rankIndicator;
    j["TX Power"] = txPower;
    
    connectionParameters = j.dump();
    LogInfo(("Current Connection: " + connectionParameters).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/currentConnectionService/connectionParametersCharacteristic");
}
// Connected RAT Characteristic
void setConnectedRat(std::string serviceType)
{
    connectedRat = serviceType;
    LogInfo(("Connected RAT: " + connectedRat).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/currentConnectionService/connectedRatCharacteristic");
}
// Connection Status Code Characteristic
void setConnectionStatusCode(uint8_t connectionCode)
{
    connectionStatusCode = connectionCode;
    LogInfo(("Connection Status code: " + std::to_string(connectionStatusCode)).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/currentConnectionService/connectionStatusCodeCharacteristic");
}
// Antenna Bars Characteristic
void setAntennaBars(uint8_t antennaBars)
{
    connectionAntennaBars = antennaBars;
    LogInfo(("Antenna Bars: " + std::to_string(connectionAntennaBars)).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/currentConnectionService/antennaBarsCharacteristic");
}

//
// Visible Cell Service
//

// Visible Cell List Characteristic
void setVisibleCellList(std::string pciList)
{
    visibleCellList = pciList;
    LogInfo(("Visible Cell PCIs: " + visibleCellList).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/visibleCellService/visibleCellListCharacteristic");
}
// Cell Detail Request Characteristic (NB: writable by client, we set requested cell characteristic based on this)
const char * getVisibleCellRequest(void)
{
    return visibleCellRequest.c_str();
}

const char * getVisibleCellList(void)
{
    return visibleCellList.c_str();
}

void clearVisibleCellRequest(void)
{
    visibleCellRequest = "";
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/visibleCellService/visibleCellRequestCharacteristic");
}

// Cell Detail Characteristic
void setVisibleCellDetails(std::string cellType, std::string cellTech, unsigned int cellId, unsigned int pci, unsigned int earfcn, 
                           int rsrp, int rsrq, int sinr, CellRole role)
{
    json j;
    j["Cell Type"] = cellType;
    j["Cell Technology"] = cellTech;
    j["Cell ID"] = cellId;
    j["PCI"] = pci;
    j["EARFCN"] = earfcn;
    j["RSRP"] = rsrp;
    j["RSRQ"] = rsrq;
    j["SINR"] = sinr;
    switch (role)
    {
        case CELL_ROLE_PRIMARY:
        {
            j["Role"] = "P";
            break;
        }
        case CELL_ROLE_SECONDARY:
        {
            j["Role"] = "S";
            break;
        }
        case CELL_ROLE_NEIGHBOUR:
        {
            j["Role"] = "N";
            break;
        }
    }
    
    visibleCellInfo = j.dump();
    LogInfo(("Requested Cell Info: " + visibleCellInfo).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/visibleCellService/visibleCellInfoCharacteristic");
    
    visibleCellRequest = "";
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/visibleCellService/visibleCellRequestCharacteristic");
}

const char *getVisibleCellDetails(void)
{
    return visibleCellInfo.c_str();
}

// Set/get/clear the content of "visibleCellService/visibleCellInfoCharacteristic" notification.
void setVisibleCellInfoNoti(std::string notiContent)
{
    visibleCellInfoNoti = notiContent;
}

std::string getVisibleCellInfoNoti(void)
{
    return visibleCellInfoNoti;
}

void clearVisibleCellInfoNoti(void)
{
    visibleCellInfoNoti = "";
}

//
// LED Service
//

// LED Colour Characteristic
void setLedColour(std::string newColour)
{
    ledColour = newColour;
    LogInfo(("LED colour is now " + ledColour).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/ledService/ledColourCharacteristic");
}

// LED Flashing Characteristic
void setLedFlash(bool flash, bool fastflash)
{
    json j;
    j["flash"] = flash;
    j["fastflash"] = fastflash;
    
    ledFlash = j.dump();
    LogInfo(("LED flashing is " + ledFlash).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/ledService/ledFlashCharacteristic");
}

//
// RSSI Service
//

// BLE RSSI Characteristic
void setBleRssi(int rssi)
{
    json j;
    j["RSSI"] = rssi;
    
    bleRssi = j.dump();
    LogInfo(("BLE RSSI: " + bleRssi).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/rssiService/rssiCharacteristic");
}
//
// SpeedTest Service
//

// SpeedTest Request Characteristic (NB: writable by client, we set results if request is newly written to)
bool checkSpeedTestRequest(void)
{
    return speedTestRequest;
}

void clearspeedTestRequest(void)
{
    speedTestRequest = 0;
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/speedTestService/speedTestRequestCharacteristic");
}

// SpeedTest Result Characteristic (only if request has been newly written to)
void setSpeedTestResult(unsigned int downstream, unsigned int upstream, unsigned int latency, std::string experience)
{
    json j;
    j["Downstream (Mbps)"] = downstream;
    j["Upstream (Mbps)"] = upstream;
    j["Latency"] = latency;
    j["Experience"] = experience;

    speedTestResult = j.dump();
    LogInfo(("Speed Test Result: " + speedTestResult).c_str());
    ggkNofifyUpdatedCharacteristic("/com/gobbledegook/speedTestService/speedTestResultCharacteristic");

    clearspeedTestRequest();
}


//
// Generate some test data for the characteristics, if we're not on a real device
//

void generateRandomData(void)
{
    //
    // Device Information Service
    //
    
    // Device Version Characteristic
    setDeviceVersion("0.1", "0.1", "0.1");

    // Device State Characteristic
    setDeviceState(random() % 5);
    
    // Device Error Message Characteristic
    const std::string errors[] = { "OK", "Little Error", "Big Error" };
    setDeviceError(errors[random() % (sizeof(errors)/sizeof(errors[0]))]);
    
    // SIM and APN Details Characteristic
    setSimApnDetails("OK", "MyAPN", "IPv4");
    
    // Connectivity Characteristic
    setConnectivityDetails("1000Mbit", "N/A");
    
    // IP Addresses Characteristic
    setIpAddresses("0.0.0.0", "0.0.0.0", 0);
    
    // TR069 Status Characteristic
    setTr069Status("10.0.0.123", "2019-08-24 14:40:22");

    // Supported Bands Characteristic
    setSupportedBands("AAAAAAAAAAAAAACACAAARQ==", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIA=", "AAAC");

    // BandLock Characteristic (NB: writable by client, we set modem based on this)
    setBandLock("AAAAAAAAAAAAAACACAAARQ==", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIA=", "AAAC");
    
    // GPS and Magnetic Data Characteristic
    setGpsMagneticData(randMToN(-90,90), randMToN(-180,180), (int)(random() % 8858) - 10, random() % 2, randMToN(0,359), randMToN(-90,90));
    
    // Battery Characteristic
    setBatteryData(random() % 100, randMToN(3.7, 4.2));
    
    //
    // Current Connection Service
    //
    
    // Current Connection Details Characteristic
    setCurrentConnection(random() % 100, random() % 32, random() % 100, random() % 10000);
    
    // Connected RAT Characteristic
    const std::string serviceType[] = { "No Service", "Limited Service", "LTE", "5G Sub-6GHz", "5G mmWave" };
    setConnectedRat(serviceType[random() % (sizeof(serviceType)/sizeof(serviceType[0]))]);
    
    // Connection Status Code Characteristic
    setConnectionStatusCode(random() % 5);
    
    // Antenna Bars Characteristic
    setAntennaBars(random() % 5);
    
    //
    // Visible Cell Service
    //
    
    // Visible Cell List Characteristic
    std::vector<int> vec;
    // LTE cells
    int i;
    i = random() % 5;
    while (i--)
    {
        vec.push_back(random() % 503);
    }
    // NR cells
    i = random() % 2;
    while (i--)
    {
        vec.push_back(random() % 1008);
    }
    
    LogInfo(("Generated " + std::to_string(vec.size()) + " visible cells").c_str());
    if (vec.size())
    {
        std::ostringstream oss;
        std::copy(vec.begin(), vec.end()-1, std::ostream_iterator<int>(oss, ","));
        oss << vec.back();
        
        setVisibleCellList(oss.str());
    }
    else
    {
        setVisibleCellList("");
    }
    // Cell Detail Request Characteristic (NB: writable by client, we set requested cell characteristic based on this)
    
    // Cell Detail Characteristic (set by main loop in test mode)
    
    //
    // LED Service
    //
    
    // LED Colour Characteristic
    const std::string colours[] = { "FF0000", "0000FF", "FF8800", "FFFFFF" };
    setLedColour(colours[random() % (sizeof(colours)/sizeof(colours[0]))]);

    // LED Flashing Characteristic
    setLedFlash((random() % 2), (random() % 2));

    //
    // RSSI Service
    //
    setBleRssi((int)(random() % 50) - 90);
    // BLE RSSI Characteristic
    
    //
    // SpeedTest Service
    //
    
    // SpeedTest Request Characteristic (NB: writable by client, we set results if request is newly written to)
    
    // SpeedTest Result Characteristic (set by main loop in test mode)

}

//
// Set test data for the characteristics with json data file.
//
bool setDataWithFile(const char *fname)
{
    std::string k = "";
    try {
        json jConf;
        json newV;
        const void * oldV;
        std::ifstream confFile(fname);

        confFile >> jConf;

        for (json::iterator iter = jConf.begin(); iter != jConf.end(); ++iter) {
            k = iter.key();
            newV = iter.value();
            oldV = dataGetter(k.c_str());

            if (k == "deviceInformationService/deviceVersionCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setDeviceVersion(newV["Hardware"], newV["Software"], newV["GATT"]);
            }
            else if (k == "device/model_num" &&
                    (newV != std::string((const char*)oldV)) ) {
                modelNumString = std::string(newV);
            }
            else if (k == "deviceInformationService/deviceFamilyCharacteristic" &&
                    (newV != *((uint8_t*)oldV)) ) {
                setDeviceFamily(newV);
            }
            else if (k == "deviceInformationService/stateCharacteristic" &&
                    (newV != *((uint8_t*)oldV)) ) {
                setDeviceState(newV);
            }
            else if (k == "deviceInformationService/errorCharacteristic" &&
                    (newV != std::string((const char*)oldV)) ) {
                setDeviceError(std::string(newV));
            }
            else if (k == "deviceInformationService/simApnCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setSimApnDetails(newV["SIM Status"], newV["APN"], newV["IP Connectivity"]);
            }
            else if (k == "deviceInformationService/connectivityCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setConnectivityDetails(newV["Ethernet Link"], newV["CloudConnect"]);
            }
            else if (k == "deviceInformationService/ipAddressesCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setIpAddresses(newV["WAN IP"], newV["LAN IP"], newV["Connected Devices"]);
            }
            else if (k == "deviceInformationService/tr069StatusCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setTr069Status(newV["ACS Server"], newV["Last Connect"]);
            }
            else if (k == "deviceInformationService/supportedBandsCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setSupportedBands(newV["LTE"], newV["NR1"], newV["NR2"]);
            }
            else if (k == "deviceInformationService/bandLockCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setBandLock(newV["LTE"], newV["NR1"], newV["NR2"]);
            }
            else if (k == "deviceInformationService/gpsMagneticCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setGpsMagneticData(newV["Latitude"], newV["Longitude"], newV["Height"],
                        newV["MagneticStatus"], newV["Antenna Azimuth"], newV["Antenna Downtilt"]);
            }
            else if (k == "deviceInformationService/batteryCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV))) {
                setBatteryData(newV["Battery Level"], newV["Battery Voltage"]);
            }
            else if (k == "currentConnectionService/connectionParametersCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setCurrentConnection(newV["CQI 0/1"], newV["MCS"], newV["Rank Indicator"], newV["TX Power"]);
            }
            else if (k == "currentConnectionService/connectedRatCharacteristic" &&
                    (newV != std::string((const char*)oldV)) ) {
                setConnectedRat(std::string(newV));
            }
            else if (k == "currentConnectionService/connectionStatusCodeCharacteristic" &&
                    (newV != *((uint8_t*)oldV))) {
                setConnectionStatusCode(newV);
            }
            else if (k == "currentConnectionService/antennaBarsCharacteristic" &&
                    (newV != *((uint8_t*)oldV)) ) {
                setAntennaBars(newV);
            }
            else if (k == "visibleCellService/visibleCellListCharacteristic" &&
                    (newV != std::string((const char*)oldV)) ) {
                setVisibleCellList(newV);
            }
            else if (k == "visibleCellService/visibleCellInfoCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                visibleCellInfo = newV.dump();
            }
            else if (k == "ledService/ledColourCharacteristic" &&
                    (newV != std::string((const char*)oldV)) ) {
                setLedColour(std::string(newV));
            }
            else if (k == "rssiService/rssiCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setBleRssi(newV["RSSI"]);
            }
            else if (k == "ledService/ledFlashCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setLedFlash(newV["flash"], newV["fastflash"]);
            }
            else if (k == "rssiService/rssiCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setBleRssi(newV["RSSI"]);
            }
            else if (k == "speedTestService/speedTestResultCharacteristic" &&
                    (std::string((const char*)oldV).empty() || newV != json::parse((const char*)oldV)) ) {
                setSpeedTestResult(newV["Downstream (Mbps)"], newV["Upstream (Mbps)"], newV["Latency"], newV["Experience"]);
            }
        }
        return true;
    }

    catch(const json::exception &ex) {
        std::cout << "ERROR!!: " << k << " -" << ex.what() << std::endl;
        return false;
    }
}

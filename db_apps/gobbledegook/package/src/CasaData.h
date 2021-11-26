
#pragma once
#include <iostream>

enum CellRole
{
    CELL_ROLE_PRIMARY,
    CELL_ROLE_SECONDARY,
    CELL_ROLE_NEIGHBOUR
};

// Basic utility for generating very poor quality random numbers
float randMToN(float M, float N);

const void *dataGetter(const char *pName);

int dataSetter(const char *pName, const void *pData);

void setDeviceVersion(std::string hardwareVersion, std::string softwareVersion, std::string gattVersion);

void setDeviceState(uint8_t state);

void setDeviceError(std::string message);

void setSimApnDetails(std::string simStatus, std::string apn, std::string ipConnectivity);

void setConnectivityDetails(std::string ethernetLink, std::string cloudConnect);

void setIpAddresses(std::string wanIp, std::string lanIp, int connectedDevices);

void setTr069Status(std::string acsServer, std::string lastConnect);

void setSupportedBands(std::string const &lte, std::string const &nr1, std::string const &nr2);

void setBandLock(std::string const &lte, std::string const &nr1, std::string const &nr2);

void setGpsMagneticData(float latitude, float longitude, int height, int magneticStatus, int azimuth, int downtilt);

void setBatteryData(int level, float voltage);

void setCurrentConnection(int cqi, int mcs, int rankIndicator, int txPower);

void setConnectedRat(std::string serviceType);

void setConnectionStatusCode(uint8_t connectionCode);

void setAntennaBars(uint8_t antennaBars);

void setVisibleCellList(std::string pciList);

const char* getVisibleCellRequest(void);

const char* getVisibleCellList(void);

void clearVisibleCellRequest(void);

void setVisibleCellDetails(std::string cellType, std::string cellTech, unsigned int cellId, unsigned int pci, unsigned int earfcn, 
                           int rsrp, int rsrq, int sinr, CellRole role);

const char *getVisibleCellDetails(void);

void setLedColour(std::string newColour);

void setLedFlash(bool flash, bool fastflash);

void setBleRssi(int rssi);

bool checkSpeedTestRequest(void);

void clearspeedTestRequest(void);

void setSpeedTestResult(unsigned int downstream, unsigned int upstream, unsigned int latency, std::string experience);

void generateRandomData(void);

bool setDataWithFile(const char *fname);

void setVisibleCellInfoNoti(std::string notiCont);

std::string getVisibleCellInfoNoti(void);

void clearVisibleCellInfoNoti(void);


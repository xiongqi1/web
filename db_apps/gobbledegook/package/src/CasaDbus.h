
#pragma once

namespace CasaDbus
{
extern bool niceMode;
bool checkConnectionsForPairing(bool depairCurrentDevice);
void deletePairingRecord(const char* device, const char* interface);
};



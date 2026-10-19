#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_HOSTED_ENABLED

#include <stdint.h>

#include "Print.h"
#include "IPAddress.h"

#include "WiFiType.h"
#include "WiFiSTA.h"
#include "WiFiAP.h"
#include "WiFiScan.h"
#include "WiFiGeneric.h"

#include "WiFiClient.h"
#include "WiFiServer.h"
#include "WiFiUdp.h"

class WiFiClass : public WiFiGenericClass, public WiFiSTAClass, public WiFiScanClass, public WiFiAPClass {
private:
  bool prov_enable;

public:
  WiFiClass() {
    prov_enable = false;
  }

  using WiFiGenericClass::channel;

  using WiFiSTAClass::BSSID;
  using WiFiSTAClass::BSSIDstr;
  using WiFiSTAClass::RSSI;
  using WiFiSTAClass::SSID;

  using WiFiScanClass::BSSID;
  using WiFiScanClass::BSSIDstr;
  using WiFiScanClass::channel;
  using WiFiScanClass::encryptionType;
  using WiFiScanClass::RSSI;
  using WiFiScanClass::SSID;

public:
  void printDiag(Print &dest);
  friend class NetworkClient;
  friend class NetworkServer;
  friend class NetworkUDP;
  void enableProv(bool status);
  bool isProvEnabled();
};

extern WiFiClass WiFi;

#endif /* SOC_WIFI_SUPPORTED */

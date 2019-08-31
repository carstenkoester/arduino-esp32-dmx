/*
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncUDP.h>
#include <lwip/ip_addr.h>
#include <lwip/igmp.h>
#include <WiFiDMX.h>

#include "sACNPacket.h"

/**
 ** Global variables
 **/
volatile unsigned long WifiDMX::dmxLastReceived;
unsigned char WifiDMX::_previousDMXBuffer[513];
dmx_callback_func_type WifiDMX::_callbackFunc = NULL;
boolean WifiDMX::_packetDebug;
AsyncUDP WifiDMX::_udp;
volatile boolean WifiDMX::_newData = false;

//  Hand macro to conditionalize logging
#define DebugPrintLn(a)  if(WifiDMX::_packetDebug) Serial.println(a)

WiFiMulti wifiMulti;

/**
 ** Public methods
 **/

/*
 * Set up a new DMX client.
 *
 * This function takes care of setting up the WiFi client, and then configures a sACN UDP listener
 * for one universe.
 *
 * If packetDebug is true, then every received UDP packet will be logged.
 *
 * This method is intended to be used when using a "poll-mode" implementation waiting for new
 * DMX packets. After setup, waitForNewData() can be used in Arduino's loop() function to
 * continously wait for new data.
 */
void WifiDMX::setup(const char* WiFiSSID, const char* WiFiPassword, int universe, boolean packetDebug)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPassword);
  WiFi.setSleep(false);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("WiFi Failed");
      delay(1000);
      ESP.restart();
  }

  WifiDMX::_setup_common(universe, packetDebug);
}

void WifiDMX::setup(int numCredentials, const wlan_credential_t WifiCredentials[], int universe, boolean packetDebug)
{
  for (int i=0; i<numCredentials; i++) {
    Serial.printf("Adding AP %d SSID \"%s\" to search list\n", i+1, WifiCredentials[i].ssid);
    wifiMulti.addAP(WifiCredentials[i].ssid, WifiCredentials[i].password);
  }

  uint64_t chipid=ESP.getEfuseMac(); //The chip ID is essentially its MAC address
  Serial.printf("ESP32 Chip ID = %04X",(uint16_t)(chipid>>32)); //print High 2 bytes
  Serial.printf("%08X\n",(uint32_t)chipid); //print Low 4bytes.

  Serial.printf("Connecting Wifi...\n");
  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.print("WiFi connected. SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    WiFi.setSleep(false);
  } else {
    Serial.println("WiFi Failed");
    delay(1000);
    ESP.restart();
  }

  WifiDMX::_setup_common(universe, packetDebug);
}

/*
 * Alternative method for setting up a DMX client.
 *
 * In addition to the same parameters as setup(), this method also takes a callback function as
 * a parameter. Each time new DMX data is received, the callback function will be called -
 * avoiding the need for waiting in a loop.
 */
void WifiDMX::setup_with_callback(const char* WiFiSSID, const char* WiFiPassword, int universe, dmx_callback_func_type callbackFunc, boolean packetDebug)
{
  setup(WiFiSSID, WiFiPassword, universe, packetDebug);
  _callbackFunc = callbackFunc;
}

void WifiDMX::setup_with_callback(int numCredentials, const wlan_credential_t WifiCredentials[], int universe, dmx_callback_func_type callbackFunc, boolean packetDebug)
{
  setup(numCredentials, WifiCredentials, universe, packetDebug);
  _callbackFunc = callbackFunc;
}

/*
 * When set up in poll-mode, this function blocks and waits for new DMX data.
 */
WifiDMX::dmxBuffer WifiDMX::waitForNewData()
{
  while (!_newData) {}
  _newData = false;
  return _previousDMXBuffer;
}


/**
 ** Private methods
 **/

/*
 * Internal, common part of Setup function
 */
void WifiDMX::_setup_common(int universe, boolean packetDebug)
{
  IPAddress sacnMulticastAddress;

  // Used by lwip library
  ip4_addr_t _sacnMulticastAddress;
  ip4_addr_t _wifiAddress;

  // Set up UDP listener
  sacnMulticastAddress = IPAddress(239,255,((universe >> 8) & 0xff), universe & 0xff);
  if (_udp.listenMulticast(sacnMulticastAddress, 5568, 1)) {
      Serial.print("UDP Listening on IP: ");
      Serial.println(IPAddress(239,255,0,universe));

      // Convert IP addresses to a format suitable for lwip and join IGMP group
      _wifiAddress.addr = static_cast<uint32_t>(WiFi.localIP());
      _sacnMulticastAddress.addr = static_cast<uint32_t>(sacnMulticastAddress);
      if (igmp_joingroup(&_wifiAddress, &_sacnMulticastAddress) != ERR_OK)
      {
        Serial.print("Error joining IGMP group!");
      }

      // Define packet handler
      _udp.onPacket(_dmx_receive);
  }

  // Pre-set last packet timer to current time
  dmxLastReceived = millis();

  // Configure debugging
  _packetDebug = packetDebug;
}

/*
 * Internal packet handler for received UDP packets
 *
 * Performs validation of the received packet and then calls the user-provided
 * callback function, if any.
 */
void WifiDMX::_dmx_receive(AsyncUDPPacket packet)
{
  sACNPacket_t *sACNPacket;

  DebugPrintLn((String) "Received packet from " + packet.remoteIP() + ", port " + packet.remotePort());

  // We're only processing packets that contain a full DMX universe - these would always be 638 bytes long
  if (packet.length() != 638)  // We don't handle short universes
  {
    DebugPrintLn((String) "Packet has unexpected size " + packet.length());
    return;
  }
    
  //  Overlay the sACN packet structure onto the received packet
  sACNPacket = (sACNPacket_t *) packet.data();

  /*
   * Test to see that we really have a sACN packet, by looking the fields
   * at each layer that indicates the particular protocol
   */
  if (memcmp(sACNPacket->packet.Root.packet_id, "ASC-E1.17\0\0\0", 12) != 0)
  {
    DebugPrintLn("Packet is not an sACN root packet");
    return;
  }

  if (memcmp(sACNPacket->packet.Frame.vector, "\0\0\0\002", 4) != 0)
  {
    DebugPrintLn("Packet is not an sACN frame");
    return;
  }

  if (sACNPacket->packet.DMP.ad_types != 0xA1)
  {
    DebugPrintLn("Packet is not a DMP packet");
    return;
  }

  // Make sure it is a good old DMX packet, which has 0x00 in the starting slot
  if (sACNPacket->packet.DMP.slots[0] != 0x00)
  {
    // Not a DMX packet
    DebugPrintLn("Packet is not a DMX packet");
    return;
  }

  // This officially counts as having received a packet. Reset the packet timer.
  dmxLastReceived = millis();

  // Verify that the packet content differs from previous buffer ie. that we
  // actually have _new_ data
  if (memcmp(sACNPacket->packet.DMP.slots, _previousDMXBuffer, sizeof(_previousDMXBuffer)) == 0)
  {
    DebugPrintLn("Packet is identical to current values; no change");
    return;
  }

  // If we got here, then this is a new SACN packet
  DebugPrintLn("Received sACN packet with new DMX data");
  memcpy(_previousDMXBuffer, sACNPacket->packet.DMP.slots, sizeof(_previousDMXBuffer));
  _newData = true;

  if (_callbackFunc) {
    _callbackFunc(sACNPacket->packet.DMP.slots);
    _newData = false;
  }
}

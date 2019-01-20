#include <WiFi.h>
#include <AsyncUDP.h>
#include <WiFiDMX.h>

#include "sACNPacket.h"

unsigned char WifiDMX::_previousDMXBuffer[513];
dmx_callback_func_type WifiDMX::_callbackFunc = NULL;
boolean WifiDMX::_packetDebug;
AsyncUDP WifiDMX::_udp;
volatile boolean WifiDMX::_newData = false;

//
//  Handy macros to conditionalize logging
//
#define DebugPrint(a)    if(WifiDMX::_packetDebug) Serial.print(a)
#define DebugPrintLn(a)  if(WifiDMX::_packetDebug) Serial.println(a)

void WifiDMX::setup(const char* WiFiSSID, const char* WiFiPassword, int universe, boolean packetDebug)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFiPassword);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed");
        while(1) {
            delay(1000);
        }
    }

    if (_udp.listenMulticast(IPAddress(239,255,0,universe), 5568, 1)) {
        Serial.print("UDP Listening on IP: ");
        Serial.println(IPAddress(239,255,0,universe));
        _udp.onPacket(dmx_receive);
    }

    _packetDebug = packetDebug;
}

void WifiDMX::setup_with_callback(const char* WiFiSSID, const char* WiFiPassword, int universe, dmx_callback_func_type callbackFunc, boolean packetDebug)
{
    setup(WiFiSSID, WiFiPassword, universe, packetDebug);
    _callbackFunc = callbackFunc;
}


unsigned char* WifiDMX::waitForNewData()
{
  while (!_newData) {}
  _newData = false;
  return _previousDMXBuffer;
}

void WifiDMX::dmx_receive(AsyncUDPPacket packet)
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

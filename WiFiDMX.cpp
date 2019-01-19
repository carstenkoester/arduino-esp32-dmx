#include <WiFi.h>
#include <AsyncUDP.h>
#include <WiFiDMX.h>

#include "sACNPacket.h"

unsigned char WifiDMX::_previousDMXBuffer[513];
dmx_callback_func_type WifiDMX::_callbackFunc;
boolean WifiDMX::_packetDebug;
AsyncUDP WifiDMX::_udp;

//
//  Handy macros to conditionalize logging
//
#define DebugPrint(a)    if(WifiDMX::_packetDebug) Serial.print(a)
#define DebugPrintLn(a)  if(WifiDMX::_packetDebug) Serial.println(a)

void WifiDMX::setup(const char* WiFiSSID, const char* WiFiPassword, int universe, dmx_callback_func_type callbackFunc, boolean packetDebug)
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
    _callbackFunc = callbackFunc;
}


void WifiDMX::dmx_receive(AsyncUDPPacket packet)
{
  DebugPrintLn((String) "Received packet from " + packet.remoteIP() + ", port " + packet.remotePort());

  // We're only processing packets that contain a full DMX universe - these would always be 638 bytes long
  if (packet.length() != 638)  // We don't handle short universes
  {
    DebugPrintLn((String) "Packet has unexpected size " + packet.length());
    return;
  }
    
  //  Read the sACN packet into the data structure
  memcpy(sACNPacket.buffer, packet.data(), sizeof(sACNPacket.buffer));

  /*
   * Test to see that we really have a sACN packet, by looking the fields
   * at each layer that indicates the particular protocol
   */
  if (memcmp(sACNPacket.packet.Root.packet_id, "ASC-E1.17\0\0\0", 12) != 0)
  {
    // Not sACN root packet
    DebugPrintLn("Packet is not an sACN root packet");
    return;
  }

  if (memcmp(sACNPacket.packet.Frame.vector, "\0\0\0\002", 4) != 0)
  {
    // Not an ACN Frame
    DebugPrintLn("Packet is not an sACN frame");
    return;
  }

  if (sACNPacket.packet.DMP.ad_types != 0xA1)
  {
    // Not a DMP packet
    DebugPrintLn("Packet is not a DMP packet");
    return;
  }

  // Make sure it is a good old DMX packet, which has 0x00 in the starting slot
  if (sACNPacket.packet.DMP.slots[0] != 0x00)
  {
    // Not a DMX packet
    DebugPrintLn("Packet is not a DMX packet");
    return;
  }

  // Verify that the packet content differs from previous buffer ie. that we
  // actually have _new_ data
  if (memcmp(sACNPacket.packet.DMP.slots, _previousDMXBuffer, sizeof(_previousDMXBuffer)) == 0)
  {
    DebugPrintLn("Packet is identical to current values; no change");
    return;
  }

  // If we got here, then this is an SACN packet
  DebugPrintLn("Received sACN packet with new DMX data");

  memcpy(_previousDMXBuffer, sACNPacket.packet.DMP.slots, sizeof(_previousDMXBuffer));
  _callbackFunc(sACNPacket.packet.DMP.slots);
}

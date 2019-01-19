#ifndef WIFI_DMX_H
#define WIFI_DMX_H

#include <AsyncUDP.h>

typedef void (*dmx_callback_func_type)(unsigned char *);

class WifiDMX
{
  public:
    static void setup(const char* WiFiSSID, const char* WiFiPassword, int universe, dmx_callback_func_type callback_func, boolean packetDebug);

  private:
    static void dmx_receive(AsyncUDPPacket packet);

    static dmx_callback_func_type _callbackFunc;
    static unsigned char _previousDMXBuffer[513];
    static boolean _packetDebug;
    static AsyncUDP _udp;
};

#endif

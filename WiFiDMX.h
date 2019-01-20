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
#ifndef WIFI_DMX_H
#define WIFI_DMX_H

#include <AsyncUDP.h>

typedef void (*dmx_callback_func_type)(unsigned char *);

class WifiDMX
{
  public:
    static void setup(const char* WiFiSSID, const char* WiFiPassword, int universe, boolean packetDebug);
    static void setup_with_callback(const char* WiFiSSID, const char* WiFiPassword, int universe, dmx_callback_func_type callback_func, boolean packetDebug);

    static unsigned char* waitForNewData();

  private:
    static void _dmx_receive(AsyncUDPPacket packet);

    static dmx_callback_func_type _callbackFunc;
    static unsigned char _previousDMXBuffer[513];
    static boolean _packetDebug;
    static AsyncUDP _udp;
    volatile static boolean _newData;
};

#endif

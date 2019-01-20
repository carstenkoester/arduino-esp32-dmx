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
#include <WiFiDMX.h>

/**
 ** This application implements a basic PWM dimmer for an RGBW strip
 ** (or, of course, an RGBA strip or any other 4-channel LED device).
 **
 ** It consumes FIVE DMX channels:
 **  - First channel is an overall intensity
 **  - Second through 5th channel are the values for Red, Green, Blue
 **    and White (in that order).
 **
 ** The 5-channel method seems to work well with lighting consoles
 ** and software that treat RGB fixtures as intelligent fixtures
 ** which have a single intensity value, and colors defined as
 ** attributes.
 **/
 
/*
 * User-level constants. These (most likely) need to be modified each time
 * this application is used in a new location
 */
// WLAN SSID and password
const char WLAN_SSID[] = "<your network ssid>";
const char WLAN_PASSWORD[] = "<your network password>";

// DMX universe and address
const int DMX_UNIVERSE = 1;
const int DMX_ADDRESS = 100;

// Whether or not to enable packet debug. Use for testing only.
const boolean ALL_PACKET_DEBUG = false;  // Debug print ALL packets
const boolean NEW_PACKET_DEBUG = true;   // Debug print on new packets (changed values) only

/*
 * Internal constants. These DO NOT need to be changed unless the hardware
 * layout is changed.
 */
const String VERSION = "0.9.0";   // Application version

//  GPIO pins that are used. Set these to match your board
const int RED_PWM = 26;
const int GREEN_PWM = 25;
const int BLUE_PWM = 4;
const int WHITE_PWM = 21;

// setting PWM properties
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;


/* 
 * Arduino setup and loop routines 
 */
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("");
  Serial.println("");
  Serial.println((String) "RGBW LED control program -- Version " + VERSION);
  Serial.print((String) "Listening on DMX Universe " + DMX_UNIVERSE);
  Serial.println((String) ", address " + DMX_ADDRESS + "-" + (DMX_ADDRESS+4));

  for (int ledChannel = 0; ledChannel < 3; ledChannel++) {
    ledcSetup(ledChannel, PWM_FREQ, PWM_RESOLUTION);  
  }

  // Define the GPIO pins we are going to use
  ledcAttachPin(RED_PWM, 0);
  ledcAttachPin(GREEN_PWM, 1);
  ledcAttachPin(BLUE_PWM, 2);
  ledcAttachPin(WHITE_PWM, 3);
  
  //  Initialize the WiFi_DMX routines with the Universe of interest
  WifiDMX::setup(WLAN_SSID, WLAN_PASSWORD, DMX_UNIVERSE, ALL_PACKET_DEBUG);
} // setup()

void loop()
{
  int intensity;
  WifiDMX::dmxBuffer dmxBuffer;

  dmxBuffer = WifiDMX::waitForNewData();
  if (NEW_PACKET_DEBUG) {
    Serial.printf("RX: Intensity:%d R:%d B:%d G:%d W:%d\n", dmxBuffer[DMX_ADDRESS], dmxBuffer[DMX_ADDRESS+1], dmxBuffer[DMX_ADDRESS+2], dmxBuffer[DMX_ADDRESS+3], dmxBuffer[DMX_ADDRESS+4]);
  }
  intensity = dmxBuffer[DMX_ADDRESS];
  ledcWrite(0, dmxBuffer[DMX_ADDRESS+1] * intensity / 255);
  ledcWrite(1, dmxBuffer[DMX_ADDRESS+2] * intensity / 255);
  ledcWrite(2, dmxBuffer[DMX_ADDRESS+3] * intensity / 255);
  ledcWrite(3, dmxBuffer[DMX_ADDRESS+4] * intensity / 255);  
}

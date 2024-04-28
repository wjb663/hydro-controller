# 26-Computer-Vision-based-hydroponic-control-system

Navigate to "features" branch to get the latest version of the files capable of being ran.

ModusToolbox installation link: https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/

Select CY8CPROTO-062-4343W when the IDE has loaded and it will download the relevant drivers. Download WiFi-MQTT_Client_Code example program and replace the code in the files with the code from the features branch. Then run the program and it'll flash to the board.

For the camera:
Two ESP-32s are needed to run the current version of the working code - one for the camera capture, one for the sensor code. The hardware files in our Google drive contain the KICAD files for the camera PCB. There are two version of the camera board - one using an ESP-32 breakout board, the second using a soldered on ESP 32. Both PCB designs contain the pinouts for how the DHT11 sensor, the ambient light sensor, and the Arducam should be connected to each camera board/ESP 32. Connect the DHT11 and ambient light sensor to one ESP32, the ArduCAM to the second. 

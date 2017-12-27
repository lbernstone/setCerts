setCerts.ino
============

This is a quickie sketch that requires no modification to the code to use.  Upload it to your ESP32.  It will create a WiFi AP where you can connect. With a cell phone, it will have a captive portal (at 192.168.4.1) that will show you scanned WiFi points.  Select a SSID and enter the password.  It will then restart and connect to your AP (the IP address will come up on the serial monitor).

You can then connect to the web server at root to enter your MQTT credentials.  When you submit, it will save the credentials to NVS, and then attempt to make a connection.  If unsuccessful, it will give you some basic error info.  You can also connect directly to /test to test with saved credentials.

The PubSubClient used is available in the Arduino IDE library manager.  The source for WebServer and WiFiManager have not been updated to work with ESP32 yet, so you will need to get those from [bbx10's github](http://github.com/bbx10).

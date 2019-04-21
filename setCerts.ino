/* Example AWS IOT tester with web based certificate upload
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <MD5Builder.h>
#include <WebServer.h>
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient

#define KEY_MAX_SIZE 4000 //max size of an nvs string
// unremark the ssid and pass to bypass WiFiManager
//#define MYSSID "larryb"
//#define MYPASS "clownfish"

#ifndef MYSSID
// WiFiManager currently needs to be run in the
// development branch for ESP32
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#endif

//nvs_handle nvs_mqtt;
Preferences prefs;
WebServer server(80);

char* hostname() {
  char host[19];
  snprintf(host, 19, "ESP32-%012llX", ESP.getEfuseMac());
  return host;
}

void verify_ca(const char* cert, const char* server) {
}

String print_key (const char* key, const String val) {
  String html = "<br>" + String(key) + ":  ";
  if (strstr(key,"cert")) {
    MD5Builder md5;
    md5.begin();
    md5.add(val);
    md5.calculate();
    html += md5.toString() + " (md5)";
  } else {html += String(val);}
  Serial.println(html);
  return html;
}

String runTest() {
  String mqtt_id, mqtt_addr;
  String root_ca_pem, certificate_pem_crt, private_pem_key;
  String result = "<html><head></head><body><h2>Testing MQTT Connection</h2>";
  mqtt_id = prefs.getString("mqtt_id");
  result += print_key("mqtt_id",mqtt_id);
  mqtt_addr = prefs.getString("mqtt_addr");
  result += print_key("mqtt_addr",mqtt_addr);
  root_ca_pem = prefs.getString("root_cert");
  result += print_key("root_cert",root_ca_pem);
  certificate_pem_crt = prefs.getString("client_cert");  
  result += print_key("client_cert",certificate_pem_crt);
  private_pem_key = prefs.getString("cert_key");
  result += print_key("cert_key",private_pem_key);

  WiFiClientSecure net;
  net.setCACert(root_ca_pem.c_str());
  net.setCertificate(certificate_pem_crt.c_str());
  net.setPrivateKey(private_pem_key.c_str());

  net.connect(mqtt_addr.c_str(), 8883);
  const char azfp[] = "8E CD E6 88 4F 3D 87 B1 12 5B A3 1A C3 FC B1 3D 70 16 DE 7F 57 CC 90 4F E1 CB 97 C6 AE 98 19 6E";
  if (net.verify(azfp, mqtt_addr.c_str())) Serial.println("Certificate verified!");

  PubSubClient thing(mqtt_addr.c_str(), 8883, net);
  thing.connect(mqtt_id.c_str());
  if (thing.connected()) {
    result += "<h2>Connection successful!</h2>";
  } else {
      result += "<h2>Connection failed.</h2>";
      char err_buf[100];
      if (net.lastError(err_buf,100)<0) {
        result += "<br>" + String(err_buf);
      } else {
        result += "<br>MQTT Error: " + String(thing.state());
      }
  }
  return result;
}

void handleTest() {
  if (server.hasArg("mqtt_addr")) {
      prefs.putString("mqtt_addr", server.arg("mqtt_addr"));
  }
  if (server.hasArg("mqtt_id")) {
      prefs.putString("mqtt_id", server.arg("mqtt_id"));
  }
  server.send(200, "text/html", runTest());
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  boolean valid_type = false;
  if (upload.totalSize > KEY_MAX_SIZE) {
    Serial.println("Certificate file too large"); return;
  }
  if (upload.status == UPLOAD_FILE_START) {
    Serial.print("File Upload: ");
    Serial.println(upload.name);
    if (upload.name == "root_cert" || upload.name == "client_cert" || upload.name == "cert_key") {
      valid_type = true;}
    if (!valid_type) {
      Serial.println("Didn't find cert type");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
// Since our files must be under the size of the buffer, we can ignore any
// UPLOAD_FILE_WRITEs, and just use the buffer at FILE_END
    upload.buf[upload.totalSize]=0;
    prefs.putString(upload.name.c_str(), (char*)upload.buf);
  }
}

void handleRoot() {
  String html = R"(
<html><head></head><body>
<form method="post" action='test' enctype="multipart/form-data">
 <table>
   <tr><td>Host ID<td>
   <input type="text" id="mqtt_id" name="mqtt_id" value=")";
  html += String(hostname());
  html += R"(">
   <tr><td>Host Address<td>
   <input type="text" id="mqtt_addr" name="mqtt_addr" width="42">
   <tr><td>Root CA<td>
   <input type="file" id="root_cert" name="root_cert" accept=".crt,.pem">
   <tr><td>Client Certificate<td>
   <input type="file" id="client_cert" name="client_cert" accept=".crt">
   <tr><td>Private Key<td>
   <input type="file" id="cert_key" name="cert_key" accept=".key">
 </table>
   <input type='submit' value='Test Connection'/>
</form>)";
  server.send(200, "text/html", html);
}

void initWebserver() {
  server.on("/", handleRoot);
  server.on("/test", HTTP_POST, handleTest, handleUpload);
  server.on("/test", HTTP_GET, handleTest);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println();

#ifdef MYSSID
  WiFi.begin(MYSSID, MYPASS);
  WiFi.waitForConnectResult();
#else
  WiFiManager wifimanager;
  wifimanager.setDebugOutput(true);
  wifimanager.autoConnect(hostname());
#endif
  Serial.println(WiFi.localIP());

  prefs.begin("mqtt");
  initWebserver();
}

void loop() {
  server.handleClient();
  delay(100);
}

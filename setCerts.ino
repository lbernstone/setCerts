// Example AWS IOT tester with web based certificate upload
#include <WiFi.h>
#include <nvs_flash.h>
#include <WiFiClientSecure.h>
#include <MD5Builder.h>
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <WebServer.h> //https://github.com/bbx10/WebServer_tng
#include <WiFiManager.h> //https://github.com/bbx10/WiFiManager

#define KEY_MAX_SIZE 1984 //max size of an nvs key

nvs_handle nvs_mqtt;
WebServer server(80);

void set_nvs(const char* key, const char* value) {
  esp_err_t err = nvs_set_str(nvs_mqtt, key, value);
  if (err != ESP_OK) {
    Serial.print("NVS Error: ");
    Serial.println(err);
  }
}

String get_nvs (const char* key) {
  size_t required_size = KEY_MAX_SIZE;
  nvs_get_str(nvs_mqtt, key, NULL, &required_size);
  char result[required_size];
  esp_err_t err = nvs_get_str(nvs_mqtt, key, result, &required_size);
  if (err != ESP_OK) {
    Serial.print("NVS Error: ");
    Serial.println(err);
  }
  return String(result);
}

String print_nvs (const char* key, const String val) {
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
  mqtt_id = get_nvs("mqtt_id");
  result += print_nvs("mqtt_id",mqtt_id);
  mqtt_addr = get_nvs("mqtt_addr");
  result += print_nvs("mqtt_addr",mqtt_addr);
  root_ca_pem = get_nvs("root_cert");
  result += print_nvs("root_cert",root_ca_pem);
  certificate_pem_crt = get_nvs("client_cert");
  result += print_nvs("client_cert",certificate_pem_crt);
  private_pem_key = get_nvs("cert_key");
  result += print_nvs("cert_key",private_pem_key);

  WiFiClientSecure net;
  net.setCACert(root_ca_pem.c_str());
  net.setCertificate(certificate_pem_crt.c_str());
  net.setPrivateKey(private_pem_key.c_str());

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
      set_nvs("mqtt_addr", server.arg("mqtt_addr").c_str());
  }
  if (server.hasArg("mqtt_id")) {
      set_nvs("mqtt_id", server.arg("mqtt_id").c_str());
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
    esp_err_t err = nvs_set_str(nvs_mqtt, upload.name.c_str(), (char*)upload.buf);
  }
}

void handleRoot() {
  String html = R"(
<html><head></head><body>
<form method="post" action='test' enctype="multipart/form-data">
 <table>
   <tr><td>Host ID<td>
   <input type="text" id="mqtt_id" name="mqtt_id" value=")";
  html += String(ESP_getChipId());
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

  WiFiManager wifimanager;
  //wifimanager.setDebugOutput(true);
  wifimanager.autoConnect();
  Serial.println(WiFi.localIP());

  esp_err_t err = nvs_flash_init();
  err = nvs_open("mqtt", NVS_READWRITE, &nvs_mqtt);

  initWebserver();
}

void loop() {
  server.handleClient();
  delay(100);
}

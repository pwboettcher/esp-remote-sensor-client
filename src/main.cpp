
//nodemcu pinout https://github.com/esp8266/Arduino/issues/584
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// This were for an old version with that used DS18B20 Dallas Semiconductor
// temperature sensors over OneWire.  The plan is to put these back into
// this newer framework

//#include <OneWire.h>
//#include <DallasTemperature.h>


const int MY_VERSION = 11;

const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 0;

const int PIR_PIN = 5;

void connectWifi();
void sayHello();
String build_full_json();
void sendJSON(const String &json);

HX711 scale;

struct nw {
  const char *ssid;
  const char *pwd;
};

/*
In networks.h, put these definitions.  And don't check it into source control.

const char* server = "something.somewhere.com";
nw networks[] = { {"ssid1", "pw1"},
		  {"ssid2", "pw2"},
		  {NULL, NULL}};
*/
#include "networks.h"

// Simple class to take in a bunch of samples
// and compute an average
struct Averager {
    double cur_val;
    double debug_arr[80];
    int cnt;
    void submit(double val) {
        cur_val += val;
        debug_arr[cnt] = val;
        cnt++;
    }
    void reset() {
        cnt = 0;
        cur_val = 0.0;
    }
    double val() {
        return cur_val / static_cast<double>(cnt);
    }
};

// Base class for a sensor that hangs off of this ESP board
struct Sensor {
    String name;
    String id;
    Averager avg;
    bool present;
    bool debug;

    Sensor(const char *_name) : name(_name), present(true), debug(false) {
        char msg[32];
        sprintf(msg, "%08X", ESP.getChipId());
        id = name + msg;
    }
    bool is_present() const { return present; }
    virtual double get_cur_reading() = 0;
    void DoMeasure() {
        avg.submit(get_cur_reading());
    }
    void AddJSONObj(JsonObject &obj)
    {
        obj["type"] = name;
        obj["id"] = id;
        obj["val"] = avg.val();

        if(debug) {
            JsonArray debug_arr = obj.createNestedArray("debug");
            for(int i=0; i<avg.cnt; i++) {
                debug_arr.add(avg.debug_arr[i]);
            }
        }
    }
};

// Strain gauge interface.  This is sloppy... this
// class just refers to the global HX711 interface
struct Scale : public Sensor {
    Scale() : Sensor("strain") {}

    double get_cur_reading() override {
        if (scale.is_ready()) {
            long x = scale.read_average(8);
            //long x = scale.read();
            Serial.println(x);
            return(static_cast<double>(x));
        } else {
            Serial.println("HX711 not found.");
            present = false;
        }
        return(0.0);
    }
};

// PIR interface.  Just a digital pin.  This needs
// to be better-configurable
struct PIR : public Sensor {
    PIR() : Sensor("pir") {}

    double get_cur_reading() override {
        Serial.println(digitalRead(PIR_PIN));
        return static_cast<double>(digitalRead(PIR_PIN));
    }
};

std::vector<Sensor *> sensors;
int cntReading = 0;

void setup() {
    Serial.begin(115200);
    connectWifi();

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    //scale.set_gain(128);
    //scale.power_up();

    pinMode(PIR_PIN, INPUT);

    Scale *s = new Scale();
    s->debug = true;
    sensors.push_back(s);

    PIR *p = new PIR();
    sensors.push_back(p);

    //DS18B20.begin();
    //nSensors = DS18B20.getDS18Count();

    /*
    Serial.println(String("found ") + String(nSensors) + " DS18B20 sensors:");
    for(uint8_t i=0; i<nSensors; i++) {
        bool ret = DS18B20.getAddress(addr[i], i);
        if(ret)
        Serial.println(String("addr: ") + genAddressString(addr[i]));
        else
        Serial.println("failure");
    }
    */

    // connect with server and say hi
    sayHello();
}

// over-the-air update... fetch a binfile from an internet server and
// program.  right now, we then wait until the next schedule reset
// to pick up the changes.  this should probably cause an immediate
// reset
void doOTAupdate()
{
    WiFiClient client;
    if (client.connect(server, 80))
    {
        Serial.println("Connected to server for update");
        t_httpUpdate_return ret = ESPhttpUpdate.update(client, server, 80, "/static/sensor.bin", "optional current version string here");
        int err = ESPhttpUpdate.getLastError();
        String errStr = ESPhttpUpdate.getLastErrorString();
        Serial.println(errStr + " " + String(err));

        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
            Serial.println("[update] Update failed.");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[update] Update no Update.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[update] Update ok."); // may not called we reboot the ESP
            break;
        }
    }
}

// main loop
int totalCount = 0;
void loop() {

    for(auto &s : sensors) {
        s->DoMeasure();
    }

    // read 60 times for averaging,
    // then submit
    if(++cntReading >= 60) {
        cntReading = 0;

        sendJSON(build_full_json());

        for(auto &s : sensors) {
            s->avg.reset();
        }

        totalCount++;

        if(totalCount >= 60) {
            ESP.reset();
        }
    }

    delay(800);
}

void connectWifi()
{
    int n = WiFi.scanNetworks();
    nw network = {NULL, NULL};

    Serial.println("");
    for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        Serial.printf("%i: %s (%i)%c\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                      (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? ' ' : '*');

        for (int j = 0; networks[j].ssid != NULL; ++j) {
            if (String(networks[j].ssid) == WiFi.SSID(i)) {
                network = networks[j];
            }
        }
    }

    if(network.ssid != NULL) {
        Serial.print(String("Connecting to ") + network.ssid);
        WiFi.begin(network.ssid, network.pwd);
        while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        }
    }

    Serial.println("\nConnected\n");
}

String build_full_json()
{
    DynamicJsonDocument  doc(2000);
    JsonArray meas = doc.createNestedArray("measurements");

    for(auto &s : sensors) {
        //if(!s->is_present())
        //    continue;

        JsonObject sensor_obj = meas.createNestedObject();
        s->AddJSONObj(sensor_obj);
    }

    String json;
    serializeJson(doc, json);
    return(json);
}

void sendJSON(const String &json)
{
    WiFiClient client;
    HttpClient http = HttpClient(client, server, 80);
    http.post("/post_measurements", "application/json", json);
    String response = http.responseBody();
    Serial.println(response);
    client.stop();
}

void sayHello()
{
    Serial.println("Version " + String(MY_VERSION));

    char msg[32];
    sprintf(msg, "%08X", ESP.getChipId());

    DynamicJsonDocument  doc(2000);
    doc["chip"] = msg;
    doc["version"] = MY_VERSION;

    String postStr;
    serializeJson(doc, postStr);

    WiFiClient client;
    HttpClient http = HttpClient(client, server, 80);
    http.post("/hello", "application/json", postStr);
    client.stop();

    String response = http.responseBody();

    DynamicJsonDocument serverJson(256);
    DeserializationError ret = deserializeJson(serverJson, response);
    Serial.println(ret.c_str());

    JsonObject sinfo = serverJson["data"];
    serializeJson(sinfo, Serial);

    // The "hello" response includes the latest firmware version
    // on the server, so we can decide whether to update
    int version = sinfo["fwversion"];
    Serial.println("Server has version " + String(version));

    if(version > MY_VERSION) {
        Serial.println("Server has version " + String(version) + ".  Doing update.");
        doOTAupdate();
    }

}

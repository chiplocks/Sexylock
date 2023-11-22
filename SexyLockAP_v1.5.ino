#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <StreamUtils.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

AsyncWebServer server(80);

const char index_html[] PROGMEM = "<!DOCTYPE html><html><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>h1{text-align:center;margin:auto;color:#2bca23;text-shadow:0 0 5px #26ac1f,0 0 20px #26ac1f,0 0 30px #26ac1f}.line{cursor:default;position:relative;font-size:50px}.line:after{content:'';position:absolute;width:100%;height:5px;bottom:-10px;left:-4px;transform:scaleX(0);box-shadow:0 -5px 0 #26ac1f6d;background:#20711969;transform-origin:bottom right;transition:transform .5s ease-out}.line:hover:after{transform:scaleX(1);transform-origin:bottom left}body{background-image:linear-gradient(to right,#2c2b2b,#1a1b1b,#403f3f,#403f3f,#393939,#252525)}h2{color:#26ac1f;text-shadow:0 0 20px #26ac1f;text-align:center}#uidtable{text-align:center;margin:auto}th{color:#26ac1f;text-shadow:0 0 5px #26ac1f}td{color:#26ac1f;text-shadow:0 0 5px #26ac1f}.button{color:red;text-shadow:0 0 10px red}.button:hover{text-shadow:0 0 10px red,0 0 20px red,0 0 35px red,0 0 30px red}#uidaddname,#uidadduid{padding-bottom:12px}label{align-items:center;margin:auto;display:block;margin-bottom:2px}input[type=text]{align-items:center;margin:auto;display:block;background-color:#343434;color:#45ca3e;border-color:#000}input[type=text]:focus{outline:0}input[type=submit]{color:#40cb65;background-color:#3b3a3a}input[type=submit]:hover{color:#000;background-color:#30b127}form{color:#34a351;text-shadow:0 0 5px #26ac1f,0 0 20px #26ac1f;text-align:center;margin:auto}</style><script>document.addEventListener(\"readystatechange\",(function(e){\"interactive\"==e.target.readyState&&fetch(\"uids.json\").then((e=>{if(!e.ok)throw new Error(\"HTTP error \"+e.status);return e.json()})).then((e=>{let t=document.getElementById(\"uidtable\");for(let n=0,r=e.length;n<r;n++){let r=e[n],o=t.insertRow(-1);o.insertCell(-1).appendChild(document.createTextNode(r.name)),o.insertCell(-1).appendChild(document.createTextNode(r.uid)),o.insertCell(-1).innerHTML='<a href=\"remove?uid='+n+'\" class=\"button\">&#x2715;</a>'}document.getElementById(\"loading\").style.display=\"none\"})).catch((function(){console.log(\"Error fetching UIDs JSON\")}))}))</script><body><main><h1 class=\"line\">chimpolock</h1><div id=\"uidlist\"><h2>Stored UIDs:</h2><table id=\"uidtable\"><tr><th>Name</th><th>UID</th><th>Remove</th></tr></table><h3 id=\"loading\">Loading UIDs, please wait...</h3></div><div id=\"uidadd\"><h2>Add UID:</h2><form action=\"/add\" method=\"get\"><div id=\"uidaddname\"><label for=\"name\">Name:</label> <input type=\"text\" name=\"name\" id=\"name\" required></div><div id=\"uidadduid\"><label for=\"uid\">UID:</label> <input type=\"text\" name=\"uid\" id=\"uid\" required></div><input type=\"submit\" value=\"Add\"></form></div></main></body></html>";
void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_html);
}

void checkJSON() {
  File file = LittleFS.open("/uids.json", "r");
  if (!file) {
    file.close();
    Serial.println("Creating new uids.json");
    File filew = LittleFS.open("/uids.json", "w");
    filew.print("[]");
    filew.close();
  } else {
    file.close();
  }
}

void handleJSON(AsyncWebServerRequest *request) {
  request->send(LittleFS, "/uids.json");
}

void handleRemove(AsyncWebServerRequest *request) {
  if(request->hasParam("uid")){
    AsyncWebParameter* p = request->getParam("uid");
    uint8_t index = p->value().toInt();

    File file = LittleFS.open("/uids.json", "r");
    ReadBufferingStream bufferedFileR(file, 64);

    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 1024);
    deserializeJson(doc, bufferedFileR);
    doc.shrinkToFit();
    
    file.close();

    doc.as<JsonArray>().remove(index);

    File filew = LittleFS.open("/uids.json", "w");
    WriteBufferingStream bufferedFileW(filew, 64);
    serializeJson(doc, bufferedFileW);
    bufferedFileW.flush();
    filew.close();
  }

  request->redirect("/");
}

void handleAdd(AsyncWebServerRequest *request) {  
  if(request->hasParam("uid") && request->hasParam("name")){
    AsyncWebParameter* puid = request->getParam("uid");
    AsyncWebParameter* pname = request->getParam("name");

    Serial.print("ADD UID - ");
    Serial.print("Name: ");
    Serial.println(pname->value());
    Serial.print("UID: ");
    Serial.println(puid->value());

    File file = LittleFS.open("/uids.json", "r");
    ReadBufferingStream bufferedFileR(file, 64);

    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 1024);
    deserializeJson(doc, bufferedFileR);
    file.close();

    JsonObject nested = doc.as<JsonArray>().createNestedObject();
    nested["name"] = pname->value();
    nested["uid"] = puid->value();

    File filew = LittleFS.open("/uids.json", "w");
    WriteBufferingStream bufferedFileW(filew, 64);
    serializeJson(doc, bufferedFileW);
    bufferedFileW.flush();
    filew.close();

    Serial.println("New JSON -");
    serializeJson(doc, Serial);
  }
  request->redirect("/");
}



void setup() {
  Serial.begin(115200);
  WiFi.softAP("ChimpoLock", "Password123");

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    LittleFS.format();
    LittleFS.begin();
  }
  checkJSON();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/add", HTTP_GET, handleAdd);
  server.on("/remove", HTTP_GET, handleRemove);
  server.on("/uids.json", HTTP_GET, handleJSON);
  
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
  });

  server.begin();
  Serial.println("Booted!");
}

void loop() {
  
}
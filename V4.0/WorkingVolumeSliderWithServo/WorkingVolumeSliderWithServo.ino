#include <WiFiS3.h>
#include <Servo.h>
Servo myservo;

// ===== WiFi config =====
char ssid[] = ""; // Fill Wifi Name
char pass[] = ""; // Fill Wifi Passoword
WiFiServer server(80);


// ===== Helpers: HTTP responses =====
void sendOK(WiFiClient& c, const char* body="ok", const char* type="text/plain") {
  c.print("HTTP/1.1 200 OK\r\n");
  c.print("Content-Type: "); c.print(type); c.print("\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
  c.print("Access-Control-Allow-Headers: Content-Type\r\n");
  c.print("Connection: close\r\n\r\n");
  c.print(body); c.print("\r\n");
}

void sendBadRequest(WiFiClient& c, const char* msg="bad request") {
  c.print("HTTP/1.1 400 Bad Request\r\n");
  c.print("Content-Type: text/plain\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Connection: close\r\n\r\n");
  c.print(msg); c.print("\r\n");
}

void sendNoContent(WiFiClient& c) {
  c.print("HTTP/1.1 204 No Content\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
  c.print("Access-Control-Allow-Headers: Content-Type\r\n");
  c.print("Connection: close\r\n\r\n");
}

bool hasDHCPLease() {
  IPAddress ip = WiFi.localIP();
  return (ip[0] | ip[1] | ip[2] | ip[3]) != 0;
}

// Extracts numeric level from ".../volume?level=NN ..." (0..100). Returns -1 if not found/invalid.
int parseLevel(const String& reqLine) {
  int q = reqLine.indexOf("GET /volume");
  if (q < 0) return -1;
  int lv = reqLine.indexOf("level=", q);
  if (lv < 0) return -1;
  lv += 6; // skip "level="
  int end = reqLine.indexOf(' ', lv);
  if (end < 0) end = reqLine.length();
  String num = reqLine.substring(lv, end);
  // chop at '&' if present
  int amp = num.indexOf('&');
  if (amp >= 0) num = num.substring(0, amp);
  num.trim();
  if (num.length() == 0) return -1;
  long val = num.toInt();
  if (val < 0 || val > 100) return -1;
  return (int)val;
}

void setup() {
  Serial.begin(115200);
  myservo.attach(9);
  myservo.write(180);
  while (!Serial) {}

  Serial.println();
  Serial.println("==== UNO R4 WiFi — Volume Server ====");
  Serial.print("Target SSID: "); Serial.println(ssid);

  Serial.print("Connecting (link)… ");
  WiFi.begin(ssid, pass);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  wl_status_t st = (wl_status_t)WiFi.status();
  Serial.print("Link status: "); Serial.println((int)st); // expect 3 == WL_CONNECTED

  // Wait for DHCP to deliver a non-0 IP
  Serial.println("Waiting for DHCP lease…");
  unsigned long t1 = millis();
  while (!hasDHCPLease() && millis() - t1 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (!hasDHCPLease()) {
    Serial.println("!! No DHCP lease (IP=0.0.0.0).");
    Serial.println("   • Ensure 2.4GHz network");
    Serial.println("   • Check router DHCP is enabled");
    Serial.println("   • Or use static IP via WiFi.config()");
    // We won’t start the server without an IP.
    return;
  }

  // Report network details
  Serial.println("-- Connection details --");
  Serial.print("SSID: ");    Serial.println(WiFi.SSID());
  Serial.print("IP: ");      Serial.println(WiFi.localIP());
  Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet: ");  Serial.println(WiFi.subnetMask());
  Serial.print("RSSI: ");    Serial.println(WiFi.RSSI());
  Serial.println("DHCP lease acquired. ✅");

  server.begin();
  Serial.println("HTTP server started on port 80. Ready.");
}

void loop() {
  WiFiClient c = server.available();
  if (!c) return;

  // Wait briefly for data
  unsigned long t0 = millis();
  while (c.connected() && !c.available() && millis() - t0 < 1000) delay(1);
  if (!c.available()) { c.stop(); return; }

  // Read the first line of the request
  String line = c.readStringUntil('\n');
  line.trim();
  if (line.length()) {
    Serial.print("[REQ] "); Serial.print(c.remoteIP()); Serial.print(" -> ");
    Serial.println(line);
  }

  // CORS preflight
  if (line.startsWith("OPTIONS ")) {
    sendNoContent(c);
    c.stop();
    return;
  }

  // ping endpoint
  if (line.indexOf("GET /ping") >= 0) {
    Serial.print("[PING] from ");
    Serial.println(c.remoteIP());
    sendOK(c, "pong");
  }
  // /volume?level=0..100 endpoint
  else if (line.indexOf("GET /volume") >= 0) {
    int level = parseLevel(line);
    if (level < 0) {
      Serial.println("[VOL] bad/missing level param");
      sendBadRequest(c, "Expected /volume?level=0..100");
    } else {
      Serial.print("[VOL] set to "); Serial.print(level); Serial.println("%");
      int angle = map(level, 0, 100, 180, 0);
      myservo.write(angle);

      String body = String("{\"ok\":true,\"level\":") + level + "}\n";
      sendOK(c, body.c_str(), "application/json");
    }
  }

  else {
    sendOK(c, "ok");
  }

  // Drain extra bytes quickly; then close
  unsigned long t = millis();
  while (millis() - t < 30 && c.connected() && c.available()) c.read();
  c.stop();
}


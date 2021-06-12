// You'll need to personalize a few settings in this file.
// They're marked with 'Change this!'. Feel free to search it.

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <MD5Builder.h>
#include <ESP8266WiFiMulti.h>

void blinkN(int N);
void initWLAN();

MD5Builder _md5;
String md5(String str) {
  _md5.begin();
  _md5.add(String(str));
  _md5.calculate();
  return _md5.toString();
}

int press_1s();
int press_10s();
int resetPC();
int statusPC();
const char *commands_str[] = {"press_1s", "press_10s", "resetPC", "statusPC"/*, "auth", "press_Ns"*/};
const int num_commands = sizeof(commands_str) / sizeof(char *);
int (*commands_func[])(){&press_1s, &press_10s, &resetPC, &statusPC/*, &auth, &press_Ns*/};

WiFiUDP Udp;
const int localUdpPort = 4567; //---------------- Change this! ----------------
char incomingPacket[255];
char outgoingPacket[255];

String secret = "YOUR-SUPER-SECRET-STRING-HERE"; //---------------- Change this! ----------------
String expectedResponse;
bool authPending = false;
int functionPending;

ESP8266WiFiMulti WiFiMulti;

void setup() {
   //---------------- Change this! ----------------
  pinMode(D1, OUTPUT); // Power button control
  pinMode(D2, OUTPUT); // Reset button control
  pinMode(D5, INPUT); // Power indicator on PC
  pinMode(D4, OUTPUT); // Onboard LED
  pinMode(A0, INPUT); // Random seed
  Serial.begin(115200);
  Serial.println();
  randomSeed(analogRead(A0));

  initWLAN();

  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

  Serial.printf("Secret: %s\n\n", secret.c_str());
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (!packetSize)
  {
    return;
  }
  
  Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
  int contentLength = Udp.read(incomingPacket, 255);
  if (contentLength == 0)
  {
    return; // Screw empty packets
  }
  incomingPacket[contentLength] = 0; // Terminate string
  Serial.printf("UDP packet contents: %s\n", incomingPacket);

  for (int i = 0; i < num_commands; i++) {
    if (strcmp(incomingPacket, commands_str[i]) == 0) {
      functionPending = i;
      authPending = true;
      String nonce = md5(String(random(0, 32768))); // Generates a lengthy string
      expectedResponse = md5(String(nonce + secret)); // Random string concentrated with secret sting is calculated with MD5 algorithm
      nonce.toCharArray(outgoingPacket, 255);
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(outgoingPacket, strlen(outgoingPacket)); // Send that random string to client and see if it can correctly respond to our challenge
      Udp.endPacket();

      Serial.printf("Execution of commands_func[%d] has been requested.\n", functionPending);
      Serial.printf("Nonce: %s\nExpectation: %s\n\n", nonce.c_str(), expectedResponse.c_str());
      blinkN(1);
      return;
    }
  }
  
  // When code gets here, it indicates that we have either received a response to our challenge (processed only if authPending == true, namely, a response is expected) or some garbage.

  if (authPending) {
    if (strcmp(incomingPacket, expectedResponse.c_str()) == 0) {
      Serial.printf("commands_func[%d] executed.\n", functionPending);
      String((*commands_func[functionPending])()).toCharArray(outgoingPacket, 255); // Permission granted!
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(outgoingPacket, strlen(outgoingPacket));
      Udp.endPacket();
      Serial.printf("Reply: %s\n\n", outgoingPacket);
      blinkN(2);
    } else { // Oh, incorrect answer!
      Serial.printf("Incorrect response. Expected %s\n\n", expectedResponse.c_str());
      blinkN(3);
    }
  } else {
    Serial.printf("Illegal command.");
    blinkN(4);
  }
  
  authPending = false;
}

void blinkN(int N) {
  for (int i = 0; i < N * 2; i++) {
    digitalWrite(D4, !digitalRead(D4));
    delay(200);
  }
}

int press_1s() {
  digitalWrite(D1, HIGH);
  delay(1000);
  digitalWrite(D1, LOW);
  return 0;
}

int press_10s() {
  digitalWrite(D1, HIGH);
  delay(10000);
  digitalWrite(D1, LOW);
  return 0;
}

int resetPC() {
  digitalWrite(D2, HIGH);
  delay(1000);
  digitalWrite(D2, LOW);
  return 0;
}

int statusPC() {
  return digitalRead(D5);
}

void smartConfig() {
  Serial.print("Performing SmartConfig ");
  WiFi.beginSmartConfig();
  while (!WiFi.smartConfigDone()) {
    Serial.print(".");
    digitalWrite(D4, !digitalRead(D4));
    delay(200);
  }
  WiFiMulti.addAP(WiFi.SSID().c_str(), WiFi.psk().c_str());
  Serial.println(" done.");

  Serial.print("Reconnecting to WLAN ");
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(D4, !digitalRead(D4));
    delay(500);
  }
  Serial.println(" connected.");
  digitalWrite(D4, HIGH);
}

void initWLAN() {
  int prevMillis = millis();
  int WLMultiFailed = false;

  WiFiMulti.addAP("Placeholder SSID", "Placeholder PSK"); //---------------- Change this! ----------------
  
  Serial.print("Connecting to WLAN ");
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(D4, !digitalRead(D4));
    delay(500);
    if (millis() > prevMillis + 15000) {
      WLMultiFailed = true;
      break;
    }
  }

  if (WLMultiFailed) {
    Serial.println(" timed out after 15 seconds.");
    smartConfig();
  } else {
    Serial.println(" connected.");
    digitalWrite(D4, HIGH);
  }
  
  Serial.printf("SSID: %s\nPSK: %s\n\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
}

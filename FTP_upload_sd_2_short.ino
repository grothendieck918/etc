/*
   Original Arduino: FTP passive client 
   http://playground.arduino.cc/Code/FTP
   Modified 6 June 2015 by SurferTim

   You can pass flash-memory based strings to Serial.print() by wrapping them with F().

   2015-12-09 Rudolf Reuter, adapted to ESP8266 NodeMCU, with the help of Markus.
*/

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduCAM.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "memorysaver.h"

/*#if !(defined ESP8266 )
#error Please select the ArduCAM ESP8266 UNO board in the Tools/Board
#endif
//This demo can work on OV2640_MINI_2MP/OV5640_MINI_5MP_PLUS/OV5642_MINI_5MP_PLUS/OV5642_MINI_5MP_PLUS/
//OV5642_MINI_5MP_BIT_ROTATION_FIXED/ ARDUCAM_SHIELD_V2 platform.
#if !(defined (OV2640_MINI_2MP)||defined (OV5640_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP_PLUS) \
    || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) \
    ||(defined (ARDUCAM_SHIELD_V2) && (defined (OV2640_CAM) || defined (OV5640_CAM) || defined (OV5642_CAM))))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif*/

// set GPIO16 as the slave select :
const int CS = 16;
//Version 2,set GPIO0 as the slave select :
const int SD_CS = 0;
const int CAM_POWER_ON = 15;
//const int sleepTimeS = 10;
String id = "2kdn2lf";

//#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
ArduCAM myCAM(OV2640, CS);
/*#elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
ArduCAM myCAM(OV5640, CS);
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
ArduCAM myCAM(OV5642, CS);
#endif*/

void myCAMSaveToSDFile(int n){
  char str[8];
  byte buf[256];
  static int i = 0;
  //static int k = 0;
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  bool is_header = false;
  File outFile;
  //Flush the FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();
  Serial.println(F("Star Capture"));
 while(!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK));
 Serial.println(F("Capture Done."));  

 length = myCAM.read_fifo_length();
 Serial.print(F("The fifo length is :"));
 Serial.println(length, DEC);
  if (length >= MAX_FIFO_SIZE) //8M
  {
    Serial.println(F("Over size."));
  }
    if (length == 0 ) //0 kb
  {
    Serial.println(F("Size is 0."));
  }
 //Construct a file name
 itoa(n, str, 10);
 strcat(str, ".jpg");
 //Open the new file
 outFile = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);
 if(! outFile){
  Serial.println(F("File open faild"));
  return;
 }
 i = 0;
 myCAM.CS_LOW();
 myCAM.set_fifo_burst();

while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    //Read JPEG data from FIFO
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
        buf[i++] = temp;  //save the last  0XD9     
       //Write the remain bytes in the buffer
        myCAM.CS_HIGH();
        outFile.write(buf, i);    
      //Close the file
        outFile.close();
        Serial.println(F("Image save OK."));
        is_header = false;
        i = 0;
    }  
    if (is_header == true)
    { 
       //Write image data to buffer if not full
        if (i < 256)
        buf[i++] = temp;
        else
        {
          //Write 256 bytes image data to file
          myCAM.CS_HIGH();
          outFile.write(buf, 256);
          i = 0;
          buf[i++] = temp;
          myCAM.CS_LOW();
          myCAM.set_fifo_burst();
        }        
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      buf[i++] = temp_last;
      buf[i++] = temp;   
    } 
  } 
}

// comment out next line to write to SD from FTP server
#define FTPWRITE

// Set these to your desired softAP credentials. They are not configurable at runtime.
const char *ssid = "AndroidHotspot0398";
const char *password = "ghost444";

//boolean debug = false;  // true = more messages
boolean debug = false;

// LED is needed for failure signalling

unsigned long startTime = millis();

// provide text for the WiFi status
const char *str_status[]= {
  "WL_IDLE_STATUS",
  "WL_NO_SSID_AVAIL",
  "WL_SCAN_COMPLETED",
  "WL_CONNECTED",
  "WL_CONNECT_FAILED",
  "WL_CONNECTION_LOST",
  "WL_DISCONNECTED"
};

// provide text for the WiFi mode
const char *str_mode[]= { "WIFI_OFF", "WIFI_STA", "WIFI_AP", "WIFI_AP_STA" };

// change to your server
IPAddress server( 106, 10, 42, 185 );

WiFiClient client;
WiFiClient dclient;

char outBuf[128];
char outCount;

// change fileName to your file (8.3 format!)
char fileName[13];

// SPIFFS file handle
File fh;

void signalError() {  // loop endless with LED blinking in case of error
  while(1) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(300); // ms
      digitalWrite(LED_BUILTIN, HIGH);
      delay(300); // ms
  }
}

//----------------------- WiFi handling
void connectWifi() {
  Serial.print("Connecting as wifi client to SSID: ");
  Serial.println(ssid);

  // use in case of mode problem
  WiFi.disconnect();
  // switch to Station mode
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
  }

  WiFi.begin ( ssid, password );

  if (debug ) WiFi.printDiag(Serial);

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  // Check connection
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected; IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("WiFi connect failed to ssid: ");
    Serial.println(ssid);
    Serial.print("WiFi password <");
    Serial.print(password);
    Serial.println(">");
    Serial.println("Check for wrong typing!");
  }
}  // connectWiFi()

//----------------- FTP fail
void efail() {
  byte thisByte = 0;

  client.println("QUIT");

  while (!client.available()) delay(1);

  while (client.available()) {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  client.stop();
  Serial.println("Command disconnected");
  fh.close();
  Serial.println("SD closed");
}  // efail

//-------------- FTP receive
byte eRcv() {
  byte respCode;
  byte thisByte;

  while (!client.available()) delay(1);

  respCode = client.peek();

  outCount = 0;

  while (client.available()) {
    thisByte = client.read();
    Serial.write(thisByte);

    if (outCount < 127) {
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  if (respCode >= '4') {
    efail();
    return 0;
  }
  return 1;
}  // eRcv()

//--------------- FTP handling
byte doFTP(int i) {
  itoa(i, fileName, 10);
  strcat(fileName, ".jpg");
  fh = SD.open(fileName, FILE_READ);
  
  if (!fh) {
    Serial.println("SD open fail");
    return 0;
  }

  if (!fh.seek(0)) {
    Serial.println("Rewind fail");
    fh.close();
    return 0;
  }  

  if (debug) Serial.println("SD opened");

  if (client.connect(server, 21)) {  // 21 = FTP server
    Serial.println("Command connected");
  } else {
    fh.close();
    Serial.println("Command connection failed");
    return 0;
  }

  if (!eRcv()) return 0;
  if (debug) Serial.println("Send USER");
  client.println("USER test");

  if (!eRcv()) return 0;
  if (debug) Serial.println("Send PASSWORD");
  client.println("PASS 4539");

  if (!eRcv())  return 0;
  if (debug) Serial.println("Send SYST");
  client.println("SYST");
  
  if (!eRcv()) return 0;
  if (debug) Serial.println("Send Type I");
  client.println("Type I");
  

  if (!eRcv()) return 0;
  if (debug) Serial.println("Send PASV");
  client.println("PASV");
  
  if(i==1)client.println("MKD "+id);
  client.println("CWD "+id);

  if (!eRcv()) return 0;

  char *tStr = strtok(outBuf, "(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL, "(,");
    array_pasv[i] = atoi(tStr);
    if (tStr == NULL) {
      Serial.println("Bad PASV Answer");
    }
  }
  unsigned int hiPort, loPort;
  hiPort = array_pasv[4] << 8;
  loPort = array_pasv[5] & 255;

  if (debug) Serial.print("Data port: ");
  hiPort = hiPort | loPort;
  if (debug) Serial.println(hiPort);

  if (dclient.connect(server, hiPort)) {
    Serial.println("Data connected");
  }
  else {
    Serial.println("Data connection failed");
    client.stop();
    fh.close();
    return 0;
  }
  
  if (debug) Serial.println("Send STOR filename");
  client.print("STOR ");
  client.println(fileName);  

  if (!eRcv()) {
    dclient.stop();
    return 0;
  }
  
  if (debug) Serial.println("Writing");
    // for faster upload increase buffer size to 1460
#define bufSizeFTP 1460
  uint8_t clientBuf[bufSizeFTP];
  size_t clientCount = 0;
  
  while (fh.available()) {
    clientBuf[clientCount] = fh.read();
    clientCount++;
    if (clientCount > (bufSizeFTP-1)) {
      dclient.write((const uint8_t *) &clientBuf[0], bufSizeFTP);
      clientCount = 0;
      delay(1);
    }
  }
  if (clientCount > 0) dclient.write((const uint8_t *) &clientBuf[0], clientCount);

  dclient.stop();
  Serial.println("Data disconnected");

  if (!eRcv()) return 0;

  client.println("QUIT");

  if (!eRcv()) return 0;

  client.stop();
  Serial.println("Command disconnected");

  fh.close();
  if (debug) Serial.println("SD closed");
  return 1;
}  // doFTP()

void setup() {
  uint8_t vid, pid;
  uint8_t temp;
  Wire.begin();
  Serial.begin(115200);
  Serial.println(F("ArduCAM Start!"));

  //set the CS as an output:
  pinMode(CS,OUTPUT);
  pinMode(CAM_POWER_ON , OUTPUT);
  digitalWrite(CAM_POWER_ON, HIGH);

  //initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHZ

  delay(1000);
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55){
    Serial.println(F("SPI1 interface Error!"));
    while(1);
  }
  else
  Serial.println(F("SD Card detected!"));
   
//#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
  //Check if the camera module type is OV2640
  myCAM.wrSensorReg8_8(0xff, 0x01);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 )))
   Serial.println(F("Can't find OV2640 module!"));
  else
   Serial.println(F("OV2640 detected."));
/*  #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
   //Check if the camera module type is OV5640
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_LOW, &pid);
   if((vid != 0x56) || (pid != 0x40))
   Serial.println(F("Can't find OV5640 module!"));
   else
   Serial.println(F("OV5640 detected."));
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
 //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
   if((vid != 0x56) || (pid != 0x42)){
   Serial.println(F("Can't find OV5642 module!"));
   }
   else
   Serial.println(F("OV5642 detected."));
  #endif
    //Change to JPEG capture mode and initialize the OV2640 module*/
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
//  #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
    myCAM.OV2640_set_JPEG_size(OV2640_320x240);
/*  #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5640_set_JPEG_size(OV5640_320x240);
  #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5642_set_JPEG_size(OV5642_320x240);  
  #endif*/
  
  delay(1000);
  Serial.println("Sync,Sync,Sync,Sync,Sync");
  delay(500);
  Serial.println();
  // signal start
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100); // ms
  digitalWrite(LED_BUILTIN, HIGH);
  delay(300); // ms

  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  Serial.println ( "Connect to Router requested" );
  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi mode: ");
    Serial.println(str_mode[WiFi.getMode()]);
    Serial.print ( "Status: " );
    Serial.println (str_status[WiFi.status()]);
    // signal WiFi connect
    digitalWrite(LED_BUILTIN, LOW);
    delay(300); // ms
    digitalWrite(LED_BUILTIN, HIGH);      
  } else {
    Serial.println("");
    Serial.println("WiFi connect failed, push RESET button.");
    signalError();
  }

  if (!SD.begin(0)) {
     Serial.println("SD init fail");
     signalError();
  }

  fh = SD.open(fileName, FILE_READ);
  if (!fh) {
    Serial.println("SD open fail");
    signalError();
  } else fh.close();
  
  Serial.println("Ready. Press u");
}  // setup()

void loop() {
  byte inChar;
  if (Serial.available() > 0) {
    inChar = Serial.read();
  }  
  if (inChar == 'u') {    
    while(get_http() !=0);
    for(int n = 1; n<10; n++){
      //delay(200);
      myCAMSaveToSDFile(n);
      if(n!=9) delay(1000);
      }
    for(int k = 1; k<10; k++){    
      if (doFTP(k)) Serial.println("FTP OK"); 
      else Serial.println("FTP FAIL");}
  }
  delay(10);  // give time to the WiFi handling in the background
}
int get_http(){
  HTTPClient http;
  int ret = 0;
  Serial.println("[HTTP] begin");
  http.begin("http://maker.ifttt.com/trigger/door/with/key/cODWpkfP92z3pM1nTUmo8Q");

  Serial.println("[HTTP] GET");
  int httpCode = http.GET();
  if(httpCode >0){
    Serial.printf("[HTTP] GET code: %d\n", httpCode);
    if(httpCode == HTTP_CODE_OK){
      String payload = http.getString();
      Serial.println(payload);
      }
  } else {
    ret = -1;
    Serial.printf("[HTTP] GET failed. error : %s\n", http.errorToString(httpCode).c_str());      
    }
    http.end();
    return ret;
}



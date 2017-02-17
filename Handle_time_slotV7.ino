#include <Constellation.h>
#include <EEPROM.h>
#include "ntp.h"

//Interval to checkTime on the NTP server
//We check the actual time all 10 seconds here
long intervalCheckTime = 10000;
long previousCheckOfTime = 0;

/*****************************************************************
****************ADRESSES EPPROMM INITIALIZATION*******************
******************************************************************
Here, we reserved some addresses in the eeprom memory of the device
*/
  const int addrBeginHour = 0;
  const int addrBeginMinute = 1;
  const int addrEndHour = 2;
  const int addrEndMinute = 3;

/*****************************************************************
****************TIMEZONE VARIABLE*******************************
****************************************************************
If you don't set the setting "TimeZone" in constellation, we will get the french time by default. 
*/
  int timeZone = 1; 

//************************************************************************************************
//************************************INITIALIZATION *********************************************
//************************************************************************************************
#include <ESP8266WiFi.h>
  char ssid[] = "Connectify-Dan";
  char password[] = "123456789";

//Constellation client
  Constellation<WiFiClient> constellation("192.168.163.1", 8088, "ESP8266", "DemoIsen", "075cd01fd02dcbb279735a5692b6815d975510f4");


//************************************************************************************************
//*****************************************SETUP**************************************************
//************************************************************************************************

void setup(void) {
  EEPROM.begin(512);
  Serial.begin(115200);  
  delay(10);

/************ LED ACTIVATION ****************
You can for test the program, connect a RGB Led to D2,D3 and D4. 
With the HTML example provide, you can switch on or off the LED. 
*/
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  
//************* Set Wifi mode ******************
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

// ********** Connecting to Wifi ***************
  Serial.print("Connecting to ");
  Serial.println(ssid);  
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());


//************************************************************************************************
//**************** DISCUSSION WITH CONSTELLATION *************************************************
//************************************************************************************************

  constellation.registerMessageCallback("DriveLED",
    MessageCallbackDescriptor().setDescription("Operating Schedule").addParameter("heure debut","System.Int32").addParameter("minute debut","System.Int32").addParameter("heure fin","System.Int32").addParameter("minute fin","System.Int32"),
      [](JsonObject& json) {

       writeIntoEEPROM(json["Data"][0].as<int>(),json["Data"][1].as<int>(),json["Data"][2].as<int>(),json["Data"][3].as<int>());

       JsonObject& settings = constellation.getSettings();
          if(settings.containsKey("Timezone")) {
             timeZone = settings["Timezone"].as<int>();
          }
          if(settings.containsKey("NtpServerName")) {
             ntpServerName = settings["NtpServerName"].as<const char*>();
             setNtpServerName(ntpServerName);
          }
      
       constellation.pushStateObject("TimeSlot", stringFormat("{'beginHour':%d, 'beginMinute':%d,'endHour':%d, 'endMinute':%d }", 
       json["Data"][0].as<int>(), 
       json["Data"][1].as<int>(),
       json["Data"][2].as<int>(),
       json["Data"][3].as<int>()));
      });
        
  // Declare the package descriptor
  constellation.declarePackageDescriptor();

  // WriteLog info
  constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());  
}


//************************************************************************************************
//*****************************************LOOP***************************************************
//************************************************************************************************

void loop(void) {
    constellation.loop();

    unsigned long currentMillis = millis();

    if (currentMillis - previousCheckOfTime > intervalCheckTime) {
        previousCheckOfTime = currentMillis;
            
        //Initialization of all variables for our program
        unsigned long result = 0;
        int actualHour = 0;
        int actualMinute = 0;
        int actualTime = 0;
        bool inTimeSlotOrNot = false;
        
        //Getting the current time and minutes
        result = localTime();
        actualHour = ((result  % 86400L) / 3600) + timeZone;
        actualMinute = ((result  % 3600) / 60);
    
        //Writting test in eeprom
        Serial.print("EPPROM debut");
        Serial.print(EEPROM.read(addrBeginHour));
        Serial.print("h");
        Serial.println(EEPROM.read(addrBeginMinute));
        Serial.print("EPPROM fin");
        Serial.print(EEPROM.read(addrEndHour));
        Serial.print("h");
        Serial.println(EEPROM.read(addrEndMinute));
    
        //Checking the current time in the serial canal
        Serial.print("Horaire :");
        Serial.print(actualHour);
        Serial.print("h");
        Serial.print(actualMinute);
        Serial.println("min");
    
        //Adapt the actualTime according the timeZone variable .
        actualTime = ((result  % 86400L) + timeZone) + (result  % 3600);
    
        /*Checking if the we are in the time_slot selected by the user. 
        If we are in, we switch on the equipment.
        If we are not in, we switch off the equipment.*/
        inTimeSlotOrNot = false;
        inTimeSlotOrNot = checkTimeSlot(actualTime);
        if (inTimeSlotOrNot == true){
           digitalWrite(D2, HIGH);
        }else {
           digitalWrite(D2, LOW);
        }
        Serial.println(inTimeSlotOrNot);
    }
}

bool checkTimeSlot(int actualTime){
    int beginHour = 0;
    int endHour = 0;
    int beginMinute = 0;
    int endMinute = 0;

    /*These variable are the translation of the beginning of the time slot and his end, convert into second. 
    It will be easier to compare with actualTime later.*/
    int beginSecond = 0;
    int endSecond = 0;
    
    //Getting time_slot datas since EEPROM memory
    beginHour = EEPROM.read(addrBeginHour);
    endHour = EEPROM.read(addrEndHour);
    beginMinute = EEPROM.read(addrBeginMinute);
    endMinute = EEPROM.read(addrEndMinute);

    //Convert hours and minutes get from EEPROM into second before our comparaison
    beginHour = beginHour*3600;
    beginMinute = beginMinute*60;
    beginSecond = beginHour + beginMinute;

    endHour = endHour*3600;
    endMinute = endMinute*60;
    endSecond = endHour + endMinute;

    //We will test if the time slot setted by the user is on two days or not.
    if (beginSecond <= endSecond){
      if (actualTime>= beginSecond && actualTime<=endSecond){
          return true;        
      }else{
          return false;
      }
    } else if (beginSecond > endSecond){
        if (actualTime >= beginSecond){
            return true; 
        }else {
           if (actualTime <= endSecond){
              return true;
           }else {
              return false;
           }
        }
    }
}

void writeIntoEEPROM(int beginHour, int beginMinute, int endHour, int endMinute) {
     EEPROM.write(addrBeginHour, beginHour);
     EEPROM.write(addrBeginMinute, beginMinute);
     EEPROM.write(addrEndHour, endHour);
     EEPROM.write(addrEndMinute, endMinute); 
     delay(100);
}









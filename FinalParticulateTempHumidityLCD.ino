// BEGIN CONFIGURATION

#define USERAGENT    "Outdoor Air Particle Sensor" // paste your project name
#define FEEDID       FEED-ID // paste your Xively Feed ID
#define APIKEY       "API-KEY" // paste your Xivley API Key, including quotes
#define MACADDRESS   0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // type your Arduino Ethernet MAC address

// END CONFIGURATION

#include <SPI.h>
#include <Ethernet.h>
#include <HttpClient.h>
#include <Xively.h>
#include <Wire.h>
#include "DHT.h"
#include "rgb_lcd.h"

// sets digital pin for air particle sensor
int particleSensorPin = 8;

// sets analog pin for temperature and humidity sensor, then sets type of sensor
#define DHTPIN A0
#define DHTTYPE DHT11

// instantiates temperature and humidity sensor and lcd screen 
DHT dht(DHTPIN, DHTTYPE);
rgb_lcd lcd;

// makes a custom degree character for LCD screen
byte degree[8] = 
{
    0b00011,
    0b00011,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// sets color of LCD
const int colorR = 255;
const int colorG = 0;
const int colorB = 0;

// defines strings for sensors
char particleSensor[]     =  "concentration";
char temperatureSensor[]  =  "temperature";
char humiditySensor[]     =  "humidity";

// package individual sensors into three datastreams
XivelyDatastream datastreams[] = 
{
  XivelyDatastream(particleSensor, strlen(particleSensor), DATASTREAM_FLOAT),
  XivelyDatastream(temperatureSensor, strlen(temperatureSensor), DATASTREAM_FLOAT),
  XivelyDatastream(humiditySensor, strlen(humiditySensor), DATASTREAM_FLOAT)
};

// MAC address
byte mac[] =
{
  MACADDRESS
};

// Xively API Key
char xivelyKey[] = APIKEY;

// Xively Feed ID and number of datastreams
XivelyFeed feed(FEEDID, datastreams, 3);

// initialize ethernet and Xively clients
EthernetClient client;
XivelyClient xivelyclient(client);

// numeric IP for api.xively.com for particle average
IPAddress server(216,52,233,122);

// starting values for particle count calculation
unsigned long duration;
unsigned long lowPulseOccupancy = 0;
float ratio = 0;
float concentration = 0;

// starting value for average particle count
int averageParticles = 0;

// waiting periods and start times for multitasking
unsigned long wait30s = 30000;
unsigned long wait60s = 60000;
unsigned long wait10m = 600000;
unsigned long start30s;
unsigned long start60s;
unsigned long start10m;

void setup()
{
  // begin temperature sensing
  dht.begin();
  
  // set up the LCD's number of columns and rows and color
  lcd.begin(16, 2);
  lcd.setRGB(colorR, colorG, colorB);
  
  // create the degree character defined in sketch setup
  #if 1
    lcd.createChar(0, degree);
  #endif
  
  // initialize port for Serial Monitor
  Serial.begin(9600);
  
  // initialize pin for particle sensor
  pinMode(8, INPUT);
  
  // begin Ethernet connection, pause for any problems and try again
  while (Ethernet.begin(mac) != 1)
  {
    Serial.println(F("Error getting IP address via DHCP, trying again..."));
    delay(15000);
  }
  
  // set start times for calculations and multitasking
  start30s = millis();
  start60s = millis();
  start10m = millis();
}

void loop()
{
  // create variables to carry temperature and humidity data from sensor
  int temperature = dht.readTemperature();
  int humidity = dht.readHumidity(); 
  
  // estimate air particulate
  duration = pulseIn(particleSensorPin, LOW);
  lowPulseOccupancy = lowPulseOccupancy + duration;
  
  // measure air particlate and post all sensor data to Xively every 30 seconds
  if ((millis() - start30s) > wait30s)
  {
    ratio = lowPulseOccupancy / (wait30s * 10.0); // integer percentage 0 => 100
    concentration = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62; // using spec sheet curve
    // once particle sensing data has been gathered, send particle, temperature, and humidity data to Xively
    datastreams[0].setFloat(concentration);
    datastreams[1].setFloat(temperature);
    datastreams[2].setFloat(humidity);
    int ret = xivelyclient.put(feed, xivelyKey); // send put request to Xively
    // Serial.print(F("xivelyclient.put returned ")); // uncomment this line and next line to see return code from Xively
    // Serial.println(ret); // print return from Xively to Serial Monitor
    client.stop();
    client.flush();
    // reset start time and lowPulseOccupancy
    lowPulseOccupancy = 0;
    start30s = millis();
  }
  
  // update LCD screen every 60 seconds
  if ((millis() - start60s) > wait60s)
  {
    // print values to LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Particles: ");
    lcd.print(averageParticles);
    
    lcd.setCursor(0, 1);
    
    lcd.print("T: ");
    lcd.print(temperature);
    lcd.write((unsigned char)0);
    lcd.print("C ");
    
    lcd.print("H: ");
    lcd.print(humidity);
    lcd.print(" %");
    
    start60s = millis();
  }
  
  // pull average particle sensing data from Xively every 10 minutes
  if ((millis() - start10m) > wait10m)
  {
    sendGetRequest();
    start10m = millis();
  }
}

// makes an HTTP connection to the Xively server and pulls average from last hour 
void sendGetRequest() {
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println("Connecting to Xively for average particle value...");
    // send the HTTP GET request:
    client.print("GET /v2/feeds/");
    client.print(F("INSERT FEED ID"));
    client.println("/datastreams/concentration.csv?interval=3600&function=average&limit=1&duration=1hour HTTP/1.1");
    client.println("Host: api.xively.com");
    client.print("X-ApiKey: ");
    client.println(F("INSERT API KEY"));
    client.println("Content-Type: text/csv");
    client.println("Connection: close");
    client.println();
    if (client.connected())
    {
      client.find("Z,");  // skip the timestamp
      averageParticles = client.parseInt(); // get parsed numeric value
      Serial.println();
      Serial.print(averageParticles);
      Serial.println(" particles per 0.01 cubic feet");
      Serial.println();
    }
    client.stop();
  } 
  else {
    Serial.println("Connection failed, disconnecting...");
    client.stop();
  }
}

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define TURBIDITY_PIN A0
#define DS18B20_PIN 5

OneWire oneWire(DS18B20_PIN);
DallasTemperature waterTempSensor(&oneWire);

Adafruit_BMP085 bmp;
Adafruit_HMC5883_Unified compass = Adafruit_HMC5883_Unified(12345);

bool bmpOK = false;
bool compassOK = false;

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }
  
  Serial.println(F("--- OCEAN MONITORING SYSTEM INIT ---"));
  waterTempSensor.begin();
  
  if (bmp.begin()) bmpOK = true;
  if (compass.begin()) compassOK = true;
  delay(2000);
}

void loop() {
  Serial.println(F("\n--- NEW READING ---"));

  // Turbidity
  long totalTurbidity = 0;
  for (byte i = 0; i < 10; i++) {
    totalTurbidity += analogRead(TURBIDITY_PIN);
    delay(3); 
  }
  int turbidity = totalTurbidity / 10;
  Serial.print(F("Turbidity ADC: ")); Serial.println(turbidity);

  // Water Temp
  waterTempSensor.requestTemperatures(); 
  Serial.print(F("Water Temp: ")); Serial.println(waterTempSensor.getTempCByIndex(0));

  // Air Data
  if (bmpOK) {
    Serial.print(F("Air Temp: ")); Serial.println(bmp.readTemperature());
    Serial.print(F("Air Pressure: ")); Serial.println(bmp.readPressure() / 100.0);
  }

  // Compass
  if (compassOK) {
    sensors_event_t event; 
    compass.getEvent(&event);
    float heading = atan2(event.magnetic.y, event.magnetic.x);
    if (heading < 0) heading += 2 * PI;
    Serial.print(F("Compass Heading: ")); Serial.println(heading * 180 / M_PI);
  }

  delay(1500); 
}

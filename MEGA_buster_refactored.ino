#include <Servo.h>
#include "RTClib.h"
#include <Wire.h>
#include "MS5837.h"
#include <SPI.h>
#include <SD.h>

MS5837 PT_sensor;
RTC_PCF8523 rtc;
DateTime now;
byte servoPin = 13;
Servo servo;
// FILENAME SHORT 8.3 CHARACTERS!!
String file_name = "THW0113.TXT";
File sd;

int speed_0 = 1500;
long cd = 5000;
int speeds[5] = {1450, 1425, 1375, 1325, 1100};
long ts[5] = {420000, 300000, 240000, 240000, 240000};
int num_speeds = sizeof(speeds) / sizeof(speeds[0]);
int i = 0;
int num_cycles = 3;

void setup() {
  // servo setup
  Serial.begin(9600);
  servo.attach(servoPin);
  Serial.println("Initializing...");
  // RTC setup
  if (! rtc.begin()) {
    Serial.println("RTC NOT found");
    Serial.flush();
  }
  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC NOT initialized");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.start();
  Serial.println("RTC started");
  // not sure what the point is here...7 seconds at speed 0?
  servo.writeMicroseconds(speed_0);
  delay(7000); 
  // PT sensor setup
  Wire.begin();
  if (!PT_sensor.init()) {
    Serial.println("PT NOT initialized");
  }
  PT_sensor.setModel(MS5837::MS5837_30BA);
  PT_sensor.setFluidDensity(1029); // kg/m^3 (freshwater, 1029 for seawater)
  // SD card setup
  Serial.print("Initializing SD...");
  pinMode(53, OUTPUT);
  if (!SD.begin(53)) {
    Serial.println("SD NOT initialized");
  }
  else{
    Serial.println("SD initialized");
  }
  // run servo at speed 1 for 3 seconds, then speed 0 for 5 seconds?
  servo.writeMicroseconds(1450);
  delay(3000);
  servo.writeMicroseconds(speed_0);
  delay(5000);
  // SD card logging begin
  sd = SD.open(file_name, FILE_WRITE);
  sd.println();
  sd.println();
  sd.println("initialization done.");
  sd.flush();
}

void log_time_PT() {
  // read time and PT
  now = rtc.now();
  PT_sensor.read();
  // print date
  sd.print(now.year(), DEC);
  sd.print('/');
  sd.print(now.month(), DEC);
  sd.print('/');
  sd.println(now.day(), DEC);
  // print time
  sd.print(now.hour(), DEC);
  sd.print(':');
  sd.print(now.minute(), DEC);
  sd.print(':');
  sd.print(now.second(), DEC);
  sd.println();
  // print PT
  sd.print("P ");
  sd.println(PT_sensor.pressure());
  sd.print("T ");
  sd.println(PT_sensor.temperature());
  sd.flush();
}

void flush() {
  Serial.println("Step 0");
  sd.println("Flushing at 1900 for 15s");
  servo.writeMicroseconds(1100);
  delay(14500);   
  servo.writeMicroseconds(speed_0);
  delay(1000);  
  sd.println("Flushing at 1100 for 15s");
  servo.writeMicroseconds(1900);
  delay(14500);   
  sd.println("Flushing done. Wait 5 seconds");  
  servo.writeMicroseconds(speed_0);
  delay(cd);
  log_time_PT();
  // -------------------------------------------------
  sd.println("--------------------------");
  sd.println("-$$$$$$$$$$$$$$$");
  sd.println("--------------------------");
  sd.flush();  
}

void run_speed(int j) {
  sd.print("ESC runs at ");
  sd.print(speeds[j]);
  sd.print(" for ");
  sd.print(ts[j]/1000);
  sd.println("s");
  Serial.println("Step " + String(j + 1));
  log_time_PT();
  servo.writeMicroseconds(speeds[j]);
  delay(ts[j]);
  Serial.println("Step " + String(j + 1) + " done.");
  sd.println("Step " + String(j + 1) + " done.");
  log_time_PT();
  sd.flush();
}

void finish_cycle() {
  servo.writeMicroseconds(speed_0);
  delay(7000); 
  i += 1;
  Serial.println("Done for this depth.");
  sd.println("Done for this depth.");
  sd.println("$$$$$$$$$$$$$$$$$$$$$$");
  sd.flush();
  delay(23000); // wait 30s
}

void idle() {
  now = rtc.now();
  PT_sensor.read();
  sd.println();
  sd.println("Idle...");
  Serial.println("Idle...");
  log_time_PT();
  servo.writeMicroseconds(1450);
  delay(3000);
  servo.writeMicroseconds(speed_0);
  delay(7000);
  sd.flush();
  delay(20000);
}

void loop() {

  Serial.println("Started another loop...");

  if(i == 0){
    Serial.println("Wait 300s..."); // +xs initialization
    delay(250000);
    log_time_PT();
    flush();
  }
  else {
    Serial.println("Wait 300s..."); // +xs initialization
    delay(250000);
    log_time_PT();
  }

  if(i < num_cycles) {
    Serial.println("Cycle " + String(i + 1));
    for (int j = 0; j < num_speeds; j++) {
      run_speed(j);
    }
    finish_cycle();
  }
  else {
    idle();
  }
}

#include <LowPower.h>
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
String file_name = "THW0115.TXT";
File sd;
// start pressure used as reference to determine when we are underwater [mbar]
float start_pressure;
// speeds are PWM signals in [us] for T200 thruster, 1500 = idle, <1500 = reverse thrust (thruster installed in reverse)
int speed_0 = 1500;
int speeds[5] = {1450, 1425, 1375, 1325, 1100};
// corresponding time to run each speed [ms]
long ts[5] = {300000, 300000, 240000, 240000, 240000};
int num_speeds = sizeof(speeds) / sizeof(speeds[0]);
int i = 0;
int num_cycles = 3;

void setup() {
  // Start serial stream
  Serial.begin(9600);
  Serial.println("Initializing...");
  // servo setup
  servo.attach(servoPin);
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
  servo.writeMicroseconds(speed_0);
  delay(7000);
  // PT sensor setup
  Wire.begin();
  if (!PT_sensor.init()) {
    Serial.println("PT NOT initialized");
  }
  PT_sensor.setModel(MS5837::MS5837_30BA);
  PT_sensor.setFluidDensity(1029); // kg/m^3 (freshwater, 1029 for seawater)
  PT_sensor.read();
  start_pressure = PT_sensor.pressure();
  // SD card setup
  Serial.print("Initializing SD...");
  pinMode(53, OUTPUT);
  if (!SD.begin(53)) {
    Serial.println("SD NOT initialized");
  }
  else{
    Serial.println("SD initialized");
  }
  // run servo at speed 1 for 3 seconds, then idle for 5 seconds
  servo.writeMicroseconds(1450);
  delay(3000);
  servo.writeMicroseconds(speed_0);
  delay(5000);
  // SD card logging begin
  sd = SD.open(file_name, FILE_WRITE);
  // log message for end of initialization
  sd.println();
  sd.println();
  sd.println("initialization done.");
  sd.flush();
}

void sleep(long time_ms) {
  // low power sleep as alternative to delay()
  // splits the total time into a series of 8s and 1s sleep periods (limitation of low power library)
  // subtract one second for re-initialization of servo after sleep
  int time_s = time_ms / 1000 - 1;
  int num_8s_sleeps = time_s / 8;
  int num_1s_sleeps = time_s - (8 * num_8s_sleeps);
  Serial.println("Sleeping...");
  sd.println("Sleeping...");
  // flush before sleep so stream isn't corrupted
  Serial.flush();
  sd.flush();
  for (int i = 0; i < num_8s_sleeps; i++){
    // keep timer 1 on (used by servo), SPI, and TWI (I2C) (not sure if needed)
    LowPower.idle(SLEEP_8S, ADC_OFF, TIMER5_OFF, TIMER4_OFF, TIMER3_OFF,
        TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART3_OFF,
        USART2_OFF, USART1_OFF, USART0_OFF, TWI_OFF);
  }
  for (int i = 0; i < num_1s_sleeps; i++){
    LowPower.idle(SLEEP_1S, ADC_OFF, TIMER5_OFF, TIMER4_OFF, TIMER3_OFF,
        TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART3_OFF,
        USART2_OFF, USART1_OFF, USART0_OFF, TWI_OFF);
  }
  // re-initialize servo
  Serial.println("Awake!");
  sd.println("Awake!");
  sd.flush();
  servo.writeMicroseconds(speed_0);
  delay(1000);
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

void check_pressure() {
  // make sure we are at least 2 m depth before continuing
  PT_sensor.read();
  while (PT_sensor.pressure() < start_pressure + 200) {
    Serial.print("Pressure: ");
    Serial.println(PT_sensor.pressure());
    Serial.print("Threshold pressure: ");
    Serial.println(start_pressure + 200);
    log_time_PT();
    Serial.println("Still at surface, waiting 10 s...");
    sd.println("Still at surface, waiting 10 s...");
    sd.flush();
    delay(10000);
    PT_sensor.read();
  }
  Serial.print("Pressure: ");
  Serial.println(PT_sensor.pressure());
  Serial.println("At sufficient depth. Continuing...");
  sd.print("Pressure: ");
  sd.println(PT_sensor.pressure());
  sd.println("At sufficient depth. Continuing...");
}

void flush() {
  Serial.println("Step 0");
  sd.println("Flushing at 1100 for 15s");
  servo.writeMicroseconds(1100);
  delay(14500);
  servo.writeMicroseconds(speed_0);
  delay(1000);
  sd.println("Flushing at 1900 for 15s");
  servo.writeMicroseconds(1900);
  delay(14500);
  sd.println("Flushing done. Wait 5 seconds");  
  servo.writeMicroseconds(speed_0);
  delay(5000);
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
  // flush for first cycle
  if(i == 0){
    check_pressure();
    Serial.println("Wait 300s...");
    sleep(250000);
    log_time_PT();
    flush();
  }
  // otherwise just wait 5 min between cycles
  else {
    Serial.println("Wait 300s...");
    sleep(250000);
    log_time_PT();
  }
  // run through all speeds
  if(i < num_cycles) {
    Serial.println("Cycle " + String(i + 1));
    for (int j = 0; j < num_speeds; j++) {
      run_speed(j);
    }
    finish_cycle();
  }
  // if we have run all depths/cycles, idle
  else {
    idle();
  }
}

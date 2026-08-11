//library inclusions
#include <LiquidCrystal.h>
#include <IRremote.h>
#include <EEPROM.h>

// led pins
#define redLED_pin 10
#define yellowLED_pin 11
#define greenLED_pin 5

// yellow led variables
unsigned long lastYellowLEDBlink = 0;
unsigned long yellowLEDDelay = 500;
int yellowLEDState = LOW;

// red led variables
unsigned long lastRedLEDBlink = 0;
unsigned long redLEDDelay = 100;
int redLEDState = LOW;

// lock & warning varables
double lockDistance = 15.0;
bool isLocked = false;
double warningDistance = 60.0;

// photoresistor pin
#define photoresistor_pin A2

// photoresistor variables
unsigned long lastReadLuminosity = 0;
unsigned long readLuminosityDelay = 100;

// lcd pins
#define lcd_rs_pin 9
#define lcd_e_pin 8
#define lcd_d4_pin 7
#define lcd_d5_pin 6
#define lcd_d6_pin A0
#define lcd_d7_pin A1

// lcd setup
LiquidCrystal lcd(lcd_rs_pin, lcd_e_pin, lcd_d4_pin,
                  lcd_d5_pin, lcd_d6_pin, lcd_d7_pin);

// lcd mode variables
#define lcd_mode_distance 0
#define lcd_mode_settings 1
#define lcd_mode_luminosity 2
int lcdMode = lcd_mode_distance;

// ir pin
#define ir_receiver_pin A3

// ir variables
#define ir_button_play 64
#define ir_button_off 69
#define ir_button_eq 25
#define ir_button_up 9
#define ir_button_down 7

// unit variables
#define distance_unit_cm 0
#define distance_unit_in 1
#define cm_to_in 0.393701
int distanceUnit = distance_unit_cm;

// eeprom address
#define eeprom_address_distance_unit 50

// ultrasonic sensor pins
#define trig_pin 4
#define echo_pin 3

// ultrasonic sensor variables
unsigned long lastUltrasonicTrig = 0;
unsigned long ultrasonicTrigDelay = 60;
volatile unsigned long pulseInBegin = 0;
volatile unsigned long pulseInEnd = 0;
volatile bool newDistanceAvailable = false;
double previousDistance = 400;

// toggles yellow led
void toggleYellowLED() {
  yellowLEDState = (yellowLEDState == HIGH) ? LOW : HIGH;
  digitalWrite(yellowLED_pin, yellowLEDState);
}

/* uses ultrasonic distance to determine yellow led blink rate and
if distance lower than blink rate defaults to 30 */
void setYellowLEDBlinkRateFromDistance(double distance) {
    yellowLEDDelay = distance * 4;
    if (yellowLEDDelay < 30) yellowLEDDelay = 30;
}

// toggles red led
void toggleRedLED() {
  redLEDState = (redLEDState == HIGH) ? LOW : HIGH;
  digitalWrite(redLED_pin, redLEDState);
}

// locks program
void lock() {
  if (!isLocked) {
    isLocked = true;
    yellowLEDState = LOW;
    redLEDState = LOW;
  }
}

// unlocks the program
void unlock() {
  if (isLocked) {
    isLocked = false;
    redLEDState = LOW;
    digitalWrite(redLED_pin, redLEDState);
    lcdMode = lcd_mode_distance;
    lcd.clear();
    }
}

// prints distance & messages to user on lcd
void printDistanceOnLCD(double distance) {
  if (isLocked) { // only runs is program is currently locked
    lcd.setCursor(0,0);
    lcd.print("!!! OBSTACLE !!!         ");
    lcd.setCursor(0,1);
    lcd.print("Press to unlock.         ");
  }
  else if (lcdMode == lcd_mode_distance) { // runs all the time unless program is locked
    lcd.setCursor(0,0);
    lcd.print("Dist: ");
    if (distanceUnit == distance_unit_in) {
      lcd.print(distance * cm_to_in);
      lcd.print(" in      ");  
    }
    else {
    lcd.print(distance);
    lcd.print(" cm        ");
    }
    lcd.setCursor(0,1);
    if (distance > warningDistance) { // checks if distance is too close
      lcd.print("No Obstacle.           ");
    }
    else {
      lcd.print("!! WARNING !!          ");
    }
  }
}

/* toggling up & down lcd distance interface, lcd settings interface,
and lcd luminosity interface depending on the boolean's state*/
void toggleLCDScreen(bool next) {
  switch (lcdMode) {
    case lcd_mode_distance: {
      lcdMode = (next) ? lcd_mode_settings : lcd_mode_luminosity;
      break;
    }
    case lcd_mode_settings: {
      lcdMode = (next) ? lcd_mode_luminosity : lcd_mode_distance;
      break;
    }
    case lcd_mode_luminosity: {
      lcdMode = (next) ? lcd_mode_distance : lcd_mode_settings;
      break;
    }
    default: {
      lcdMode = lcd_mode_distance;
    }
  }

  lcd.clear(); // clears screen when you switch to different interface

  if (lcdMode == lcd_mode_settings) { // only does this on settings interface
    lcd.setCursor(0,0);
    lcd.print("Press OFF to         ");
    lcd.setCursor(0,1);
    lcd.print("reset settings.      ");
  }
}

// resets the changable settings back to their default
void resetSettingsToDefault() {
  if (lcdMode == lcd_mode_settings) {
    distanceUnit = distance_unit_cm;
    EEPROM.write(eeprom_address_distance_unit, distanceUnit); // reverts back to cm in the memory since that is default
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Settings have       ");
    lcd.setCursor(0,1);
    lcd.print("been reset.         ");
  }
}

// toggling between cm and in units & storing into eeprom
void toggleDistanceUnit() {
  if (distanceUnit == distance_unit_cm) {
    distanceUnit = distance_unit_in;
  }
  else {
    distanceUnit = distance_unit_cm;
  }
  EEPROM.write(eeprom_address_distance_unit, distanceUnit); // stores current distance unit in memory
}

// tells program what to do when each button is clicked on the ir remote
void handleIRcommand(long command) {
  switch (command) {
    case ir_button_play: {
      unlock();
      break;
    }
    case ir_button_off: {
      resetSettingsToDefault();
      break;
    }
    case ir_button_eq: {
      toggleDistanceUnit();
      break;
    }
    case ir_button_up: {
      toggleLCDScreen(true);
      break;
    }
    case ir_button_down: {
      toggleLCDScreen(false);
      break;
    }
    default: {

    }
  }
}

// triggers ultrasonic sensor
void trigUltrasonicSensor() {
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_pin, LOW);
}

// echo interrupt catches time, start & end
void echoPinInterrupt() {
  if (digitalRead(echo_pin) == HIGH) {
    pulseInBegin = micros();
  }
  else {
    pulseInEnd = micros();
    newDistanceAvailable = true;
  }
}

// computes & returns distance measured by ultrasonic sensor
double getUltrasonicDistance() {
  double durationMicros = pulseInEnd - pulseInBegin;
  double distance = durationMicros / 58.0; // centimeters

  if (distance >= 400) { // only runs is distance captured is greater than max distance
    return previousDistance;
  }
  distance = previousDistance * 0.60 + distance * 0.40;
  previousDistance = distance;

  return distance;
}

/* maps the inverse brightness from luminosity gathered by
photoresistor and outputs that to the green led: the darker the
room the brighter the led, the brighter the room the darker the led*/
void setGreenLEDFromLuminosity(int luminosity) {
  int brightness = map(luminosity, 0, 1023, 255, 0);
  analogWrite(greenLED_pin, brightness);
}

// displays luminosity onto luminosity interface as long as program isn't locked
void printLuminosityOnLCD(int luminosity) {
  if ((!isLocked) && (lcdMode == lcd_mode_luminosity)) {
    lcd.setCursor(0,0);
    lcd.print("Luminosity: ");
    lcd.print(luminosity);
    lcd.print("            ");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(yellowLED_pin, OUTPUT);
  pinMode(redLED_pin, OUTPUT);
  pinMode(greenLED_pin, OUTPUT);
  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);

  lcd.begin(16,2);
  lcd.print("Initializing...");
  delay(1000);
  lcd.clear();

  distanceUnit = EEPROM.read(eeprom_address_distance_unit);
  if (distanceUnit == 255) {
    distanceUnit = distance_unit_cm;
  }

  IrReceiver.begin(ir_receiver_pin);

  attachInterrupt(digitalPinToInterrupt(echo_pin), 
                                        echoPinInterrupt, 
                                        CHANGE);
}

void loop() {
  unsigned long timeNow = millis();

  if (isLocked) { // only runs when program locked
    if (timeNow - lastRedLEDBlink >= redLEDDelay) {
      lastRedLEDBlink += redLEDDelay;
      toggleRedLED(); // toggles red led every 200 ms
      toggleYellowLED(); // toggles yellow led every 200 ms
    }
  }
  else {
    // every 500ms yellow led blinks
    if (timeNow - lastYellowLEDBlink >= yellowLEDDelay) {
      lastYellowLEDBlink += yellowLEDDelay;
      toggleYellowLED();
    }
  }

  if (IrReceiver.decode()) {
    IrReceiver.resume();
    long command = IrReceiver.decodedIRData.command;
    handleIRcommand(command);
  }

  // calls the ultrasonic sensor trigger function every 60ms
  if (timeNow - lastUltrasonicTrig >= ultrasonicTrigDelay) {
    lastUltrasonicTrig += ultrasonicTrigDelay;
    trigUltrasonicSensor();
  }

  /* calls the function which sets the blink rate for the yellow
  led, checks whether the program should be locked or not */
  if (newDistanceAvailable) {
    newDistanceAvailable = false;
    double distance = getUltrasonicDistance();
    setYellowLEDBlinkRateFromDistance(distance);
    printDistanceOnLCD(distance);
    if (distance < lockDistance) { // program locks if distance falls within the lock distance
      lock();
    }
  }

  if (timeNow - lastReadLuminosity >= readLuminosityDelay) {
    lastReadLuminosity += readLuminosityDelay;
    int luminosity = analogRead(photoresistor_pin);
    setGreenLEDFromLuminosity(luminosity);
    printLuminosityOnLCD(luminosity);
  }
}
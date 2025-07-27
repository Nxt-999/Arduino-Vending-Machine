/*
  Arduino Vending Machine v1.0
  Developed by Nxt-999 

  == Features ==
  - 2 products selectable via 3x4 keypad (# = confirm, * = reset)
  - Coin detection with IR sensor (no dispensing without payment)
  - 2 gear motors to control individual product slots
  - Servo-controlled flap for theft protection
  - LCD display for user guidance (supports €, ä, ü, ...)
  - Optional light sensor for product verification (currently disabled)
  - Fully locked process logic (no double input or bypassing)

  Hardware: Arduino Mega, IR sensor, 2 motors, 1 servo, 3x4 keypad, I2C LCD, [optional] light sensor

*/
//--------------------------------Libaries--------------------------------
#include <LiquidCrystal_I2C.h>  // LCD display via I2C
#include <Wire.h>               // I2C communication
#include <Servo.h>              // Servo motor control
#include <Keypad.h>             // Keypad input handling
//-------------------------------Euro-Zeichen-€---------------------------
byte euroChar[8] = {  // Defines pixels for Euro sign character
  B01110,
  B10000,
  B11110,
  B10000,
  B11110,
  B10000,
  B01110,
  B00000
};
//-----------------------------Ü-zeichen---------------------------------
byte uUmlaut[8] = {  // Defines pixels for 'ü' character
  B00101,
  B00000,
  B10001,
  B10001,
  B10001,
  B10001,
  B01111,
  B00000
};
//----------------------------ä-Zeichen-----------------------------------
byte aUmlaut[8] = {  // Defines pixels for 'ä' character
  B00101,
  B00000,
  B01110,
  B00001,
  B01111,
  B10001,
  B01111,
  B00000
};
//-------------------------------LCD+Servo---------------------------------
LiquidCrystal_I2C Lcd(0x27, 16, 2);  // Initialize LCD at I2C address 0x27, 16x2 chars
Servo myServo;                       // Create servo object for flap control
//------------------------Licht-Sensor-------------------------------
int lightSensor = A0;  // Light sensor analog pin
int lightValue = -1;   // Stores light sensor reading, -1 = invalid/no reading

// Typical values:
// 530 = empty
// 640 = product present

//-----------------------Infrarot-Sensor-----------------------------------
const int irPin = 18;  // IR sensor digital pin for coin detection (interrupt capable)
//-----------------------BOOL-Variables-----------------------------------
volatile bool irDetected = false;  // Flag: coin detected by IR sensor (interrupt)
bool servoOpen = false;            // Flag: servo flap opened
bool numberTyped = false;          // Flag: product number entered
bool motorTurned = false;          // Flag: motor has turned (product dispensed)
bool progressRunning = false;      // Flag: dispensing process ongoing
bool GeldAbfrageAngezeigt = false; // Flag: money insert prompt shown
bool angezeigt = false;            // Flag: something displayed on LCD
volatile bool interruptDone = false;  // Flag: interrupt routine finished (for debug)
//----------------------------MOTOR----------------------------------------
const int motorP1 = 9;      // Motor 1 control pin 1
const int motorP2 = 10;     // Motor 1 control pin 2
const int motorP3 = 11;     // Motor 2 control pin 1
const int motorP4 = 12;     // Motor 2 control pin 2
const int turnTime = 1000;  // Motor runtime in milliseconds for dispensing
//----------------------------KEYPAD---------------------------------------
const byte ROWS = 4;  // Number of keypad rows
const byte COLS = 3;  // Number of keypad columns
// Define keypad button symbols
char hexaKeys[ROWS][COLS] = {
  { '*', '0', '#' },
  { '7', '8', '9' },
  { '4', '5', '6' },
  { '1', '2', '3' },
};
byte rowPins[ROWS] = { 4, 2, 14, 3 };  // Connect keypad rows
byte colPins[COLS] = { 7, 6, 5 };      // Connect keypad columns
// Create keypad instance
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
String input = "";  // Stores user keypad input

//----------------------VOID-SETUP----------------------------------------
void setup() {
  attachInterrupt(digitalPinToInterrupt(18), detectIR, RISING);  // Interrupt on IR sensor pin rising edge
  // Function detectIR called when interrupt triggered

  //---------------------------LCD+Serial-Monitor-------------------------
  Serial.begin(9600);   // Start serial communication for debugging
  Lcd.init();           // Initialize LCD
  Lcd.backlight();      // Turn on LCD backlight
  showWelcomeScreen();  // Display welcome message

  //-------------------------MOTOR----------------------------------------
  pinMode(motorP1, OUTPUT);  // Set motor pins as outputs
  pinMode(motorP2, OUTPUT);
  pinMode(motorP3, OUTPUT);
  pinMode(motorP4, OUTPUT);

  digitalWrite(motorP1, LOW);  // Initialize motors off
  digitalWrite(motorP2, LOW);
  digitalWrite(motorP3, LOW);
  digitalWrite(motorP4, LOW);

  //--------------------------IR------------------------------------------
  pinMode(irPin, INPUT);  // Set IR sensor pin as input

  //---------------------------Servo---------------------------------------
  myServo.attach(22);  // Attach servo to pin 22
  myServo.write(0);    // Close flap initially
}

//----------------------------Welcome-Screen-------------------------------
void showWelcomeScreen() {  // Display prompt to select product
  Lcd.clear();
  Lcd.createChar(2, aUmlaut);  // Load custom 'ä' character in slot 2
  Lcd.setCursor(1, 0);
  Lcd.print("Bitte Produkt");   // "Please product"
  Lcd.setCursor(5, 1);
  Lcd.print("w");
  Lcd.write(2);                 // Print 'ä'
  Lcd.print("hlen");            // Completes "waehlen" (select)
}

//----------------------------Danke-Screen---------------------------------
void showDankeScreen() {  // Display thank you message
  Lcd.clear();
  Lcd.createChar(1, uUmlaut);  // Load custom 'ü' character in slot 1
  Lcd.setCursor(3, 0);
  Lcd.print("Danke f");
  Lcd.write(1);                // Print 'ü'
  Lcd.print("r");
  Lcd.setCursor(2, 1);
  Lcd.print("den Einkauf");   // "Thanks for the purchase"
}

//---------------------Geld-einwerfen-Screen-------------------------------
void showGeldEinwerfenScreen() {  // Prompt to insert 2€
  Lcd.clear();
  Lcd.createChar(0, euroChar);  // Load custom '€' character in slot 0
  Lcd.setCursor(4, 0);
  Lcd.print("Bitte ");
  Lcd.print("2");
  Lcd.write(byte(0));           // Print '€' sign

  Lcd.setCursor(3, 1);
  Lcd.print("einwerfen");       // "please insert"
}
void ProduktEntnehmenScreen() {  // Prompt to take product
  Lcd.clear();
  Lcd.setCursor(1, 0);
  Lcd.print("Bitte ");
  Lcd.print("Produkt");
  Lcd.setCursor(3, 1);
  Lcd.print("entnehmen");       // "please take"
}
//----------------------------VOID-DETECT-IR--------------------------------
void detectIR() {     // Interrupt routine for IR sensor
  irDetected = true;  // Mark coin detected
  interruptDone = true;  // For debug print in loop
}

//------------------------VOID-Ultraschallmessung---------------------------
void measureLight() {
  lightValue = analogRead(lightSensor);  // Read light sensor value
  Serial.println(lightValue);             // Print for debug
}

//------------------------VOID-LOOP-----------------------------------------
void loop() {
  if (!irDetected && !GeldAbfrageAngezeigt) {
    showGeldEinwerfenScreen();      // Prompt to insert money if none detected yet
    GeldAbfrageAngezeigt = true;
  }
  if (!progressRunning && irDetected) {  // If money inserted and no active process
    if (!angezeigt) {
      showWelcomeScreen();          // Show product selection prompt
      angezeigt = true;
    }
    char customKey1 = customKeypad.getKey();  // Read keypad input
    if (customKey1) {
      delay(50);                  // Debounce delay
      if (customKey1 >= '0' && customKey1 <= '9') {  // If number key pressed
        if (input.length() < 2) {    // Accept up to 2 digits
          input += customKey1;       // Append digit
          Serial.print("Aktuelle Eingabe:");
          Serial.println(input);     // Print current input
          Lcd.clear();
          Lcd.setCursor(7, 0);
          Lcd.print(input);          // Show input on LCD
        }
      } else if (customKey1 == '#') {  // Confirm input with '#'
        if (input.length() == 2) {     // Only if 2 digits entered
          Serial.print("Eingegebene Zahl:");
          Serial.println(input);        // Print final input
          Lcd.createChar(0, euroChar);  // Load Euro sign
          Lcd.setCursor(0, 0);
          Lcd.print("Produkt ");
          Lcd.print(input);
          Lcd.setCursor(0, 1);
          Lcd.print("Preis:");
          Lcd.print("   2");
          Lcd.write(byte(0));           // Show price 2€

          if (input.toInt() == 22)  // If product 22 selected
          {
            digitalWrite(motorP1, HIGH);  // Turn on motor 1 forward
            digitalWrite(motorP2, LOW);
            Serial.println("Motor gedreht");
            progressRunning = true;       // Mark process running
            motorTurned = true;           // Motor turned, start measurement
            numberTyped = true;           // Valid product number entered
            Lcd.clear();
            Lcd.setCursor(1, 0);
            Lcd.print("22 ausgegeben");  // "22 dispensed"
            delay(turnTime);              // Run motor for turnTime
            digitalWrite(motorP1, LOW);  // Stop motor
            digitalWrite(motorP2, LOW);

            delay(1000);  // Show message longer

            showDankeScreen();            // Show thank you
            delay(2000);
            ProduktEntnehmenScreen();    // Prompt to take product
          }
          if (input.toInt() == 58)  // If product 58 selected
          {
            digitalWrite(motorP3, LOW);  // Turn on motor 2 forward
            digitalWrite(motorP4, HIGH);
            progressRunning = true;
            motorTurned = true;
            numberTyped = true;
            Lcd.clear();
            Lcd.setCursor(1, 0);
            Lcd.print("58 ausgegeben");  // "58 dispensed"
            delay(turnTime);
            digitalWrite(motorP3, LOW);  // Stop motor
            digitalWrite(motorP4, LOW);

            delay(1000);  // Show message longer

            showDankeScreen();            // Show thank you
            delay(2000);
            ProduktEntnehmenScreen();    // Prompt to take product
          }
          if (input.toInt() != 58 && input.toInt() != 22) {  // If invalid product number
            Lcd.clear();
            Lcd.createChar(1, uUmlaut);  // Load 'ü'
            Lcd.setCursor(3, 0);
            Lcd.print("Ung");
            Lcd.write(1);                // 'ü'
            Lcd.print("ltige");
            Lcd.setCursor(6, 1);
            Lcd.print("Zahl");          // "Invalid number"
            Serial.println("Ungültige Zahl, wird zurückgesetzt");
            delay(2000);
            // Reset all flags and input
            servoOpen = false;
            numberTyped = false;
            motorTurned = false;
            progressRunning = false;
            GeldAbfrageAngezeigt = false;
            input = "";
            lightValue = -1;
            showWelcomeScreen();
          }
          input = "";  // Reset input after processing
          Serial.println("Bitte die nächste Zahl eingeben:");
        } else {
          // Input less than 2 digits is invalid
          Serial.println("Ungültige Eingabe. Bitte 2 Ziffern eingeben.");
          input = "";
          lightValue = -1;
          Lcd.clear();
          Lcd.createChar(1, uUmlaut);
          Lcd.setCursor(3, 0);
          Lcd.print("Ung");
          Lcd.write(1);
          Lcd.print("ltige");
          Lcd.setCursor(4, 1);
          Lcd.print("Eingabe");        // "Invalid input"
          delay(1000);
          progressRunning = false;
          showWelcomeScreen();
        }
      } else if (customKey1 == '*') {  // Reset input on '*'
        input = "";
        lightValue = -1;
        GeldAbfrageAngezeigt = false;
        Serial.println("Eingabe zurückgesetzt");
        Serial.println("Bitte die nächste Zahl eingeben:");
        Lcd.clear();
        Lcd.createChar(1, uUmlaut);
        Lcd.setCursor(4, 0);
        Lcd.print("Eingabe");
        Lcd.setCursor(1, 1);
        Lcd.print("Zur");
        Lcd.write(1);
        Lcd.print("ckgesetzt");        // "Input reset"
        delay(1000);
      }
    }
  }

  //-----------------------------Coin detected print-----------------------------
  if (interruptDone) {
    Serial.println("Coin detected, interrupt worked");
    interruptDone = false;
  }

  //----------------------------Light sensor measurement------------------------------
  if (motorTurned) {                       // If motor turned (product dispensed)
    lightValue = analogRead(lightSensor);  // Read light sensor value
    Serial.println(lightValue);             // Debug output
  }

  //--------------------------Open dispensing flap---------------------------------
  if (irDetected && /*lightValue > 640 && lightValue < 670 &&*/ !servoOpen && numberTyped) {  
    // Check coin inserted, flap closed, and product selected
    myServo.write(90);              // Open flap
    Serial.println("Servo moved");

    irDetected = false;             // Reset coin detected flag
    servoOpen = true;               // Mark flap as open

    Lcd.clear();
    Lcd.setCursor(1, 0);
    Lcd.print("Bitte ");
    Lcd.print("Produkt");
    Lcd.setCursor(3, 1);
    Lcd.print("entnehmen");        // "Please take product"
  }

  //------------------------Close dispensing flap--------------------------------
  // Close flap after product taken and delay
  if (servoOpen /*&& lightValue < 660*/) {  
    delay(7000);                   // Wait 7 seconds
    myServo.write(0);             // Close flap

    // Reset all flags for next use
    irDetected = false;
    servoOpen = false;
    numberTyped = false;
    motorTurned = false;
    progressRunning = false;
    GeldAbfrageAngezeigt = false;
    angezeigt = false;
    input = "";
    lightValue = -1;

    Lcd.clear();
    Lcd.setCursor(4, 0);
    Lcd.print("Produkt");
    Lcd.setCursor(3, 1);
    Lcd.print("entnommen");       // "Product taken"
    delay(1500);

    showWelcomeScreen();           // Show welcome screen again
    Serial.println("Program reset");
    Serial.println("Measurement stopped");
  }
  delay(1);  // Small delay for stability and debounce
}

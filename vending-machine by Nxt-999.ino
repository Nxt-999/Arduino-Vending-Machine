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
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Servo.h>
#include <Keypad.h>
//-------------------------------Euro-Zeichen-€---------------------------
byte euroChar[8] = {  //Erstellt bzw. definiert welche Pixel für "€" leuchten müssen
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
byte uUmlaut[8] = {  //Erstellt bzw. definiert welche Pixel für "ü" leuchten müssen
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
byte aUmlaut[8] = {  //Erstellt bzw. definiert welche Pixel für "ä" leuchten müssen
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
LiquidCrystal_I2C Lcd(0x27, 16, 2);  //LCD Adresse ansprechen und Feldgröße definieren, damit LCD Befehle funktionieren
Servo myServo;                       //Zugriff auf Servo Libary, damit Befehle wie "myServo.write(90)" funktionieren
//------------------------Licht-Sensor-------------------------------
int lightSensor = A0;  // HC-SR501 Ausgang an Pin 2
int lightValue = -1;   //lightValue ist der Wert des Lichtsensors, er wird auf "-1" gesetzt, das der Wert außerhalb seiner Verwendung wie "" ist (wie bei string x="")

//530 wenn leer
//640 wenn ware

//-----------------------Infrarot-Sensor-----------------------------------
const int irPin = 18;  // interrupts: 2, 3, >>>18<<<, 19, 20, and 21
//-----------------------BOOL-Varbiablen-----------------------------------
volatile bool irDetected = false;  //irDetected wird auf false gesetzt // volatile=global ==> gilt für alle voids
bool servoOpen = false;            // kontrolliert ob der Servo schon geöffnet wurde (für Ausgabeklappe)
bool numberTyped = false;          // kontrolliert ob schon eine zahl eingegeben wurde
bool motorTurned = false;          // kontrolliert ob sich der Motor schon gedreht hat, damit der Ultraschall nur misst, nachdem das Produkt ausgegeben wurde
bool progressRunning = false;      // kontrolliert ob der schon eine Zahl eingegeben und bestätigt wurde, sodass danach bis zum Ende des Prozesses(Ausgabe) keine neuen zahlen eingegeben werden können
bool GeldAbfrageAngezeigt = false; 
bool angezeigt = false;
volatile bool interruptDone = false;  //Temporär, um zu testen ob interrupt ausgelöst wurde (If schleife mit Serial print)
//----------------------------MOTOR----------------------------------------
const int motorP1 = 9;      //motorP1+motorP2= Motor1
const int motorP2 = 10;     //
const int motorP3 = 11;     //motorP3+motorP4= Motor 2
const int motorP4 = 12;     //
const int turnTime = 1000;  // Bestimmt wie lange sich der Motor drehen soll
//----------------------------KEYPAD---------------------------------------
const byte ROWS = 4;  // vier Reihen
const byte COLS = 3;  // drei Spalten
// define the symbols on the buttons of the keypad
char hexaKeys[ROWS][COLS] = {
  { '*', '0', '#' },
  { '7', '8', '9' },
  { '4', '5', '6' },
  { '1', '2', '3' },
};
byte rowPins[ROWS] = { 4, 2, 14, 3 };  // verbinde mit den Zeilen-Pins des Keypads
byte colPins[COLS] = { 7, 6, 5 };      // verbinde mit den Spalten-Pins des Keypads
// initialisiere eine Instanz der Klasse Keypad
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
String input = "";  // String, um die gedrückten Tasten zu speichern

//----------------------VOID-SETUP----------------------------------------
void setup() {
  attachInterrupt(digitalPinToInterrupt(18), detectIR, RISING);  //Interrupt für Pin 18(IR-Sensor)erstellen. Interrupt wird ausgelöst wenn Pin von "LOW" auf "HIGH" geht (RISING)
  //detectIR definiert den "Interrupt Loop" ==> "void detectIR() {...}"
  //---------------------------LCD+Serial-Monitor-------------------------
  Serial.begin(9600);   //Serieller Monitor wird eingerichtet
  Lcd.init();           //Lcd wird initialisiert
  Lcd.backlight();      //Lcd-Backlight wird angeschlaten
  showWelcomeScreen();  //Geht zu void showWelcomeScreen() {}, vermeidet Wiederholungen ==> mehr Performance
  //-------------------------MOTOR----------------------------------------
  pinMode(motorP1, OUTPUT);
  pinMode(motorP2, OUTPUT);
  pinMode(motorP3, OUTPUT);
  pinMode(motorP4, OUTPUT);

  digitalWrite(motorP1, LOW);
  digitalWrite(motorP2, LOW);
  digitalWrite(motorP3, LOW);
  digitalWrite(motorP4, LOW);

  //--------------------------IR------------------------------------------
  pinMode(irPin, INPUT);

  //---------------------------Servo---------------------------------------
  myServo.attach(22);  //Servo PIN
  myServo.write(0);    // Klappe zu (Servo Position=0)
}

//----------------------------Welcome-Screen-------------------------------
void showWelcomeScreen() {  //Einfacher zu benutzen, da nur showWelcomeScreen(); geschrieben werden muss, um die folgenden Texte anzuzeigen
  Lcd.clear();
  Lcd.createChar(2, aUmlaut);  // Speichert das Zeichen "ä" als benutzerdefiniertes Zeichen am Speicherplatz "2"
  Lcd.setCursor(1, 0);
  Lcd.print("Bitte Produkt");
  Lcd.setCursor(5, 1);
  Lcd.print("w");
  Lcd.write(2);  //Zeigt "ä" an
  Lcd.print("hlen");
}

//----------------------------Danke-Screen---------------------------------
void showDankeScreen() {  //Zeigt immer LCD-Danke-Screen wenn man showDankeScreen(); schreibt ==> vereinfach/verkürzt Programm + mehr Übersicht
  Lcd.clear();
  Lcd.createChar(1, uUmlaut);  // Speichert das Zeichen "ü" als benutzerdefiniertes Zeichen am Speicherplatz "1"
  Lcd.setCursor(3, 0);
  Lcd.print("Danke f");
  Lcd.write(1);  // zeigt "ü" an
  Lcd.print("r");
  Lcd.setCursor(2, 1);
  Lcd.print("den Einkauf");
}

//---------------------Geld-einwerfen-Screen-------------------------------
void showGeldEinwerfenScreen() {  //Zeigt immer LCD-Bitte Geld Einwerfen Screen wenn man showGeldEinwerfenScreen(); schreibt ==> vereinfach/verkürzt Programm + mehr Übersicht
  Lcd.clear();

  Lcd.createChar(0, euroChar);  // Speichert das Zeichen "€" als benutzerdefiniertes Zeichen am Speicherplatz "0"
  Lcd.setCursor(4, 0);
  Lcd.print("Bitte ");
  Lcd.print("2");
  Lcd.write(byte(0));  //Zeigt "€ "an

  Lcd.setCursor(3, 1);
  Lcd.print("einwerfen");
}
void ProduktEntnehmenScreen() {
  Lcd.clear();
  Lcd.setCursor(1, 0);
  Lcd.print("Bitte ");
  Lcd.print("Produkt");
  Lcd.setCursor(3, 1);
  Lcd.print("entnehmen");
}
//----------------------------VOID-DETECT-IR--------------------------------
void detectIR() {     //Für Interrupt/Ir Messung mit Performance Verbesserung
  irDetected = true;  //irDetected=true; bedeutet Münze erkannt | Diese Flag gilt auch in loop()
  interruptDone = true;
}

//------------------------VOID-Ultraschallmessung---------------------------
void measureLight() {
  lightValue = analogRead(lightSensor);
  Serial.println(lightValue);
}

//------------------------VOID-LOOP-----------------------------------------
void loop() {
  if (!irDetected && !GeldAbfrageAngezeigt) {
    showGeldEinwerfenScreen();
    GeldAbfrageAngezeigt = true;

  }
  if (!progressRunning && irDetected) {  //Wenn der Prozess noch nicht läuft(der Motor sich noch nicht gedreht hat)
    if (!angezeigt) {
      showWelcomeScreen();
      angezeigt = true;
    }
    char customKey1 = customKeypad.getKey();  //"customKeypad.getKey" stammt aus der !Keypad.h"-Libary und erkennt die Eingabe, Eingabe wird = "custom Key1" gesetzt (Zwischenspeicher)
    if (customKey1) {
      delay(50);                                     //Debouncing
      if (customKey1 >= '0' && customKey1 <= '9') {  // Überprüfen, ob die gedrückte Taste eine Zahl ist (zwischen 1 und 9)
        // Wenn die Eingabe weniger als 2 Ziffern hat, füge die Zahl hinzu
        if (input.length() < 2) {  //Wenn input < 2
          input += customKey1;     //Dann input + 2. input (neuer customKey)
          Serial.print("Aktuelle Eingabe:");
          Serial.println(input);  // Zeige die aktuelle Eingabe an
          Lcd.clear();
          Lcd.setCursor(7, 0);
          Lcd.print(input);
        }
      } else if (customKey1 == '#') {  // Wenn # gedrückt wird, geben wir die vollständige Zahl aus
        if (input.length() == 2) {     // Nur wenn es 2 Ziffern gibt
          Serial.print("Eingegebene Zahl:");
          Serial.println(input);        // Finale Eingabe
          Lcd.createChar(0, euroChar);  // Speichert das Zeichen "€" als benutzerdefiniertes Zeichen am Speicherplatz "0"
          Lcd.setCursor(0, 0);
          Lcd.print("Produkt ");
          Lcd.print(input);
          Lcd.setCursor(0, 1);
          Lcd.print("Preis:");
          Lcd.print("   2");
          Lcd.write(byte(0));  //Zeigt "€ "an


          if (input.toInt() == 22)  //String zu Int umwandeln und prüfen ob = 22
          {

            digitalWrite(motorP1, HIGH);  //Ausgabemotor anschalten
            digitalWrite(motorP2, LOW);
            Serial.println("Motor gedreht");
            progressRunning = true;       //Prozess wird damit als laufend definiert ==> keine neuen Eingaben möglich bis Abschluss
            motorTurned = true;           //Motor wird als gedreht definiert ==> Ultraschallmessung beginnt
            numberTyped = true;           //Nummer wurde eingegeben (wichtig für spätere Ausgabeklappenöffnung)
            Lcd.clear();
            Lcd.setCursor(1, 0);
            Lcd.print("22 ausgegeben");
            delay(turnTime);
            digitalWrite(motorP1, LOW);  //Ausgabemotor ausschalten
            digitalWrite(motorP2, LOW);  //

            delay(1000);  // Damit "22 ausgegeben" länger angezeigt wird

            showDankeScreen();
            delay(2000);
            ProduktEntnehmenScreen();


          }
          if (input.toInt() == 58)  //String zu Int umwandeln und prüfen ob = 58
          {

            digitalWrite(motorP3, LOW);  //Ausgabemotor anschalten
            digitalWrite(motorP4, HIGH);   //
            progressRunning = true;
            motorTurned = true;
            numberTyped = true;
            Lcd.clear();
            Lcd.setCursor(1, 0);
            Lcd.print("58 ausgegeben");
            delay(turnTime);
            digitalWrite(motorP3, LOW);  //Ausgabemotor ausschalten
            digitalWrite(motorP4, LOW);  //

            delay(1000);  // Damit "58 ausgegeben" länger angezeigt wird

            showDankeScreen();
            delay(2000);
            ProduktEntnehmenScreen();

          }
          if (input.toInt() != 58 && input.toInt() != 22) {  //Wenn ungültige Zahl, also wenn nicht 22 & nicht 58
            Lcd.clear();
            Lcd.createChar(1, uUmlaut);  // Speichert das Zeichen "ü" als benutzerdefiniertes Zeichen am Speicherplatz "1"
            Lcd.setCursor(3, 0);
            Lcd.print("Ung");
            Lcd.write(1);  // zeigt "ü" an
            Lcd.print("ltige");
            Lcd.setCursor(6, 1);
            Lcd.print("Zahl");
            Serial.println("Ungültige Zahl, wird zurückgesetzt");
            delay(2000);
            servoOpen = false;
            numberTyped = false;
            motorTurned = false;
            progressRunning = false;
            GeldAbfrageAngezeigt = false;
            input = "";  //Eingabe zurücksetzen
            lightValue = -1;
            showWelcomeScreen();
          }
          // Zeige die vollständige Zahl an
          input = "";  // Zurücksetzen der Eingabe nach Ausgabe
          Serial.println("Bitte die nächste Zahl eingeben:");
        } else {
          // Wenn die Eingabe weniger als 2 Ziffern hat, gebe keine Zahl aus
          Serial.println("Ungültige Eingabe. Bitte 2 Ziffern eingeben.");
          input = "";  // Eingabe zurücksetzen
          lightValue = -1;
          Lcd.clear();
          Lcd.createChar(1, uUmlaut);  // Speichert das Zeichen "ü" als benutzerdefiniertes Zeichen am Speicherplatz "1"
          Lcd.setCursor(3, 0);
          Lcd.print("Ung");
          Lcd.write(1);  // zeigt "ü" an
          Lcd.print("ltige");
          Lcd.setCursor(4, 1);
          Lcd.print("Eingabe");
          delay(1000);
          progressRunning = false;
          showWelcomeScreen();
        }
      } else if (customKey1 == '*') {  // Wenn * gedrückt wird, Eingabe zurücksetzen
        input = "";                    // Eingabe zurücksetzen
        lightValue = -1;
        GeldAbfrageAngezeigt = false;
        Serial.println("Eingabe zurückgesetzt");
        Serial.println("Bitte die nächste Zahl eingeben:");
        Lcd.clear();
        Lcd.createChar(1, uUmlaut);  // Speichert das Zeichen "ü" als benutzerdefiniertes Zeichen am Speicherplatz "1"
        Lcd.setCursor(4, 0);
        Lcd.print("Eingabe");
        Lcd.setCursor(1, 1);
        Lcd.print("Zur");
        Lcd.write(1);  // zeigt "ü" an
        Lcd.print("ckgesetzt");
        delay(1000);
      }
    }
  }

  //-----------------------------Münze-erkannt-print-----------------------------
  if (interruptDone) {
    Serial.println("Münze erkannt, interrupt hat geklappt");
    interruptDone = false;
  }

  //----------------------------Helligkeits-Messung------------------------------
  if (motorTurned) {                       //Wenn Motor gedreht
    lightValue = analogRead(lightSensor);  //Lese SensorPin von Lichtsensor aus und setze = lightValue
    Serial.println(lightValue);
  }

  //--------------------------Auswurfklappe-öffnen---------------------------------
  if (irDetected && /*lightValue > 640 && lightValue < 670 &&*/ !servoOpen && numberTyped) {  //Prüfung ob Münze eingeworfen wurde(irDetected) + ob Lichtsensor produkt erkannt hat
    myServo.write(90);                                                                    //und ob der Servo noch nicht geöffnet wurde und ob eine gültige Zahl eingegeben wurde(Keypad)
    Serial.println("Servo gedreht");                                                      //Servo öffnen damit Klappe aufgeht

    irDetected = false;
    servoOpen = true;  // Servo Status auf wurde geöffnet setzen

    Lcd.clear();
    Lcd.setCursor(1, 0);
    Lcd.print("Bitte ");
    Lcd.print("Produkt");
    Lcd.setCursor(3, 1);
    Lcd.print("entnehmen");
  }

  //------------------------Auswurfkapppe-schließen--------------------------------
  // Wenn Klappe offen und Produkt entnommen → schließen nach 5 Sekunden
  if (servoOpen /*&& lightValue < 660*/) {  //Wenn Servo geöffnet war und die Helligkeit dann wieder über 650 geht, alles zurücksetzen für neue Durchgänge
    delay(7000);
    myServo.write(0);

    irDetected = false;            //Alle bool-Variablen zurücksetzen (full reset)
    servoOpen = false;             //Servo schon geöffnet zurücksetzen
    numberTyped = false;           //Nummer eingegeben zurücksetzen
    motorTurned = false;           //Ausgabemotor gedreht zurücksetzen
    progressRunning = false;       //Aktiver Prozess auf nicht aktiv setzen
    GeldAbfrageAngezeigt = false;
    angezeigt = false;
    input = "";                    //Eingabe zurücksetzen
    lightValue = -1;               //Zurücksetzen, damit Klappe nicht versehentlich aufgeht

    Lcd.clear();
    Lcd.setCursor(4, 0);
    Lcd.print("Produkt");
    Lcd.setCursor(3, 1);
    Lcd.print("entnommen");
    delay(1500);

    showWelcomeScreen();
    Serial.println("Programm resetted");
    Serial.println("Messung gestoppt");
  }
  delay(1);  //Stabilitäts-delay / Debouncing
}

#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

#include <Time.h>

/* ----------------------------------------
  Liverpool Nucleasr Physics LN2 Fill System

  Arduino bridge code to recieve commands
  from python based server code running
  elsewhere

  List of commands to be implemented:
    Open/Close valve to tank
    Open/Close LN2 line
    Read LED potential (for given line number)
    Read valve state (for given line number)
    Initiate fill (line numer)
    Initiate fill (all active lines)
    Show staus of system, including:
      any LED V
      any Valve Status
      Last fill success/fail and if success then duration
      Current system time
  ---------------------------------------- */

// Definitions
#define VALVEOPEN 1
#define VALVECLOSED 0

#define FILLHOLDTIME 20
#define FILLTIMEOUT 300

#define MAXMESSAGESIZE 2048
#define MAXTEMPMESSAGESIZE 1024

#define SUPPLYTANKPIN 2  // GPIO# used to open supply valve
#define NUMFILLLINES 4  // Total number of fill lines being managed

// Global variables
BridgeServer Server;  // server for handling requests/commands
String StatusHeader;
String StatusMessage; // Current system status message

// System configuration
const char * LineNames[NUMFILLLINES] = {    // Names of lines
  "Test1",
  "Test2",
  "Test3",
  "Test4"
};

const short LineValvePins[NUMFILLLINES] = {  // GPIO# used for operating each line
  8, 9, 10, 11
};

const short LineLedPins[NUMFILLLINES] = {    // ADC# used to read LED for each line
  0, 1, 2, 3
};

const float LineLedThresh[NUMFILLLINES] = {  // Threshold volts accross LED to class as "cold"
  3.0, 3.0, 3.0, 3.0
};

const bool LineActive[NUMFILLLINES] = {  // Is this line active (used for "fillall" command)
  1, 0, 0, 0
};

int LineFillStatus[NUMFILLLINES] = {  // Was the last fill a success (=fill time) or failure (=0)
  0, 0, 0, 0
};

// Globals for managing ongoing fill
int NumFilling = 0;  // Number of lines currently being filled
bool Filling[NUMFILLLINES] = {
  0, 0, 0, 0
};
time_t FillStartTime[NUMFILLLINES] = {
  0, 0, 0, 0
};
time_t ColdStartTime[NUMFILLLINES] = {
  0, 0, 0, 0
};
String LineLastFillData[NUMFILLLINES] = {
  "", "", "", ""
};

// Setup and main loop
// ----------------------------------
void setup() {

  // Initialise seriel comms
  Serial.begin(9600);

  // Initialise pins
  int Pin, Line;
  pinMode(SUPPLYTANKPIN, OUTPUT);  // Main supply valve control
  digitalWrite(SUPPLYTANKPIN, VALVECLOSED);
  for (Line = 0; Line < NUMFILLLINES; Line++) { // Individual line valves
    Pin = LineValvePins[Line];
    pinMode(Pin, OUTPUT);
    digitalWrite(Pin, VALVECLOSED);
  }
  pinMode(13, OUTPUT); // General status LED.

  // Initialise bridge library and start a
  // server to listen for commands
  Bridge.begin();
  Server.listenOnLocalhost();
  Server.begin();

}

void loop() {

  // Get clients coming from server
  BridgeClient Client = Server.accept();

  // Update current status message
  // updatestatus();

  // Check on any ongoing line fills
  updatefill();

  // Is there a new client?  If so process its request
  if (Client) {
    // Process request
    process(Client);
    // Close connection and free resources.
    Client.stop();
  }

  // Delay until next loop
  delay(50); // Poll every 50ms at most

}

// Process request
// ----------------------------------

void process(BridgeClient Client) {
  // read the Command
  String Command = Client.readStringUntil('/');

  // Direct open/close valve commands
  if (Command == "openline") {
    openline(Client);
  }
  if (Command == "closeline") {
    closeline(Client);
  }
  if (Command == "opentank") {
    opentank(Client);
  }
  if (Command == "closetank") {
    closetank(Client);
  }

  // Read status
  if (Command == "updatestatus") {
    updatestatus(Client);
  }
  if (Command == "readled") { // Read specific LED value
    readled(Client);
  }
  if (Command == "readvalve") { // Read valve status
    readvalve(Client);
  }
  if (Command == "showtime") {
    showtime(Client);
  }

  // Initiiate fill cycle commands
  if (Command == "fillline") {
    fillline(Client);
  }

  if (Command == "fillall") {
    fillall(Client);
  }

  // Line status command


  // Following commands are from the example code I used for building this - CU
  // is "digital" Command?
  if (Command == "digital") {
    digitalCommand(Client);
  }

  // is "analog" Command?
  if (Command == "analog") {
    analogCommand(Client);
  }

  if (Command == "testprint") {
    testprint(Client);
  }

}


// Functions for directly opening/closing valves
// -----------------------------------------------

void openline(BridgeClient Client) {
  int LineNumber, Pin;

  LineNumber = Client.parseInt();

  if (LineNumber > NUMFILLLINES) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineValvePins[LineNumber - 1];

  digitalWrite(Pin, VALVEOPEN);

  Client.print(F("Opening line "));
  Client.print(LineNumber);
  Client.print(F(" (Pin"));
  Client.print(Pin);
  Client.print(F(")..."));
}

void closeline(BridgeClient Client) {
  int LineNumber, Pin;

  LineNumber = Client.parseInt();

  if (LineNumber > NUMFILLLINES) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineValvePins[LineNumber - 1];

  digitalWrite(Pin, VALVECLOSED);

  Client.print(F("Closing line "));
  Client.print(LineNumber);
  Client.print(F(" (Pin"));
  Client.print(Pin);
  Client.print(F(")..."));
}

void opentank(BridgeClient Client) {

  digitalWrite(SUPPLYTANKPIN, VALVEOPEN);

  Client.print(F("Opening main supply tank (Pin "));
  Client.print(SUPPLYTANKPIN);
  Client.print(F(")..."));
}

void closetank(BridgeClient Client) {

  digitalWrite(SUPPLYTANKPIN, VALVECLOSED);

  Client.print(F("Closing main supply tank (Pin "));
  Client.print(SUPPLYTANKPIN);
  Client.print(F(")..."));
}


// Functions for reading states
// ----------------------------------

void readled(BridgeClient Client) {
  int LineNumber, Pin, Value ;

  LineNumber = Client.parseInt();

  if (LineNumber > NUMFILLLINES) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineLedPins[LineNumber - 1];
  Value = analogRead(Pin);

  Client.print(F("Line "));
  Client.print(LineNumber);
  Client.print(F(" (pin "));
  Client.print(Pin);
  Client.print(F(") reads "));
  Client.print(Value);
  Client.print(F(" ("));
  Client.print(Adc2Volts(Value));
  Client.print(F(" Volts)"));
}

void readvalve(BridgeClient Client) {
  int LineNumber, Pin, Value ;

  LineNumber = Client.parseInt();

  if (LineNumber > NUMFILLLINES) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineValvePins[LineNumber - 1];
  Value = (digitalRead(Pin) == VALVEOPEN);

  Client.print(F("Valve for line "));
  Client.print(LineNumber);
  Client.print(F(" is "));
  Client.print(Value ? "Open." : "Closed.");
}

void showtime(BridgeClient Client) {
  // This function should display the current system time
  char s[MAXTEMPMESSAGESIZE];
  time_t Now = now();
  sprintf(s, "Current time is %d (%d bytes)\n", Now, sizeof(Now));
  Client.print(s);
  return;
}

void updatestatus(BridgeClient Client) {

  int i;
  int AdcVal;
  bool ValveOpen;
  int Status;

  Client.print(F("# University of Liverpool - Nuclear Physics - LN2 Fill System\n\n# Full Fill-line Status Report:\n"));
  Client.print(F("Current system time: "));
  Client.print(now());
  Client.print("\n");
  Client.print("Main tank valve is ");
  Client.print(digitalRead(LineValvePins[i]) == VALVEOPEN ? "Op\n" : "Cl\n");
  Client.print(F("# LineNum\tActive?\tLED Pin\tLED Thresh\tADC val\tLED V\tValve Pin\tValve Status\tLast Fill Status\t\n\n"));

  for (i = 0; i < NUMFILLLINES; i++) {
    AdcVal = analogRead(LineLedPins[i]);
    ValveOpen = (digitalRead(LineValvePins[i]) == VALVEOPEN);

    Client.print(i);
    Client.print("\t\t");
    Client.print(LineActive[i] == 1 ? "Y" : "N");
    Client.print("\t");
    Client.print(LineLedPins[i]);
    Client.print("\t");
    Client.print(LineLedThresh[i]);
    Client.print("\t\t");
    Client.print(AdcVal);
    Client.print("\t");
    Client.print(Adc2Volts(AdcVal));
    Client.print("\t");
    Client.print(LineValvePins[i]);
    Client.print("\t\t");
    Client.print(ValveOpen ? "Op" : "Cl");

    Client.print("\t\t\t");

    if (Filling[i] == 0) {
      Client.print(LineFillStatus[i] ? "Succ! (" : "Fail! (");
      Client.print(LineFillStatus[i]);
      Client.print(")");
    } else {
      Client.print("Fill underway!!");
    }   
    Client.print("\n");
     
  }
  return;
}

void testprint(BridgeClient Client) {
  int i, N;  
  N = Client.parseInt();
  for (i=0; i<N; i++) {
    Client.print(i);
    Client.print("\n");
  }
  return;
}


// Functions for initiating/managing fill cylce
// -----------------------------------------------

// Fill a single line,
int fillline(BridgeClient Client) {
  int LineNumber;
  int FillTime = 0;

  float LedVolts = 0.0;

  LineNumber = Client.parseInt();

  if (LineNumber > NUMFILLLINES) {
    Client.print(F("Unrecognised Line Number!"));
    return -1;
  }

  Client.print(F("Filling line "));
  Client.print(LineNumber);
  Client.print(F(" please wait...\n\n"));
  Client.print(F("Time(s)\tLED Volts\tStatus\n"));

  short ValvePin = LineValvePins[LineNumber - 1];
  short LedPin = LineLedPins[LineNumber - 1];

  // Open main valve to tank
  digitalWrite(SUPPLYTANKPIN, VALVEOPEN);

  // Open valve to line
  digitalWrite(ValvePin, VALVEOPEN);

  int LedOverThreshTime = 0;
  // Monitor LED and output value until success or timeout
  while (FillTime < FILLTIMEOUT) {

    LedVolts = Adc2Volts(analogRead(LedPin));

    if (LedVolts > LineLedThresh[LineNumber - 1]) {
      LedOverThreshTime += 1;
    } else {
      LedOverThreshTime = 0;
    }

    if (LedOverThreshTime >= FILLHOLDTIME) {
      Client.print(F("Fill complete, closing valves..."));
      break;
    }

    Client.print(FillTime);
    Client.print("\t");
    Client.print(LedVolts);
    Client.print("\t");
    Client.print(LedVolts > LineLedThresh[LineNumber - 1] ? "Cold\n" : "Warm\n");

    Client.flush();

    FillTime += 1;

    delay(1000); // Wait 1s

  }

  if (FillTime < FILLTIMEOUT) {
    Client.print("\nSuccess!!!");
    LineFillStatus[LineNumber - 1] = FillTime;
  }
  else {
    Client.print("\nFail!!!");
    LineFillStatus[LineNumber - 1] = 0;
  }

  // Close valve
  digitalWrite(ValvePin, VALVECLOSED);

  // Close tank
  digitalWrite(SUPPLYTANKPIN, VALVECLOSED);

  // return fill time
  return FillTime;
}

// Fill all active lines
void fillall(BridgeClient Client) {

  Client.print("Filling all lines...\n\n");

  // Open main valve to tank
  Client.print("Opening supply tank valve...");
  digitalWrite(SUPPLYTANKPIN, VALVEOPEN);

  if (NumFilling > 0) {
    Client.print("Fill already underway.  Try reading status.");
    return;
  }
  NumFilling = 0;

  for (int i = 0; i < NUMFILLLINES; i++) {
    if (LineActive[i] == 1) {
      Filling[i] = 1;
      NumFilling += 1;
      digitalWrite(LineValvePins[i], VALVEOPEN);
      FillStartTime[i] = now();
      ColdStartTime[i] = 0;
      char s[MAXTEMPMESSAGESIZE];
      sprintf(s, "Opening line %d at %d\n", (i + 1), FillStartTime[i]);
      Client.print(s);
    }
  }
}

// Update any ongoing fills
void updatefill() {
  int ColdTime, FillTime;
  float LedVolts;
  short LedPin, ValvePin;

  // Loop all fill lines and check if currently "Active" and "Filling"
  for (int i = 0; i < NUMFILLLINES; i++) {
    if (LineActive[i] == 1 && Filling[i] == 1) {
      // Check if cold already
      if (ColdStartTime[i] > 0) {
        // Check if still cold
        LedPin = LineLedPins[i];
        LedVolts = Adc2Volts(analogRead(LedPin));
        if (LedVolts > LineLedThresh[i]) {
          // if so measure time cold
          ColdTime = int(now() - ColdStartTime[i]);
          // see if time > required time cold
          if (ColdTime >= FILLHOLDTIME) {
            // if so shut line and record fill success, adjust num filling
            // Close valve
            ValvePin = LineValvePins[i];
            digitalWrite(ValvePin, VALVECLOSED);
            // Record fill success
            NumFilling -= 1;
            LineFillStatus[i] = int(now() - FillStartTime[i]);
            Filling[i] = 0;
            // Reset fill times
            ColdStartTime[i] = 0;
            FillStartTime[i] = 0;
          }
        }
        else {  // If no longer cold, reset to warm status
          ColdStartTime[i] = 0;
        }
      }
      else {  // If not cold alread...
        // Is it cold now?
        LedPin = LineLedPins[i];
        LedVolts = Adc2Volts(analogRead(LedPin));
        if (LedVolts > LineLedThresh[i]) {
          // If so set time cold
          ColdStartTime[i] = now();
        }
      }
      // Whether cold or warm, is max time exceeded?
      ColdTime = int(now() - ColdStartTime[i]);
      if (ColdTime >= FILLTIMEOUT) {
        // If so shut off and record fill fail, adjust numfilling
        ValvePin = LineValvePins[i];
        digitalWrite(ValvePin, VALVECLOSED);
        // Record fill failure
        NumFilling -= 1;
        LineFillStatus[i] = 0;
        Filling[i] = 0;
      }
      // If numfilling is now zero, shut off main tank
      if (NumFilling < 1) {
        digitalWrite(SUPPLYTANKPIN, VALVECLOSED);
      }
    }
  }
  return;
}

// Helper functions
// ----------------------------------

float Adc2Volts(int AdcVal) {
  // This function should return ADV Volts for input of ADC value/bin.
  // Roughly works for now, needs proper calibration
  return AdcVal * 0.00495;
}


// Functions for commands from the bridge lib example are below:
// --------------------------------------------------------------------
void digitalCommand(BridgeClient Client) {
  int pin, value;

  // Read pin number
  pin = Client.parseInt();

  // If the next character is a '/' it means we have an URL
  // with a value like: "/digital/13/1"
  if (Client.read() == '/') {
    value = Client.parseInt();
    digitalWrite(pin, value);
  } else {
    value = digitalRead(pin);
  }

  // Send feedback to Client
  Client.print(F("Pin D"));
  Client.print(pin);
  Client.print(F(" set to "));
  Client.println(value);

  // Update datastore key with the current pin value
  String key = "D";
  key += pin;
  Bridge.put(key, String(value));
}

void analogCommand(BridgeClient Client) {
  int pin, value;

  // Read pin number
  pin = Client.parseInt();

  // If the next character is a '/' it means we have an URL
  // with a value like: "/analog/5/120"
  if (Client.read() == '/') {
    // Read value and execute command
    value = Client.parseInt();
    analogWrite(pin, value);

    // Send feedback to Client
    Client.print(F("Pin D"));
    Client.print(pin);
    Client.print(F(" set to analog "));
    Client.println(value);

    // Update datastore key with the current pin value
    String key = "D";
    key += pin;
    Bridge.put(key, String(value));
  } else {
    // Read analog pin
    value = analogRead(pin);

    // Send feedback to client
    Client.print(F("Pin A"));
    Client.print(pin);
    Client.print(F(" reads analog "));
    Client.println(value);

    // Update datastore key with the current pin value
    String key = "A";
    key += pin;
    Bridge.put(key, String(value));
  }
}

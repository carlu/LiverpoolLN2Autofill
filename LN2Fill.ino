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
#define LOOPDELAY 100 // ms delay on main loop polling

#define VALVEOPEN 1     // Value written to GPIO to open a valve
#define VALVECLOSED 0   // "      "         "       close a valve

#define FILLHOLDTIME 20   // Time in s for which LED must remain cold for fill success
#define FILLMINTIME 60    // Min total time for a fill to be a success
#define FILLTIMEOUT 360   // Max time for fill to be a success

#define FILLLOGINTERVAL 10  // Period in seconds of logging of LED value during fill
#define FILLLOGLENGTH (FILLTIMEOUT/FILLLOGINTERVAL)+1    // Maximum length of log, should depend on FILLTIMEOUT
                                                        // Should not be greater than 255 or LineFillDataMarker[] needs
                                                        //  to be an int instead of a byte

#define SUPPLYTANKPIN 12  // GPIO# used to open supply valve
#define NUMFILLLINES 4  // Total number of fill lines being managed

// Global variables
BridgeServer Server;  // server for handling requests/commands

// System configuration
const byte LineValvePins[NUMFILLLINES] = {  // GPIO# used for operating each line
 11, 9, 10, 8
};

const byte LineLedPins[NUMFILLLINES] = {    // ADC# used to read LED for each line
  0, 1, 2, 3
};

const float LineLedThresh[NUMFILLLINES] = {  // Threshold volts accross LED to class as "cold"
  1.9,1.9,1.9,1.9
};

bool LineActive[NUMFILLLINES] = {  // Is this line active (used for "fillall" command)
  1, 1, 0, 0
};

// Last fill stats
int LineFillStatus[NUMFILLLINES] = {  // Was the last fill a success (=fill time) or failure (=0)
  0, 0, 0, 0
};

int LineFillData[NUMFILLLINES][FILLLOGLENGTH];  // Stores the LED value log for last fill on each line
byte LineFillDataMarker[NUMFILLLINES] = {        // Stores current log position for last fill on each line
  0, 0, 0, 0
};

// Globals for managing ongoing fill
byte NumFilling = 0;  // Number of lines currently being filled
bool Filling[NUMFILLLINES] = {  // Stores whether each line is currently filling or not
  0, 0, 0, 0
};
time_t FillStartTime[NUMFILLLINES] = {  // Time current fill started for each line
  0, 0, 0, 0
};
time_t ColdStartTime[NUMFILLLINES] = {  // Time LED last indicated cold for each line
  0, 0, 0, 0
};


// Setup and main loop
// ----------------------------------
void setup() {

  // Initialise serial comms
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
  // Clear variables
  memset(LineFillData,0,sizeof(LineFillData));

  // Initialise bridge library and start a
  // server to listen for commands
  Bridge.begin();
  Server.listenOnLocalhost();
  Server.begin();

}

void loop() {

  // Check on any ongoing line fills
  updatefill();

  // Get clients coming from server
  BridgeClient Client = Server.accept();

  // Is there a new client?  If so process its request
  if (Client) {
    // Process request
    process(Client);
    // Close connection and free resources.
    Client.stop();
  }

  // Delay until next loop
  delay(LOOPDELAY); // Poll every LOOPDELAY at most

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
  if (Command == "readstatus") {
    readstatus(Client);
  }
  if (Command == "readled") { // Read specific LED value
    readled(Client);
  }
  if (Command == "readvalve") { // Read valve status
    readvalve(Client);
  }
  if (Command == "readtime") {
    readtime(Client);
  }

  // Initiiate fill cycle commands
  if (Command == "fillline") {
    fillline(Client);
  }
  if (Command == "fillall") {
    fillall(Client);
  }
  if (Command == "fillline_old") {
    fillline_old(Client);
  }
  if (Command == "activateline") {
    activateline(Client);
  }
  if (Command == "deactivateline") {
    deactivateline(Client);
  }

  // reset
  if (Command == "resetall") {
    resetall(Client);
  }

  // Following commands are from the example code I used for building this - CU
  // is "digital" Command?
  if (Command == "digital") {
    digitalCommand(Client);
  }

  // is "analog" Command?
  if (Command == "analog") {
    analogCommand(Client);
  }

  // Debugging function used for problems with passing strings to client
  if (Command == "testprint") {
    testprint(Client);
  }

}


// Functions for directly opening/closing valves
// -----------------------------------------------

void openline(BridgeClient Client) {
  int LineNumber, Pin;

  LineNumber = Client.parseInt() ;

  if (LineNumber > NUMFILLLINES || LineNumber < 1) {
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

  if (LineNumber > NUMFILLLINES || LineNumber < 1) {
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

void readtime(BridgeClient Client) {
  // This function should display the current system time

  time_t Now = now();
  Client.print(" Current system time is ");
  Client.print(Now);
  //Client.print(s);
  Client.print("s (");
  Client.print(hour(Now));
  Client.print(":");
  Client.print(minute(Now));
  Client.print(":");
  Client.print(second(Now));
  Client.print(" ");
  Client.print(weekday(Now));
  Client.print(" ");
  Client.print(day(Now));
  Client.print("/");
  Client.print(month(Now));
  Client.print("/");
  Client.print(year(Now));
  Client.print(")\n");
  return;
}

void readstatus(BridgeClient Client) {

  int i,j;
  int AdcVal;
  bool ValveOpen;
  int Status;

  Client.print(F("# University of Liverpool - Nuclear Physics - LN2 Fill System\n\n# Full Fill-line Status Report:\n"));
  readtime(Client);
  Client.print(F("Main tank valve is "));
  Client.print(digitalRead(SUPPLYTANKPIN) == VALVEOPEN ? "Open\n" : "Closed\n");
  Client.print(F("| LineNum |\tActive? |\tLED Pin |\tLED Thresh |\tADC val |\tLED V |\tValve Pin\t|Valve Status\t|\tLast Fill Status\n\n"));

  for (i = 0; i < NUMFILLLINES; i++) {
    AdcVal = analogRead(LineLedPins[i]);
    ValveOpen = (digitalRead(LineValvePins[i]) == VALVEOPEN);

    Client.print("| ");
    Client.print(i+1);
    Client.print("\t |");
    Client.print(LineActive[i] == 1 ? "\tY" : "\tN");
    Client.print("\t |\t");
    Client.print(LineLedPins[i]);
    Client.print("\t |\t");
    Client.print(LineLedThresh[i]);
    Client.print("\t |\t");
    Client.print(AdcVal);
    Client.print("\t |\t");
    Client.print(Adc2Volts(AdcVal));
    Client.print("\t|\t");
    Client.print(LineValvePins[i]);
    Client.print("\t |");
    Client.print(ValveOpen ? "\tOp" : "\tCl");

    Client.print("\t|\t");

    if (Filling[i] == 0) {
      Client.print(LineFillStatus[i] ? "Succ! (" : "Fail! (");
      Client.print(LineFillStatus[i]);
      Client.print(")");
    } else {
      Client.print("Fill underway!!");
    }
    Client.print("\n");

  }

  // No the last fill data from each line
  Client.print("\n");
  Client.print("\n");
  Client.print(F("Led values for last fill in "));
  Client.print(FILLLOGINTERVAL);
  Client.print("s intervals:\n\n");

  Client.print("Time  : ");
  for (i=0; i<FILLLOGLENGTH; i++) {
    Client.print(i*FILLLOGINTERVAL);
    if (i<10) {
      Client.print("  ");
    }
    else {
      Client.print(" ");
    }
  }
  for (i = 0; i < NUMFILLLINES; i++) {
      Client.print("\nLine ");
      Client.print(i+1);
      Client.print(": ");
      for(j = 0; j<=LineFillDataMarker[i]; j++) {
        Client.print(LineFillData[i][j]);
        Client.print(" ");
      }
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

// Fill a single line, number from URL, doesn't block server
void fillline(BridgeClient Client) {

  int LineNumber;
  // Get linenumber and check valid
  LineNumber = Client.parseInt();
  if (LineNumber < 1 || LineNumber > NUMFILLLINES) {
    Client.print(F("Line number should be between 1 and "));
    Client.print(NUMFILLLINES);
    Client.print(".\n\n");
    return;
  }
  if (Filling[LineNumber-1] == 1) {
    Client.print(F("Fill already underway.  Try reading status."));
    return;
  }

  clearfilldata(LineNumber);

  Client.print("Filling line ");
  Client.print(LineNumber);
  Client.print("\n\n");

  // Open main valve to tank
  Client.print(F("Opening supply tank valve...\n"));
  digitalWrite(SUPPLYTANKPIN, VALVEOPEN);

  // Record that line is filling and open valve
  Filling[LineNumber-1] = 1;
  NumFilling += 1;
  digitalWrite(LineValvePins[LineNumber-1], VALVEOPEN);
  // Record time of fill start and clear cold time
  FillStartTime[LineNumber-1] = now();
  ColdStartTime[LineNumber-1] = 0;
  // First entry in fill data record
  LineFillData[LineNumber-1][LineFillDataMarker[LineNumber-1]] = analogRead(LineLedPins[LineNumber-1]);
  // Print message
  Client.print(F("Opening line "));
  Client.print(LineNumber);
  Client.print("\n");
  readtime(Client);

  return;
}

// Fill a single line - Old method, blocks server, returns data to client rather than stored in LineFillData[]
int fillline_old(BridgeClient Client) {
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

  Client.print(F("Filling all active lines...\n\n"));

  // Open main valve to tank
  Client.print(F("Opening supply tank valve..."));
  digitalWrite(SUPPLYTANKPIN, VALVEOPEN);

  if (NumFilling > 0) {
    Client.print(F("Fill already underway.  Try reading status."));
    return;
  }
  NumFilling = 0;

  for (int i = 0; i < NUMFILLLINES; i++) {
    if (LineActive[i] == 1) {
      clearfilldata(i+1);
      // Record that line is filling and open valve
      Filling[i] = 1;
      NumFilling += 1;
      digitalWrite(LineValvePins[i], VALVEOPEN);
      // Record fill start time and reset cold time
      FillStartTime[i] = now();
      ColdStartTime[i] = 0;
      // First entry in fill data record
      LineFillData[i][LineFillDataMarker[i]] = analogRead(LineLedPins[i]);
      // Print message
      Client.print("Opening line ");
      Client.print(i+1);
      Client.print(" - ");
      readtime(Client);
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
    if (Filling[i] == 1) {
      FillTime = int(now() - FillStartTime[i]);
      LedPin = LineLedPins[i];
      // Check time since last data point and if required record LED value
      if ((FillTime/FILLLOGINTERVAL) > LineFillDataMarker[i] && LineFillDataMarker[i] < FILLLOGLENGTH) {
        LineFillDataMarker[i] += 1;
        LineFillData[i][LineFillDataMarker[i]] = analogRead(LedPin);
      }
      // Check if cold already
      if (ColdStartTime[i] > 0) {
        // Check if still cold
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
            if (FillTime < FILLMINTIME) { // Record Fill fail if too short
              LineFillStatus[i] = 0;
            } else { // Record fill success otherwise
              LineFillStatus[i] = int(now() - FillStartTime[i]);
            }
            // In wither case, reset global variables
            NumFilling -= 1;
            Filling[i] = 0;
            ColdStartTime[i] = 0;
            FillStartTime[i] = 0;
          }
        }
        else {  // If no longer cold, reset to warm status
          ColdStartTime[i] = 0;
        }
      }
      else {  // If not cold already...
        // Is it cold now?
        LedPin = LineLedPins[i];
        LedVolts = Adc2Volts(analogRead(LedPin));
        if (LedVolts > LineLedThresh[i]) {
          // If so set time cold
          ColdStartTime[i] = now();
        }
      }
      // Whether cold or warm, is max time exceeded?
      if (FillTime >= FILLTIMEOUT) {
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

// Functions for altering settings
// ------------------------------------

// Function should reset all global variables to default state (i.e. closed valves, not filling)
void resetall(BridgeClient Client) {
  int i;
  Client.print(F("Shutting all valves and reseting internal variables..."));
  // Close fill tank
  digitalWrite(SUPPLYTANKPIN, VALVECLOSED);

  for (i = 0; i<NUMFILLLINES; i++) {
    //Close all line valves
    digitalWrite(LineValvePins[i], VALVECLOSED);
    // Reset filling status
    Filling[i] = 0;
    LineFillStatus[i] = 0;  // Shows as a failed fill.
    // Deactivate all lines
    LineActive[i] = 0;
    // Reset timers
    FillStartTime[i] = 0;
    ColdStartTime[i] = 0;
    // Reset last fill data
    clearfilldata(i+1);
  }
  NumFilling = 0;
  return;
}

// Code should activate specified line
void activateline(BridgeClient Client) {
  // Code should parse requested line number and activate that line
  int LineNumber = Client.parseInt();

  if (LineNumber < NUMFILLLINES && LineNumber > 0) {
    Client.print("Activating line ");
    Client.print(LineNumber);
    Client.print("\n");
    LineActive[LineNumber - 1] = 1;
  } else {
    Client.print("Unrecognised line number.");
  }
  return;
}

// Code should deactivate specified line
void deactivateline(BridgeClient Client) {
  // Code should parse requested line number and deactivate that line
  int LineNumber = Client.parseInt();

  if (LineNumber < NUMFILLLINES && LineNumber > 0) {
    Client.print("Ativating line ");
    Client.print(LineNumber);
    Client.print("\n");
    LineActive[LineNumber - 1] = 0;
  } else {
    Client.print("Unrecognised line number.");
  }
  return;
}

void clearfilldata(int LineNum) {
  memset(LineFillData[LineNum-1],0,sizeof(int)*FILLLOGLENGTH);
  LineFillDataMarker[LineNum-1] = 0;
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

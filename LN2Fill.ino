#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>

/* ----------------------------------------
Liverpool Nucleasr Physics LN2 Fill System

Arduino bridge code to recieve commands
from python based server code running
elsewhere

List of commands to be implemented:
  * Open/Close valve to tank
  * Open/Close LN2 line
  * Read LED potential (for given line number)
  * Read valve state (for given line number)
  * Initiate fill (line numer)
  * Show staus of each line, including:
    * LED V
    * Valve Status
    * Last fill success/fail and if success then duration
---------------------------------------- */

// Definitions
#define VALVEOPEN 1
#define VALVECLOSED 0

#define FILLHOLDTIME 20
#define FILLTIMEOUT 300

// Global variables

BridgeServer Server;  // server for handling requests/commands

const short SupplyTankPin = 2;
const short NumFillLines = 4;

const char * LineNames[] = {
    "Test1",
    "Test2",
    "Test3",
    "Test4"
};

const short LineValvePins[] = {
  8,9,10,11
};

const short LineLedPins[] = {
  0,1,2,3
};

const float LineLedThresh[] = {
  3.0, 3.0, 3.0, 3.0
};

const int LineActive[] = {
  1, 0, 0, 0
};

int LineStatus[] = {
  0, 0, 0, 0
};

// Setup and main loop
// ----------------------------------
void setup() {

  // Initialise seriel comms
  Serial.begin(9600);

  // Initialise pins
  int Pin, Line;
  pinMode(SupplyTankPin,OUTPUT);   // Main supply valve control
  digitalWrite(SupplyTankPin,VALVECLOSED);
  for(Line = 0; Line < NumFillLines; Line++) {  // Individual line valves
    Pin = LineValvePins[Line];
    pinMode(Pin,OUTPUT);
    digitalWrite(Pin,VALVECLOSED);
  }
  pinMode(13,OUTPUT); // General status LED.

  // Initialise bridge library and start a
  // server to listen for commands
  Bridge.begin();
  Server.listenOnLocalhost();
  Server.begin();

}

void loop() {

  // Get clients coming from server
  BridgeClient Client = Server.accept();

  // Is there a new client?
  if (Client) {
    // Process request
    process(Client);
    // Close connection and free resources.
    Client.stop();
  }

  // Delay until next loop
  delay(50); // Poll every 50ms

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

  // Direct read of line status
  if(Command == "readled") {
    readled(Client);
  }
  if(Command == "readvalve") {
    readvalve(Client);
  }

  // Initiiate fill cycle commands
  if(Command == "fillline") {
    fillline(Client);
  }

  // Line status command
  if (Command == "readstatus") {
    readstatus(Client);
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

}


// Functions for directly opening/closing valves
// -----------------------------------------------

void openline(BridgeClient Client) {
  int LineNumber, Pin;

  LineNumber = Client.parseInt();

  if (LineNumber > NumFillLines) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineValvePins[LineNumber - 1];

  digitalWrite(Pin,VALVEOPEN);

  Client.print(F("Opening line "));
  Client.print(LineNumber);
  Client.print(F(" (Pin"));
  Client.print(Pin);
  Client.print(F(")..."));
}

void closeline(BridgeClient Client) {
  int LineNumber, Pin;

  LineNumber = Client.parseInt();

  if (LineNumber > NumFillLines) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineValvePins[LineNumber - 1];

  digitalWrite(Pin,VALVECLOSED);

  Client.print(F("Closing line "));
  Client.print(LineNumber);
  Client.print(F(" (Pin"));
  Client.print(Pin);
  Client.print(F(")..."));
}

void opentank(BridgeClient Client) {

  digitalWrite(SupplyTankPin,VALVEOPEN);

  Client.print(F("Opening main supply tank (Pin "));
  Client.print(SupplyTankPin);
  Client.print(F(")..."));
}

void closetank(BridgeClient Client) {

  digitalWrite(SupplyTankPin,VALVECLOSED);

  Client.print(F("Closing main supply tank (Pin "));
  Client.print(SupplyTankPin);
  Client.print(F(")..."));
}


// Functions for reading states
// ----------------------------------

void readled(BridgeClient Client) {
  int LineNumber, Pin, Value ;

  LineNumber = Client.parseInt();

  if (LineNumber > NumFillLines) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineLedPins[LineNumber -1];
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

  if (LineNumber > NumFillLines) {
    Client.print(F("Unrecognised Line Number!"));
    return;
  }

  Pin = LineValvePins[LineNumber - 1];
  Value = (digitalRead(Pin)==VALVEOPEN);

  Client.print(F("Valve for line "));
  Client.print(LineNumber);
  Client.print(F(" is "));
  Client.print(Value ? "Open." : "Closed.");
}


void readstatus(BridgeClient Client){
  // This function should return a table showing status of all lines
  // Write headings...
  Client.print(F("# University of Liverpool - Nuclear Physics - LN2 Fill System\n\n"));
  Client.print(F("# Full Fill-line Status Report:\n"));
  Client.print(F("# LineNum\tActive?\tLED Pin\tLED Thresh\tADC val\tLED V\tValve Pin\tValve Status\tLast Fill Status\t\n\n"));

  int i;
  char StatusMessage[256];
  int AdcVal;
  bool ValveOpen;
  int Status;

  for (i=0;i<NumFillLines;i++) {

    AdcVal = analogRead(LineLedPins[i]);
    ValveOpen = (digitalRead(LineValvePins[i])==VALVEOPEN);
    Status = LineStatus[i];

    sprintf(StatusMessage,"%d\t\t%s\t%d\t%04f\t\t%d\t%f\t%d\t\t%s\t\t%s\n",i,LineActive[i]==1?"Yes":"No",LineLedPins[i],LineLedThresh[i],\
        AdcVal,Adc2Volts(AdcVal),LineValvePins[i],ValveOpen?"Open":"Closed",LineStatus[i]?"Success!":"Fail!");

    Client.print(StatusMessage);
  }



}

// Functions for initiating/managing fill cylce
// -----------------------------------------------

// Fill a single line,
int fillline(BridgeClient Client) {
  int LineNumber;
  int FillTime = 0;

  float LedVolts = 0.0;

  LineNumber = Client.parseInt();

  if (LineNumber > NumFillLines) {
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
  digitalWrite(SupplyTankPin,VALVEOPEN);

  // Open valve to line
  digitalWrite(ValvePin,VALVEOPEN);

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
    LineStatus[LineNumber - 1] = 1;
  }
  else {
    Client.print("\nFail!!!");
    LineStatus[LineNumber -1] = 0;
  }

  // Close valve
  digitalWrite(ValvePin,VALVECLOSED);

  // Close tank
  digitalWrite(SupplyTankPin,VALVECLOSED);

  // return fill time
  return FillTime;
}

// Fill all active lines
int fillall(BridgeClient Client) {

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

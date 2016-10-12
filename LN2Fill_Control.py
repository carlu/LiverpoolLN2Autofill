#!/usr/local/bin/python3
#!/usr/bin/python3

# Control Program for Liverpool Nucleasr Physics LN2 Fill System
# ---------------------------------------------------------------

# This script should serve as a control system, logger, and message server
#  for the Arduino-based fill control system running in the nuclear
#  physics lab of the University of Liverpool
# Things it should do:
#   * Run on a Linux PC somewhere and keep in constant contact with the LN2 system.
#   * Monitor the time and initiate a fill sequence whenever it is due.
#   * Log important actions and errors to a text file.
#   * Log all actions and errors to terminal
#   * Email a warning to subscribers whenever:
#       * The script is started (include current config in mail)
#       * A fill is succesful or fails
#       * The script loses contact with the Arduino
#   * Generate plots of:
#       * LED reading vs time for most recent fills.
#       * Total fill time for all fills.
#   * Auto-update files relating to a web status page.

import urllib3
import time as t
import parse
import matplotlib.pyplot as plt

from email.mime.text import MIMEText
from subprocess import Popen, PIPE

# Configuration
# -------------------------------

# Set this to 1 to output lots of extra information to terminal and some extra to logfile
DEBUG = 0

# IP Address of microcontroller
ControllerIP = "localhost:5000"

# Setup urls for regular actions
StatusUrl = 'http://'+ControllerIP +'/arduino/readstatus/0'
FillAllUrl = 'http://'+ControllerIP +'/arduino/fillall/0'

# Frequency/timing of actions
PollFrequency = 30 # Seconds
FillFrequency = 24 * 60 * 60 # Seconds
LastFillTime = 0

# Logging
LogActive = 1
LogFilePath = "/home/user/TempLog.txt"

# Emails
MailNotificationActive = 1
MailAddressList = "email@company.com"
SenderEmail = "sender@company.com"

# Functions
# -------------------------------

# Function to update logfile with timestamp + message
def Log(LogFile,Message):
    if LogActive:
        #print("Writing to log file:\n---")
        if LogFile.closed:
            print("Error - Log file not open!")
            return 1
        else:
            Message = t.ctime(t.time()) + ": " + Message + "\n"
            LogFile.write(Message)
            LogFile.flush()
        print(Message)


# Send email messages to subscribed addresses
def SendMail(Message):
    if MailNotificationActive:
        print("Sending Email to subscribers ({}):\n---".format(MailAddressList))
        MailContent = MIMEText(str(Message))
        MailContent["From"] = "SenderEmail"
        MailContent["To"] = MailAddressList
        MailContent["Subject"] = "Message from LN2 Fill Server"
        p = Popen(["/usr/sbin/sendmail", "-t", "-oi"], stdin=PIPE)
        p.communicate(bytes(MailContent.as_string(),'UTF-8'))
        print(Message)

# Function to parse StatusMessage returned by microcontroller and populate dict with results
#   - need to be careful with string types, default python3 strings are utf-8 unicode
#       but the data from the microcontroller output is in ascii bytes.  String comparison
#       fails if the types are different.
#  - "line" can mean a physical LN2 fill line or a line of text in the status message, ugly
#       but I couldn't think of better names at the time of writing.
def ParseStatus(StatusMessage):
    if DEBUG:
        Log(LogFile,"Parsing status message...")
    # Populate dummy data for now, real function later
    Status = dict()
    Status['MinFillTime'] = 10
    Status['MaxFillTime'] = 30
    Status['FillHoldTime'] = 2
    Status['MainTankStatus'] = "Closed"
    Status['LineStatus'] = []
    Status['NumLines'] = 0
    Status['LineFillStatus'] = []

    # Record of which fields have been freshly populated
    StatusCheck = dict()
    StatusCheck['MinFillTime'] = 0
    StatusCheck['MaxFillTime'] = 0
    StatusCheck['FillHoldTime'] = 0
    StatusCheck['MainTankStatus'] = 0
    StatusCheck['LineStatus'] = 0
    StatusCheck['NumLines'] = 0
    StatusCheck['LineFillStatus'] = 0

    # Loop the actual status message and extract info,
    #    log changes and record StatusCheck of this item
    StatusLines = 0 # Count of lines containing data
    Lines = StatusMessage.splitlines() # Split status into indiv lines
    StatusLineNum = 0 # Count of all lines
    while StatusLineNum < len(Lines):
        Line = Lines[StatusLineNum]
        StatusLineNum = StatusLineNum + 1
        # Skip empty lines
        if len(Line) == 0:
            continue # ..to next line of status message
        # So this should be a line containing some text
        StatusLines += 1
        if DEBUG:  # If debugging, print line before parsing
            Log(LogFile,("Line: "+Line.decode('utf-8')))
        # Check for min fill time
        Flag = b"Minimum fill time:"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Minimum fill time: {} s"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['MinFillTime'] = int(Values[0])
            StatusCheck['MinFillTime'] = 1
            if DEBUG:
                Log(LogFile,"MinFillTime = {}".format(Status['MinFillTime']))
            continue # ..to next line of status message
        # Check for max fill time
        Flag = b"Maximum fill time:"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Maximum fill time: {} s"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['MaxFillTime'] = int(Values[0])
            StatusCheck['MaxFillTime'] = 1
            if DEBUG:
                Log(LogFile,"MaxFillTime = {}".format(Status['MaxFillTime']))
            continue # ..to next line of status message
        # Check for fill hold time
        Flag = b"Fill hold time:"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Fill hold time: {} s"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['FillHoldTime'] = int(Values[0])
            StatusCheck['FillHoldTime'] = 1
            if DEBUG:
                Log(LogFile,'FillHoldTime = {}'.format(Status['FillHoldTime']))
            continue # ..to next line of status message
        # Check for main tank status
        Flag = b"Main tank valve is"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Main tank valve is {}"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['MainTankStatus'] = Values[0]
            StatusCheck['MainTankStatus'] = 1
            if DEBUG:
                Log(LogFile,'MainTankStatus = {}'.format(Status['MainTankStatus']))
            continue # ..to next line of status message
        # Check for fill line data table
        Flag = b"| LineNum |"
        if Line[0:len(Flag)] == Flag:
            # Launch loop to get line status info
            if DEBUG:
                Log(LogFile,"Starting line status loop...")
            Line = Lines[StatusLineNum]
            while len(Line) == 0: # Skip empty lines between column headings and real data
                StatusLineNum = StatusLineNum + 1
                Line = Lines[StatusLineNum]
            while StatusLineNum < len(Lines): # Now expecting one statusline for each fill line, all starting with "|"
                if len(Line) > 0 and Line.decode('utf-8')[0] == "|":
                    Items = Line.split(b'|')
                    if len(Items) == 10:
                        LineData = []
                        LineData.append(int(Items[1].strip())) # Line number
                        LineData.append(Items[2].strip())      # Active?
                        LineData.append(int(Items[3].strip())) # LED pin #
                        LineData.append(float(Items[4].strip())) # LED threshold
                        LineData.append(int(Items[5].strip())) # ADC value
                        LineData.append(float(Items[6].strip())) # LED Volts
                        LineData.append(int(Items[7].strip())) # Valve pin #
                        LineData.append(Items[8].strip())      # Valve Status
                        FillInfo = Items[9].strip().split() # Fill status string
                        LineData.append(FillInfo[0])        # Succ!/Fail! string
                        LineData.append(int(FillInfo[1].strip(b"()"))) # Fill time
                        Status['LineStatus'].append(LineData)
                        Status['NumLines'] += 1
                        StatusCheck['LineStatus'] = 1
                        StatusCheck['NumLines'] = 1
                        if DEBUG:
                            Log(LogFile,'Line {} data = {}'.format(LineData[0],str(Items)))
                    else:
                        Log(LogFile,'Bad line data ({} chars, {} Items)'.format(len(Line),len(Items)))
                    StatusLineNum = StatusLineNum + 1
                    Line = Lines[StatusLineNum]
                else:
                    break
            continue # ..to next line of status message
        # Check for last fill data
        Flag = b"Time  :"
        if Line[0:len(Flag)] == Flag:
            # Grab time scale for fill status info
            Items = Line.split()
            FillTimeScale = list(map(int,Items[2:len(Items)]))
            # Launch loop to get last fill status info
            while StatusLineNum < len(Lines):
                Line = Lines[StatusLineNum]
                StatusLineNum = StatusLineNum + 1
                Items = Line.split()
                FillLineNumber = int(Items[1].strip(b":"))
                LineFillRecord = list(map(int,Items[2:len(Items)]))
                Status['LineFillStatus'].append(LineFillRecord)
                # There should be no missing lines so FillLineNumber == number of entries so far.
                assert(FillLineNumber == len(Status['LineFillStatus']))
                StatusCheck['LineFillStatus'] = 1
                if DEBUG:
                    Log(LogFile,'Line {} fill data = {}'.format(FillLineNumber,LineFillRecord))
            continue # ..to next line of status message
        # If no match found for this line...
        if DEBUG:
            print("No match ({} chars)".format(len(Line)))

    # Check all status items have been processed
    for Key, Value in StatusCheck.items():
        if DEBUG:
            print('{} updated? = {}'.format(Key,Value))
        assert(int(Value) == 1)
    # Return Status dict to main
    return Status

# Function to check if a fill was finished succesfully.
#   - Input is a status dictionary containing data from a parsed status message
#   - Assumes status message is from at least MaxFillTime seconds after a recent fill of all active lines
#   - Should check if fill was succesful and make appropriate notifications
#   - Also add total fill time to long term log
def CheckFillSuccess(Status):
    if DEBUG:
        print("Checking fill success...")
    FailCount = 0
    ActiveCount = 0
    InactiveCount = 0
    FillSuccessMessage = "Current Min/Max/Hold time = {}/{}/{} s\n".format(Status['MinFillTime'],Status['MaxFillTime'],Status['FillHoldTime'])
    for FillLine in Status["LineStatus"]:
        if FillLine[1] == b'Y':
            FillSuccessMessage += "Line {} active. ".format(FillLine[0])
            ActiveCount += 1
            if FillLine[8][0:5] == b'Fail!':
                FillSuccessMessage += "!!!!!!!!! FILL FAILED ({}s) !!!!!!!!!!\n".format(FillLine[9])
                FailCount += 1
            elif FillLine[8][0:5] == b'Succ!':
                FillSuccessMessage += "Fill Success!!! ({}s)\n".format(FillLine[9])
            else:
                if DEBUG:
                    print("What?")
                    print(FillLine[8][0:5])
        else:
            FillSuccessMessage += "Line {} inactive.\n".format(FillLine[0])
            InactiveCount += 1
    if FailCount > 0:
        FillSuccessMessage = "ATTENTION - {} failure(s) out of {} active lines!!\n".format(FailCount,ActiveCount) + FillSuccessMessage
    else:
        FillSuccessMessage = "Looks good!\n" + FillSuccessMessage
    Log(LogFile,FillSuccessMessage)
    SendMail(FillSuccessMessage)

# Function to record long term logs of status items (e.g. LED volts) and alert if contact is lost with microcontroller
#   - Main job is to provide early warning (i.e. before an actual fill is initiated) if contact with the microcontroller is localhost
#   - May also log long term LED volts to check for slow trends
#   - Possibly in future will also be used to sync clocks between server and microcontroller.
def CheckStatus(Status):
    if DEBUG:
        print("Checking status...")

# Function to check response from microcontroller following intitiation of a fill
def CheckFillInitiated(Response):
    if DEBUG:
        print("Checking fill initiated...")

# Setup
# -------------------------------

# Open LogFile
LogFile = open(LogFilePath,'a+')
Log(LogFile,"Starting LN2 Autofill control script....")
# Setup PoolManager to handle http requests
Http = urllib3.PoolManager()

# Main loop
# -------------------------------

while 1:
    if DEBUG:
        print("--------------- DEBUG MODE: New Cycle ------------------------")
    # Check Status
    StatusMessage = Http.request('GET', StatusUrl)
    if DEBUG:
        print("----- DEBUG MODE - Raw status message from Arduino ------- ")
        print(StatusMessage.data)

    Status = ParseStatus(StatusMessage.data)
    CheckStatus(Status)

    # Check time since fill
    TimeSinceFill = t.time() - LastFillTime
    if TimeSinceFill > FillFrequency or LastFillTime == 0:
        LastFillTime = t.time()
        Log(LogFile,"Initiating fill...")
        if DEBUG:
            SendMail("Initiating LN2 Fill...")
        # Send command to fill all lines
        Response = Http.request('GET',FillAllUrl)
        if DEBUG:
            print("----- DEBUG MODE - FillAll acknowledgement message from Arduino ------- ")
            print(Response.data)
        CheckFillInitiated(Response)
        # Wait for fill timeout then check status
        Log(LogFile,"Waiting for fill timeout ({} seconds)...".format(Status['MaxFillTime']))
        t.sleep(Status['MaxFillTime']+1)
        Log(LogFile,"MaxFillTime expired, checking fill status...")
        StatusMessage = Http.request('GET', StatusUrl)
        if DEBUG:
            print("----- DEBUG MODE - Raw status message from Arduino ------- ")
            print(StatusMessage.data)
        Status = ParseStatus(StatusMessage.data)
        CheckFillSuccess(Status)

        #SendMail(StatusMessage.data)
    else:
        if DEBUG:
            Log(LogFile,"No fill this time...")


    t.sleep(PollFrequency)

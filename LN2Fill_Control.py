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

# Plotting...
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

# Basics...
import urllib3
import time as t
import parse
import os

# Email...
import smtplib
from email.mime.image import MIMEImage
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.base import MIMEBase
from email import encoders

# Configuration Function
import Config as Conf

# Call configuration functions
S = Conf.Configure()  # Settings dict, called "S" to avoid long lines later in script

# Functions
# -------------------------------

# Function to update logfile with timestamp + message
def Log(LogFile,Message):
    if S['LogActive']:
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
def SendMail(Message,*args):
    if S['MailNotificationActive']:
        print("Sending Email to subscribers ({}):\n---".format(", ".join(S['MailAddressList'])))

        # Create MIME object and add header info
        msg = MIMEMultipart()
        msg['Subject'] = 'Message from LN2 Fill Server'
        msg['From'] = S['SenderEmail']
        msg['To'] = ", ".join(S['MailAddressList'])

        # Create body as MIMEText object
        BodyText= MIMEText(Message, 'plain')
        print(BodyText)
        # Attach body to message
        msg.attach(BodyText)

        # Assume we know that the image files are all in PDF format
        for arg in args:
            # Open the files in binary mode.  Let the MIMEImage class automatically
            # guess the specific image type.
            FilePath=str(arg)
            # OPen file for attachment
            File = open(FilePath, 'rb')
            # Create empty MIME encoded file
            MIMEFile = MIMEBase('application','pdf')
            # Add contents of file to MIMEFile
            MIMEFile.set_payload(File.read())
            File.close()
            # Encode in base64, bit unsure what's going on here but it was  needed to get this working
            encoders.encode_base64(MIMEFile)
            # Add header to MIMEFile and attach to message
            MIMEFile.add_header('Content-Disposition','attachment;filename={}'.format(FilePath))
            # Attach file to the message
            msg.attach(MIMEFile)

        # Send the email via our own SMTP server.
        Smtp = smtplib.SMTP('localhost')
        Smtp.sendmail(S['SenderEmail'], S['MailAddressList'], msg.as_string())
        Smtp.quit()

# Function to parse StatusMessage returned by microcontroller and populate dict with results
#   - need to be careful with string types, default python3 strings are utf-8 unicode
#       but the data from the microcontroller output is in ascii bytes.  String comparison
#       fails if the types are different.
#  - "line" can mean a physical LN2 fill line or a line of text in the status message, ugly
#       but I couldn't think of better names at the time of writing.
def ParseStatus(StatusMessage):
    if S['DEBUG'] > 0:
        Log(LogFile,"Parsing status message...")
    # Populate dummy data for now, real function later
    Status = dict()
    Status['MinFillTime'] = 10
    Status['MaxFillTime'] = 30
    Status['FillHoldTime'] = 2
    Status['MainTankStatus'] = "Closed"
    Status['LineStatus'] = []
    Status['NumLines'] = 0
    Status['FillTimeScale'] = [];
    Status['LineFillStatus'] = []

    # Record of which fields have been freshly populated
    StatusCheck = dict()
    StatusCheck['MinFillTime'] = 0
    StatusCheck['MaxFillTime'] = 0
    StatusCheck['FillHoldTime'] = 0
    StatusCheck['MainTankStatus'] = 0
    StatusCheck['LineStatus'] = 0
    StatusCheck['NumLines'] = 0
    StatusCheck['FillTimeScale'] = 0;
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
        if S['DEBUG'] > 1:  # If debugging, print line before parsing
            Log(LogFile,("Line: "+Line.decode('utf-8')))
        # Check for min fill time
        Flag = b"Minimum fill time:"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Minimum fill time: {} s"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['MinFillTime'] = int(Values[0])
            StatusCheck['MinFillTime'] = 1
            if S['DEBUG'] > 0:
                Log(LogFile,"MinFillTime = {}".format(Status['MinFillTime']))
            continue # ..to next line of status message
        # Check for max fill time
        Flag = b"Maximum fill time:"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Maximum fill time: {} s"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['MaxFillTime'] = int(Values[0])
            StatusCheck['MaxFillTime'] = 1
            if S['DEBUG'] > 0:
                Log(LogFile,"MaxFillTime = {}".format(Status['MaxFillTime']))
            continue # ..to next line of status message
        # Check for fill hold time
        Flag = b"Fill hold time:"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Fill hold time: {} s"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['FillHoldTime'] = int(Values[0])
            StatusCheck['FillHoldTime'] = 1
            if S['DEBUG'] > 0:
                Log(LogFile,'FillHoldTime = {}'.format(Status['FillHoldTime']))
            continue # ..to next line of status message
        # Check for main tank status
        Flag = b"Main tank valve is"
        if Line[0:len(Flag)] == Flag:
            Pattern = "Main tank valve is {}"
            Values = parse.parse(Pattern,Line.decode('utf-8'))
            Status['MainTankStatus'] = Values[0]
            StatusCheck['MainTankStatus'] = 1
            if S['DEBUG'] > 0:
                Log(LogFile,'MainTankStatus = {}'.format(Status['MainTankStatus']))
            continue # ..to next line of status message
        # Check for fill line data table
        Flag = b"| LineNum |"
        if Line[0:len(Flag)] == Flag:
            # Launch loop to get line status info
            if S['DEBUG'] > 0:
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
                        if S['DEBUG'] > 0:
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
            Status['FillTimeScale'] = FillTimeScale
            StatusCheck['FillTimeScale'] = 1
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
                if S['DEBUG'] > 0:
                    Log(LogFile,'Line {} fill data = {}'.format(FillLineNumber,LineFillRecord))
            continue # ..to next line of status message
        # If no match found for this line...
        if S['DEBUG'] > 1:
            print("No match ({} chars)".format(len(Line)))

    # Check all status items have been processed
    for Key, Value in StatusCheck.items():
        if S['DEBUG'] > 1:
            print('{} updated? = {}'.format(Key,Value))
        assert(int(Value) == 1)
    # Return Status dict to main
    return Status

# Function to check if a fill was finished succesfully.
#   - Input is a status dictionary containing data from a parsed status message
#   - Assumes status message is from at least MaxFillTime seconds after a recent fill of all active lines
#   - Should check if fill was succesful and make appropriate notifications
#   - Also add total fill time to long term log
#   - Requires CheckFillSuccess.LastFill be initialised e.g. CheckFillSuccess.LastFill = [[],[],[],[]]
def CheckFillSuccess(Status):
    if S['DEBUG'] > 0:
        print("Checking fill success...")
    # Variables to count lines and failures
    FailCount = 0
    ActiveCount = 0
    InactiveCount = 0
    # Add min/max/hold times to top of status message
    FillSuccessMessage = "Current Min/Max/Hold time = {}/{}/{} s\n".format(Status['MinFillTime'],Status['MaxFillTime'],Status['FillHoldTime'])
    # Loop LN2lines, check if active, and add success/failure to the message.
    for Index, FillLine in enumerate(Status["LineStatus"]):
        if FillLine[1] == b'Y':
            FillSuccessMessage += "Line {} active. ".format(FillLine[0])
            ActiveCount += 1
            if FillLine[8][0:5] == b'Fail!':  # If fill has failed, t->-t
                # If 0 > t > -1*MinFillTime = Too Short!
                if FillLine[9] >= (-1 * Status['MinFillTime']):
                    FillSuccessMessage += "!!!!!!!!! FILL FAILED ({}s) TOO SHORT !!!!!!!!!!\n".format(FillLine[9])
                # If -1*MinFillTime > t >= -1*MAxFillTime = Timout
                elif FillLine[9] <= (-1 * Status['MaxFillTime']):
                    FillSuccessMessage += "!!!!!!!!! FILL FAILED ({}s) TIMEOUT !!!!!!!!!!\n".format(FillLine[9])
                # Should not reach this condition, t=0
                elif FillLine[9] == 0:
                    FillSuccessMessage += "!!!!! FILL FAILED ({}s) t=0 (erm, I'm not expecting this to happen...) !!!!!!!!!!\n".format(FillLine[9])
                # All other, i.e. t < -1* MaxFillTime (shouldn't happen ever), or t>0 (shouldn't happen at same time as "Fail")
                else:
                    FillSuccessMessage += "!!!!!!!!! FILL FAILED ({}s) UNKNOWN CONDITION !!!!!!!!!!\n".format(FillLine[9])
                FailCount += 1
            elif FillLine[8][0:5] == b'Succ!':
                FillSuccessMessage += "Fill Success!!! ({}s)\n".format(FillLine[9])
            else:
                if S['DEBUG'] > 0:
                    print("What?")
                    print(FillLine[8][0:5])
        else:
            FillSuccessMessage += "Line {} inactive.\n".format(FillLine[0])
            InactiveCount += 1
        CheckFillSuccess.TotalFillTimeRecord[Index].append(int(FillLine[9]))
    # Now generate a plot of LED Volts vs Time for fill
    if S['PLOTS']:
        # Create pdf to store images and get the time scale from the status message
        Pdf = PdfPages(S['LogPath'] + 'LN2Plots.pdf')
        TimeScale = list(map(int,Status['FillTimeScale']))
        # Loop LN2lines and get the LED adc values from each, add to plot
        for Index, FillStatus in enumerate(Status['LineFillStatus']):
            plt.figure(1)
            Ax = plt.subplot(111)
            if Index == 0:
                plt.cla() # Clear axis if this is first line
            # If we have a previous fill ,plot that first
            if len(CheckFillSuccess.LastFill[Index]) > 0:
                PlotFormat = S['PlotColours'][Index % len(S['PlotColours'])] + "--"
                Ax.plot(TimeScale[0:len(CheckFillSuccess.LastFill[Index])],CheckFillSuccess.LastFill[Index],PlotFormat,label="Line {} (previous)".format(Index+1))
            # Then get the latest fill values and plot them
            AdcValues = list(map(int,FillStatus))
            PlotFormat = S['PlotColours'][Index % len(S['PlotColours'])] + "-"
            Ax.plot(TimeScale[0:len(AdcValues)],AdcValues,PlotFormat,label="Line {}".format(Index+1))
            # Finally, store the latest fill as the previous.
            CheckFillSuccess.LastFill[Index] = AdcValues
        # Now make the plot pretty and add to pdf
        plt.legend(loc=2)
        plt.suptitle("LN2 Fill: Adc Voltage Drop (ADC Units) vs Time", fontsize=14, fontweight='bold')
        Ax.grid('on')
        Ax.set_xlabel('Time (s)')
        Ax.set_ylabel('Adc Value')
        Pdf.savefig()

        # Now a plot of the total fill time record
        for Index, FTRecord in enumerate(CheckFillSuccess.TotalFillTimeRecord):
            plt.figure(2)
            Ax = plt.subplot(111)
            if Index == 0:
                plt.cla() # Clear axis if this is first line
            PlotFormat = S['PlotColours'][Index % len(S['PlotColours'])] + "-"
            Ax.plot(range(0,len(FTRecord)),FTRecord,PlotFormat,label="Line {}".format(Index+1))
        # Now make the plot pretty and add to pdf
        #plt.legend(loc=2) # Commented as labels same as previous plot
        plt.suptitle("LN2 Fill: Total Fill Time", fontsize=14, fontweight='bold')
        Ax.grid('on')
        Ax.set_xlabel('Fill Number')
        Ax.set_ylabel('Total Time (s)')
        Pdf.savefig()

        Pdf.close()


    if FailCount > 0:
        FillSuccessMessage = "ATTENTION - {} failure(s) out of {} active lines!!\n".format(FailCount,ActiveCount) + FillSuccessMessage
    else:
        FillSuccessMessage = "Looks good!\n" + FillSuccessMessage
    Log(LogFile,FillSuccessMessage)
    if S['PLOTS']:
        SendMail(FillSuccessMessage,(S['LogPath'] + 'LN2Plots.pdf'))
    else:
        SendMail(FillSuccessMessage)

    # Finally save FillTimeRecord to file
    with open(S['FillRecordSaveFile'], 'w') as File:
        # Loop fill lines and for each one create a string of the times.  new line of text for each ln2 line
        for Line in CheckFillSuccess.TotalFillTimeRecord:
            s = '';
            for Fill in Line:
                s += str(Fill) + ' '
            s += '\n'
            File.write(s)




# Function to record long term logs of status items (e.g. LED volts) and alert if contact is lost with microcontroller
#   - Main job is to provide early warning (i.e. before an actual fill is initiated) if contact with the microcontroller is localhost
#   - May also log long term LED volts to check for slow trends
#   - Possibly in future will also be used to sync clocks between server and microcontroller.
def CheckStatus(Status):
    if S['DEBUG'] > 0:
        print("Checking status...")

# Function to check response from microcontroller following intitiation of a fill
def CheckFillInitiated(Response):
    if S['DEBUG'] > 0:
        print("Checking fill initiated...")

# Setup
# -------------------------------

# Open LogFile
LogFile = open(S['LogFilePath'],'a+')
Log(LogFile,"------ Starting LN2 Autofill control script ------")
Log(LogFile,"--------------------------------------------------")
# Setup PoolManager to handle http requests
Http = urllib3.PoolManager()

# Check for saved fill data
if os.path.isfile(S['FillRecordSaveFile']):
    Log(LogFile,('Loading fill record from: ' + S['FillRecordSaveFile']))
    CheckFillSuccess.TotalFillTimeRecord = []
    # If saved data exists, open file and loop lines
    with open(S['FillRecordSaveFile'], 'r') as File:
        Lines = File.readlines()
        for Line in Lines:
            # Map numbers in the line to a list of ints and append to fill time record
            CheckFillSuccess.TotalFillTimeRecord.append(list(map(int,Line.split())))
    Log(LogFile,('Loaded.'))
# If no saved data then create a new fill record...
else:
    Log(LogFile,'Starting new fill time record.')
    CheckFillSuccess.TotalFillTimeRecord = []
    for Line in range(S['NumberOfFillLines']):
        CheckFillSuccess.TotalFillTimeRecord.append([])


# Initialise last fill record in CheckFillStatus()
CheckFillSuccess.LastFill = []
for Line in range(S['NumberOfFillLines']):
    CheckFillSuccess.LastFill.append([])

RetryCount = 0

# Main loop
# -------------------------------
while 1:
    if S['DEBUG'] > 1:
        print("--------------- DEBUG MODE: New Cycle ------------------------")
    # Check Status
    try :
        StatusMessage = Http.request('GET', S['StatusUrl'])
    except:
        Log(LogFile,"=== Exception Raised Fetching Status Message! ===")
        RetryCount += 1
        if RetryCount > S['RetryStatusMax']:
            StatusMessage = ""
            Log(LogFile,"=== Maximum retries reached! ===")
            SendMail("Cannot communicate with Arduino - Max Retires Reached!")
            break
        t.sleep(S['RetryStatusTimeout'])
        continue


    if S['DEBUG'] > 1:
        print("----- DEBUG MODE - Raw status message from Arduino ------- ")
        print(StatusMessage.data)

    try:
        Status = ParseStatus(StatusMessage.data)
    except:
        Log(LogFile,"=== Cannot parse status ===")
        Log(LogFile,"Bad status as follows: ")
        Log(LogFile,StatusMessage.data)
        SendMail("Error parsing status message: \n\n" + StatusMessage.data)
        break

    CheckStatus(Status)

    # Check time since fill
    TimeSinceFill = t.time() - S['LastFillTime']
    if TimeSinceFill > S['FillFrequency'] or S['LastFillTime'] == 0:

        Log(LogFile,"Initiating fill...")
        if S['DEBUG'] > 1:
            SendMail("Initiating LN2 Fill...")

        # Send command to fill all lines
        try :
            Response = Http.request('GET',S['FillAllUrl'])
        except:
            Log(LogFile,"=== Exception Raised Initiating Fill! ===")
            StatusMessage = ""
            RetryCount += 1
            if RetryCount > S['RetryStatusMax']:
                Log(LogFile,"=== Maximum retries reached! ===")
                SendMail("Cannot communicate with Arduino - Max Retires Reached!")
                break
            t.sleep(S['RetryStatusTimeout'])
            continue

        if S['DEBUG'] > 1:
            print("----- DEBUG MODE - FillAll acknowledgement message from Arduino ------- ")
            print(Response.data)
        CheckFillInitiated(Response)

        # Wait for fill timeout then check status
        Log(LogFile,"Waiting for fill timeout ({} seconds)...".format(Status['MaxFillTime']))
        t.sleep(Status['MaxFillTime']+1)
        Log(LogFile,"MaxFillTime expired, checking fill status...")

        try :
            StatusMessage = Http.request('GET', S['StatusUrl'])
        except:
            Log(LogFile,"=== Exception Raised Fetching Status Message After Fill! ===")
            StatusMessage = ""
            RetryCount += 1
            if RetryCount > S['RetryStatusMax']:
                Log(LogFile,"=== Maximum retries reached! ===")
                SendMail("Cannot communicate with Arduino - Max Retires Reached!")
                break
            t.sleep(S['RetryStatusTimeout'])
            continue

        if S['DEBUG'] > 1:
            print("----- DEBUG MODE - Raw status message from Arduino ------- ")
            print(StatusMessage.data)

        try:
            Status = ParseStatus(StatusMessage.data)
        except:
            Log(LogFile,"=== Cannot parse status ===")
            Log(LogFile,"Bad status as follows: ")
            Log(LogFile,StatusMessage.data)
            SendMail("Error parsing status message after fill: \n\n" + StatusMessage.data)
            break

        CheckFillSuccess(Status)
        S['LastFillTime'] = t.time()

        #SendMail(StatusMessage.data)
    else:
        if S['DEBUG'] > 1:
            Log(LogFile,"No fill this time...")


    t.sleep(S['PollFrequency'])

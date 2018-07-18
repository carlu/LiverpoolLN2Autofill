# Configuration function for LN2 Fill control Server
# Should return dictionary of all settings.


def Configure():
    # Dict to contain settings
    Settings = {}

    # Set this to 1 to output lots of extra information to terminal and some extra to logfile
    Settings['DEBUG'] = 1 # 0 = print important actions only, 1 = print all actions, 2+ = also print raw messages and junk
    Settings['PLOTS'] = 1

    # IP Address of microcontroller
    Settings['ControllerIP'] = "localhost:5000"

    # Setup urls for regular actions
    Settings['StatusUrl'] = 'http://' + Settings['ControllerIP'] + '/arduino/readstatus/0'
    Settings['FillAllUrl'] = 'http://' + Settings['ControllerIP'] + '/arduino/fillall/0'
    Settings['RetryStatusMax'] = 5 # Max retries when contacting arduino, above this warning message sent
    Settings['RetryStatusTimeout'] = 120 # seconds before retry

    # Frequency/timing of actions
    Settings['PollFrequency'] = 300 # Seconds
    Settings['FillFrequency'] = 24 * 60 * 60 # Seconds
    Settings['LastFillTime'] = 0
    Settings['NumberOfFillLines'] = 4

    # Logging
    Settings['LogActive'] = 1
    Settings['LogPath'] = '/Path/To/Log/'
    Settings['LogFile']= 'LN2AutofillLog.txt'
    Settings['LogFilePath'] = Settings['LogPath'] + Settings['LogFile']

    # Emails
    Settings['MailNotificationActive'] = 1
    Settings['MailAddressList'] = ["user@company.com"]
    Settings['SenderEmail'] = "user@company.com"

    # Plots
    Settings['PlotColours'] = ['r','g','b','c','m','k','y']
    Settings['HistoryPlotPeriod'] = 90

    # Fill record  and save location
    Settings['FillRecordSaveFile'] = '/Path/To/Data/LN2AutofillData.txt'

    return(Settings)

from flask import Flask, request
app = Flask(__name__)

StatusMessage = """# University of Liverpool - Nuclear Physics - LN2 Fill System

# Status Report:
 Current system time is 83046s (23:4:6 5 1/1/1970)
Minimum fill time: 10 s
Maximum fill time: 30 s
Fill hold time: 2 s
Main tank valve is Closed
| LineNum |	Active? |	LED Pin |	LED Thresh |	ADC val |	LED V |	Valve Pin	|Valve Status	|	Last Fill Status

| 1	 |	Y	 |	0	 |	1.90	 |	144	 |	0.71	|	11	 |	Cl	|	Succ! (320)
| 2	 |	Y	 |	1	 |	1.90	 |	140	 |	0.69	|	9	 |	Cl	|	Succ! (358)
| 3	 |	N	 |	2	 |	1.90	 |	836	 |	4.14	|	10	 |	Cl	|	Fail! (0)
| 4	 |	N	 |	3	 |	1.90	 |	838	 |	4.15	|	8	 |	Cl	|	Fail! (0)


Led values for last fill in 10s intervals:

Time  : 0   10  20  30  40  50  60  70  80  90  100 110 120 130 140 150 160 170 180 190 200 210 220 230 240 250 260 270 280 290 300 310 320 330 340 350 360 370 380 390 400 410 420 430 440 450 460 470 480 490 500 510 520 530 540 550 560 570 580 590 600
Line 1: 140 124 122 127 124 126 127 129 131 134 136 137 143 139 140 140 141 141 142 467 150 151 150 150 150 353 378 374 380 379 389 528 522
Line 2: 114 108 107 108 110 111 113 115 117 119 121 122 123 124 124 125 126 126 126 127 127 126 127 127 127 128 127 127 128 128 128 129 140 144 384 390
Line 3: 0
Line 4: 0 """

FillMessage="""Filling all active lines...

Opening supply tank valve...Opening line 1 -  Current system time is 534236s (4:23:56 4 7/1/1970)
Opening line 2 -  Current system time is 534237s (4:23:57 4 7/1/1970)"""

@app.route('/')
def hello_world():
    return 'Hello, this is a fake arduino for testing the control script!!!!'

@app.route('/arduino/')
def arduino_root():
    Out = 'You are accessing the arduino!'
    return Out

@app.route('/arduino/readstatus/0')
def readstatus():
    return StatusMessage

@app.route('/arduino/fillall/0')
def fillall():
    return FillMessage

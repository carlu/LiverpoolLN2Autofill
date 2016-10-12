from flask import Flask, request
app = Flask(__name__)

StatusMessage = """# University of Liverpool - Nuclear Physics - LN2 Fill System

# Status Report:
 Current system time is 672601s (18:50:1 5 8/1/1970)
Minimum fill time: 5 s
Maximum fill time: 15 s
Fill hold time: 2 s
Main tank valve is Closed
| LineNum |	Active? |	LED Pin |	LED Thresh |	ADC val |	LED V |	Valve Pin	|Valve Status	|	Last Fill Status

| 1	 |	Y	 |	0	 |	1.90	 |	139	 |	0.69	|	11	 |	Cl	|	Succ! (10)
| 2	 |	Y	 |	1	 |	1.90	 |	138	 |	0.68	|	9	 |	Cl	|	Fail! (0)
| 3	 |	N	 |	2	 |	1.90	 |	842	 |	4.17	|	10	 |	Cl	|	Fail! (0)
| 4	 |	N	 |	3	 |	1.90	 |	844	 |	4.18	|	8	 |	Cl	|	Fail! (0)


Led values for last fill in 10s intervals:

Time  : 0   10  20  30  40  50  60  70  80  90  100 110 120 130 140 150 160 170 180 190 200 210 220 230 240 250 260 270 280 290 300 310 320 330 340 350 360 370 380 390 400 410 420 430 440 450 460 470 480 490 500 510 520 530 540 550 560 570 580 590 600
Line 1: 300 400 500 500 500 500 500 500
Line 2: 0
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

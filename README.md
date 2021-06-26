# VitaBatteryAlarm
Wakes PS Vita from sleep mode and plays a WAV file when the PS Vita has reached a certain battery percentage specified by the user.

# Why did I make this?
I want my battery to last for as many years as possible, so I always try to keep the PS Vita battery charge between 20% and 90% to minimize battery stress.  Sometimes I forget that I left my PS Vita on the charger and it ends up charging overnight.  When I do not forget, I am constantly waking the vita from sleep mode to check the battery percentage.  This app was my solution to those 2 inconveniences.

# What you can customize
1) the alarm sound (WAV file max length 8 seconds.  Anything shorter will play on repeat for 8 seconds or until user closes the alarm dialog.  WAV file must be "alarm.wav" and must be placed in "ux0:app/BALM00001" location.)
2) the alarm volume (can be changed after calibration in ux0:app/BALM00001/hibernatechargerate.txt.  Volume values can be from 0 to 30.)
3) the vita battery percentage at which to trigger the alarm (can be changed after calibration in ux0:app/BALM00001/hibernatechargerate.txt.  Percentage values can be from 0 to 100.)

# Installation Instruction
1) Download latest release of BatteryAlarm.zip
2) Transfer BatteryAlarm.vpk to your PS Vita
3) Open BatteryAlarm.vpk on your PS Vita to install it

# How to Use
Open the app and follow the on-screen prompts.  

FIRST TIME USE  
The first time you open the app, the app will need to run a 3-minute calibration test
in order to know how fast your PS Vita will charge while it is in sleep mode.  The app will not let you calibrate unless your PS Vita 
is at or below 60% charge.  This is to allow for an accurate calibration.

AFTER CALIBRATION  
Open the app, then plug in the charger.  When the alarm goes off, you will see a dialog box that states that charging is complete.  After you press the OK button, the alarm will cease and you may close the Battery Alarm app.  Refer to the "What you can customize" section above to change the settings to your preference.  

----WARNING----  
Waking up the PS Vita before the alarm goes off will delete the alarm and you will have to reopen the app to set the alarm again.

# Remarks
I still use the same battery and charger that came with my PS Vita.  My battery state of health is currently 90%.  You can use the VITAident app by joel16 to check your battery state of health and compare it to mine.  During my tests, the battery percentage of my PS Vita when the alarm went off was always within 1% of the vita battery percentage value set in the ux0:app/BALM00001/hibernatechargerate.txt file.  If your battery state of health is better than mine, your PS Vita might wake up at a percentage just above what you have set in the hibernatechargerate.txt file.  If your battery state of health is worse than mine, your PS Vita might wake up at a percentage just below what you have set in the hibernatechargerate.txt file.  Most likely, the accuracy will still be within 2% of your desired wakeup percentage.  At the very least it should be consistent, so you can compensate by adjusting the alarmChargeValue in the hibernatechargerate.txt file to improve accuracy for your device.  Let me know the results you have with the accuracy of this app and tell me the battery, its state of health, and the charging cable you are using.  This could help me improve accuracy in future releases.

# Credits
CelesteBlue  
-For idea to wake from sleep using timer function  
-For information on vita sleep states  

Bythos  
-For informing me that Wake-Up-Club uses sceTriggerUtil to trigger the alarm  

SparklingPhoenix  
-For pointing out that the vita sdk samples are all installed as apps  

SonicMastr  
-For confirming that apps need a main function instead of module_start and module_stop  

Graphene  
-For giving me information on the prerequisites for using sceTriggerUtil  

joel16  
-For his VITAident app.  I used this a lot for testing my own battery.  

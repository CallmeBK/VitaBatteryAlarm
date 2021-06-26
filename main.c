#include <string.h>
#include <taihen.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <psp2/types.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/display.h>
#include <psp2/power.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/rtc.h>
#include <psp2/triggerutil.h>
#include <psp2/sysmodule.h>
#include <psp2/ctrl.h>
#include <psp2/avconfig.h>

#include "debugScreen.h"

#define printf psvDebugScreenPrintf
#define bufferSize 44

/*define percentages at which charge rate starts to drop*/
#define CCtoCV 71
#define rateDrop1 76
#define rateDrop2 84
#define rateDrop3 85
#define rateDrop4 86
#define rateDrop5 87
#define rateDrop6 90
#define rateDrop7 93
#define rateDrop8 96
#define rateDrop9 98
#define rateDrop10 99

/*define charge rate multipliers for each rate drop percentage*/
#define CCtoCVmultiplier 0.9
#define rateDrop1multiplier 0.85
#define rateDrop2multiplier 0.8
#define rateDrop3multiplier 0.75
#define rateDrop4multiplier 0.7
#define rateDrop5multiplier 0.65
#define rateDrop6multiplier 0.55
#define rateDrop7multiplier 0.5
#define rateDrop8multiplier 0.4
#define rateDrop9multiplier 0.3
#define rateDrop10multiplier 0.2

static float hibernateChargeRate;
static int alarmChargeValue;
static char buffer[bufferSize];
static int initialbatterycapacity = 0;
static int finalbatterycapacity;
static SceRtcTick tick;
static double initialtick;
static double finaltick;
static int scan;
static char tempstring[6];
static float ones;
static float tenths;
static float hundredths;
static float thousandths;
static float tenthousandths;
static SceTriggerUtilEventParamOneTime triggerevent;
static int chargeComplete = 0;
static double secondsUntilAlarm;
static int currentVolume;
static int desiredAlarmVolume = 21;

/*Name of function says it all*/
double convertTicksToSeconds(SceInt64 Tick){
	
	double TickTimeSeconds = Tick / 1000000.0;
	
	return TickTimeSeconds;
}

/*Needed to make my own string to float conversion function*/
/*because atof is not compatible with vita at this time.*/
float stringToFloat(const char *string){
	
	ones = (float)string[0] - '0';
	tenths = ((float)string[2] - '0') / 10;
	hundredths = ((float)string[3] - '0') / 100;
	thousandths = ((float)string[4] - '0') / 1000;
	tenthousandths = ((float)string[5] - '0') / 10000;
	
	return ones + tenths + hundredths + thousandths + tenthousandths;
	
}

/*Needed to make my own float to string conversion function*/
/*because ftoa is not compatible with vita at this time.*/
void floatToString(float input, char *string){
	
	string[0] = (int)input + '0';
	string[1] = '.';
	string[2] = (int)(input * 10) + '0';
	if (((string[2] - '0') * 10) == 0){
		string[3] = (int)(input * 100) + '0';
	}
	else {
		string[3] = (int)(input * 100) % ((string[2] - '0') * 10) +'0';
	}
	if((((string[2] - '0') * 100) + ((string[3] - '0') * 10)) == 0){
		string[4] = (int)(input * 1000) + '0';
	}
	else {
		string[4] = (int)(input * 1000) % (((string[2] - '0') * 100) + ((string[3] - '0') * 10)) + '0';
	}
	if ((((string[2] - '0') * 1000) + ((string[3] - '0') * 100) + ((string[4] - '0') * 10)) == 0) {
		string[5] = (int)(input * 10000) + '0';
	}
	else {
		string[5] = (int)(input * 10000) % (((string[2] - '0') * 1000) + ((string[3] - '0') * 100) + ((string[4] - '0') * 10)) +'0';
	}
	
}

/*Function returns either upperThreshold, lowerThreshold, or initialPercentage depending*/
/*on the value of initialPercentage compared to lowerThreshold and upperThreshold*/
float initialCapacityTest(int upperThreshold, int lowerThreshold, int initialCapacity){
	
	float ret;
	float initialPercentage;
	
	initialPercentage = initialCapacity * 100.0 / scePowerGetBatteryFullCapacity();
	
	if (initialPercentage < lowerThreshold) {
		ret = lowerThreshold;
	}
	else if (initialPercentage >= lowerThreshold && initialPercentage < upperThreshold) {
		ret = initialPercentage;
	}
	else {
		ret = upperThreshold;
	}
	
	return ret;
	
}

/*Check config file for hibernate charge rate.  If it doesn't exist, create it.*/
void loadconfig(void){
	
	/*Attempt to open hibernatechargerate.txt*/
	SceUID fd = sceIoOpen("ux0:app/BALM00001/hibernatechargerate.txt", SCE_O_RDONLY, 0777);
	SceCtrlData ctrl;
	
	/*if ux0:app/BALM00001/hibernatechargerate.txt exists, store text into buffer array*/
	if (fd >= 0) {
		
		sceIoRead(fd, buffer, bufferSize);
		sceIoClose(fd);
		
	}
	/*if ux0:app/BALM00001/hibernatechargerate.txt does not exist, need to run calibration*/
	/*so that ux0:app/BALM00001/hibernatechargerate.txt can be generated custom to your vita*/
	else {
		
		psvDebugScreenInit();
		printf("Calibration not detected.\nCalibration is required before this app can function properly.\n\n");
		
		if (scePowerGetBatteryLifePercent() > 60) {
			
			printf("Battery life must be at or below 60 percent to achieve an accurate calibration.\n\n");
			printf("Reopen this app for calibration once your battery life has become less than or equal to 60 percent.\n\n");
			printf("Press X to exit");
			
			do{
			
				sceCtrlPeekBufferPositive(0, &ctrl, 1);
				
			}while(ctrl.buttons != SCE_CTRL_CROSS);
			
			sceKernelExitProcess(0);
			
		}
		
		printf("Would you like to calibrate now?  It should only take a few minutes.\n\nPress X to continue or press O to quit.\n\n\n\n\n\n");
		
		do{
			
			sceCtrlPeekBufferPositive(0, &ctrl, 1);
			
		}while(ctrl.buttons != SCE_CTRL_CROSS && ctrl.buttons != SCE_CTRL_CIRCLE);
		
		if (ctrl.buttons == SCE_CTRL_CIRCLE){
			sceKernelExitProcess(0);
		}
		else if(ctrl.buttons == SCE_CTRL_CROSS){
			
			printf("Calibration will now begin. Do not unplug the charger until calibration is complete.\n\n"); 
			printf("Plug in your charger to continue.\n");
			
			do{
				
				sceKernelDelayThread(50000);
				
			}while(!scePowerIsBatteryCharging());
			
			/*Get initial battery capacity and time as charger is plugged in*/
			initialbatterycapacity = scePowerGetBatteryRemainCapacity();
			sceRtcGetCurrentTick(&tick);
			initialtick = convertTicksToSeconds(tick.tick);
			
			/*Set 3 minute wake up timer trigger event*/
			triggerevent.ver = SCE_TRIGGER_UTIL_VERSION;
			tick.tick = (initialtick + (60 * 3)) * 1000000;
			triggerevent.triggerTime = tick;
			triggerevent.extraParam1 = 1;
			triggerevent.extraParam2 = 0;
			
			sceTriggerUtilRegisterOneTimeEvent("BALM00001", &triggerevent, 1, 0, 0);
			
			/*Get current volume so app can set volume back to what it was before the alarm was set*/
			sceAVConfigGetSystemVol(&currentVolume);
			/*Set volume to 21 so alarm can be heard*/
			sceAVConfigSetSystemVol(desiredAlarmVolume);

			/*Set vita to sleep state.*/
			scePowerRequestSuspend();
			
			sceKernelDelayThread(1000000);
			
			/*Get battery capacity and time after alarm goes off*/
			finalbatterycapacity = scePowerGetBatteryRemainCapacity();
			sceRtcGetCurrentTick(&tick);
			finaltick = convertTicksToSeconds(tick.tick);
			
			/*Calculate charge rate of vita while it is sleeping*/
			hibernateChargeRate = ((finalbatterycapacity * 100.0 / scePowerGetBatteryFullCapacity()) - (initialbatterycapacity * 100.0 / scePowerGetBatteryFullCapacity())) / (finaltick - initialtick);
			
			/*convert hibernateChargeRate to string so that it can be written to text file*/
			floatToString(hibernateChargeRate, tempstring);
			
			/*build buffer array with charge rate, default alarm percent, and default alarm volume*/
			memset(buffer, 0, bufferSize);
			strcpy(buffer, "chargerate=");
			strncat(buffer, tempstring, 6);
			strcat(buffer, ";alarmpercent=90;volume=21");
			
			fd = sceIoOpen("ux0:app/BALM00001/hibernatechargerate.txt", SCE_O_WRONLY | SCE_O_CREAT, 0777);
			sceIoWrite(fd, buffer, bufferSize);
			sceIoClose(fd);
			
			psvDebugScreenInit();
			printf("Calibration complete!  You may now unplug your charger.\n\n");
			printf("This app will alert you when your vita reaches 90 percent charge.\n\n");
			printf("If you would like your vita to alert you at a different charge percentage...\n");
			printf("change the alarmpercent value in the ux0:app/BALM00001/hibernatechargerate.txt file.\n\n");
			printf("Press X to close.\n");
			
			/*set vita volume back to what the user had it at before the alarm was set*/
			sceAVConfigSetSystemVol(currentVolume);
			
			do{
				
				sceCtrlPeekBufferPositive(0, &ctrl, 1);
				
			}while(ctrl.buttons != SCE_CTRL_CROSS);
			
			sceKernelExitProcess(0);

		}
		
	}
	
	/*Store charge rate, alarm charge percent, and alarm volume from hibernateChargeRate.txt into variables.  %f will be to 4 decimal places*/
	scan = sscanf(buffer, "chargerate=%6s;alarmpercent=%d;volume=%d", tempstring, &alarmChargeValue, &desiredAlarmVolume);
	hibernateChargeRate = stringToFloat(tempstring);
	
}

/*Thread function to check vita battery level every 5 seconds.
  If battery is at the alarm percentage and the vita is being 
  charged, a notification will go off to inform the user that 
  the vita has reached the alarm percentage.*/
int alarm_thread(SceSize arglen, void *arg) {
    (void)arglen;
    (void)arg;
	
	/*loop runs until vita shuts down*/
    for (;;) {

		/*Check vita charging status.*/
		if (scePowerIsBatteryCharging()){ 
			
			/*if charger has been plugged in and current battery life hasn't been stored, store it into initialbatterycapacity variable*/
			if (initialbatterycapacity == 0){
				
				initialbatterycapacity = scePowerGetBatteryRemainCapacity();
				
			}
			
			/*if initialbatterycapacity is greater than the desired alarm charge value, alarm will not be set*/
			/*because the current battery capacity is already above the capacity the user wants to be notifed at*/
			if ((initialbatterycapacity * 100.0 / scePowerGetBatteryFullCapacity()) >= alarmChargeValue){
				
				psvDebugScreenInit();
				printf("Current battery percentage is at or greater than the set alarm charge value.\n");
				printf("The alarm will not be set.");
				
				sceKernelDelayThread(5000000);
				sceKernelExitProcess(0);
				
			}
			else {
				
				if (chargeComplete == 0){
					
					psvDebugScreenInit();
					printf("Alarm set. Do not unplug charger until alarm goes off.");
					
					sceKernelDelayThread(5000000);
					
					sceRtcGetCurrentTick(&tick);
					initialtick = convertTicksToSeconds(tick.tick);
					
					/*if alarmChargeValue <= CC to CV percentage, use constant charge rate to calculate the time to wake up the vita from sleep mode*/
					if (alarmChargeValue <= CCtoCV){
						secondsUntilAlarm = (alarmChargeValue - (initialbatterycapacity * 100.0 / scePowerGetBatteryFullCapacity())) / hibernateChargeRate;
					}
					/*if alarmChargeValue >= CC to CV percentage, charge rate will be exponentially decreasing while the vita battery percentage is between 71 and 100 percent.*/
					/*Total charge time required will depend on how the initial capacity compares to the battery percentages where the charge rate drops*/
					else {
						
						secondsUntilAlarm = (CCtoCV - initialCapacityTest(CCtoCV, 0, initialbatterycapacity)) / hibernateChargeRate;
						
						if (alarmChargeValue <= rateDrop1) {
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue <= rateDrop2) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue == rateDrop3) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue == rateDrop4) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue == rateDrop5) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue <= rateDrop6) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop5 - initialCapacityTest(rateDrop5, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop5, initialbatterycapacity)) / (rateDrop5multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue <= rateDrop7) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop5 - initialCapacityTest(rateDrop5, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop6 - initialCapacityTest(rateDrop6, rateDrop5, initialbatterycapacity)) / (rateDrop5multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop6, initialbatterycapacity)) / (rateDrop6multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue <= rateDrop8) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop5 - initialCapacityTest(rateDrop5, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop6 - initialCapacityTest(rateDrop6, rateDrop5, initialbatterycapacity)) / (rateDrop5multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop7 - initialCapacityTest(rateDrop7, rateDrop6, initialbatterycapacity)) / (rateDrop6multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop7, initialbatterycapacity)) / (rateDrop7multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue <= rateDrop9) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop5 - initialCapacityTest(rateDrop5, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop6 - initialCapacityTest(rateDrop6, rateDrop5, initialbatterycapacity)) / (rateDrop5multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop7 - initialCapacityTest(rateDrop7, rateDrop6, initialbatterycapacity)) / (rateDrop6multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop8 - initialCapacityTest(rateDrop8, rateDrop7, initialbatterycapacity)) / (rateDrop7multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop8, initialbatterycapacity)) / (rateDrop8multiplier * hibernateChargeRate);
						}
						else if (alarmChargeValue == rateDrop10) {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop5 - initialCapacityTest(rateDrop5, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop6 - initialCapacityTest(rateDrop6, rateDrop5, initialbatterycapacity)) / (rateDrop5multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop7 - initialCapacityTest(rateDrop7, rateDrop6, initialbatterycapacity)) / (rateDrop6multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop8 - initialCapacityTest(rateDrop8, rateDrop7, initialbatterycapacity)) / (rateDrop7multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop9 - initialCapacityTest(rateDrop9, rateDrop8, initialbatterycapacity)) / (rateDrop8multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (alarmChargeValue - initialCapacityTest(alarmChargeValue, rateDrop9, initialbatterycapacity)) / (rateDrop9multiplier * hibernateChargeRate);
						}
						else {
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop1 - initialCapacityTest(rateDrop1, CCtoCV, initialbatterycapacity)) / (CCtoCVmultiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop2 - initialCapacityTest(rateDrop2, rateDrop1, initialbatterycapacity)) / (rateDrop1multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop3 - initialCapacityTest(rateDrop3, rateDrop2, initialbatterycapacity)) / (rateDrop2multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop4 - initialCapacityTest(rateDrop4, rateDrop3, initialbatterycapacity)) / (rateDrop3multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop5 - initialCapacityTest(rateDrop5, rateDrop4, initialbatterycapacity)) / (rateDrop4multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop6 - initialCapacityTest(rateDrop6, rateDrop5, initialbatterycapacity)) / (rateDrop5multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop7 - initialCapacityTest(rateDrop7, rateDrop6, initialbatterycapacity)) / (rateDrop6multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop8 - initialCapacityTest(rateDrop8, rateDrop7, initialbatterycapacity)) / (rateDrop7multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop9 - initialCapacityTest(rateDrop9, rateDrop8, initialbatterycapacity)) / (rateDrop8multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (rateDrop10 - initialCapacityTest(rateDrop10, rateDrop9, initialbatterycapacity)) / (rateDrop9multiplier * hibernateChargeRate);
							secondsUntilAlarm = secondsUntilAlarm + (100 - initialCapacityTest(100, rateDrop10, initialbatterycapacity)) / (rateDrop10multiplier * hibernateChargeRate);
						}
					}
					
					/*Create alarm timer based on secondsUntilAlarm calculation*/
					triggerevent.ver = SCE_TRIGGER_UTIL_VERSION;
					tick.tick = (initialtick + secondsUntilAlarm) * 1000000;
					triggerevent.triggerTime = tick;
					triggerevent.extraParam1 = 1;
					triggerevent.extraParam2 = 0;
					sceTriggerUtilRegisterOneTimeEvent("BALM00001", &triggerevent, 1, 0, 0);
					
					chargeComplete = 1;
					
					sceAVConfigGetSystemVol(&currentVolume);
					sceAVConfigSetSystemVol(desiredAlarmVolume);
					
					scePowerRequestSuspend();
					
				}
				/*When charging is complete, change vita volume back to what the user had it set at before the alarm was created*/
				else if (chargeComplete == 1){
					
					sceAVConfigSetSystemVol(currentVolume);
					sceKernelExitProcess(0);
					
				}
				
			}
			
		}
		else {
				
			psvDebugScreenInit();
			printf("Please plug in the charger to continue");
				
			do{
				
				sceKernelDelayThread(50000);
				
			}while(!scePowerIsBatteryCharging());
			
		}
		
		/*Add 1/2-second delay in between iterations for stability*/
		sceKernelDelayThread(500000);
    }
    return 0;
}

/*Code execution starts from this point.*/
int main(int argc, char *argv[]) { 
	
	SceUID thread_id;

	/*Create thread to monitor battery level and charge status.  Highest thread priority is 64.  Lowest thread priority is 191.*/
	thread_id = sceKernelCreateThread("AlarmThread", alarm_thread, 191, 0x10000, 0, 0, NULL);

	sceSysmoduleLoadModule(SCE_SYSMODULE_TRIGGER_UTIL);

	/*Check battery alarm settings*/
	loadconfig();

	/*Start the created thread*/
	sceKernelStartThread(thread_id, 0, NULL);
	
	do{
		sceKernelDelayThread(3000000);
	}while(1);

    return 0;
}
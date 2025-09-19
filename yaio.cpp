//------
#define MIN_WIN_VER 0x0501

#ifndef WINVER
#	define WINVER			MIN_WIN_VER
#endif

#ifndef _WIN32_WINNT
#	define _WIN32_WINNT		MIN_WIN_VER 
#endif

#pragma warning(disable:4996) //_CRT_SECURE_NO_WARNINGS


#include <Windows.h>
#include <stdio.h>
#include <conio.h>
#include <signal.h>
#include <time.h>

#include "irsdk_defines.h"
#include "irsdk_client.h"
#include "serial.h"

// for timeBeginPeriod
#pragma comment(lib, "Winmm")


irsdkCVar Throttle("Throttle");
irsdkCVar Brake("Brake");

irsdkCVar RFbrakeLinePress("RFbrakeLinePress");
irsdkCVar BrakeABSactive("BrakeABSactive");


void ex_program(int sig) 
{
	(void)sig;

	printf("recieved ctrl-c, exiting\n\n");

	timeEndPeriod(1);

	signal(SIGINT, SIG_DFL);
	exit(0);
}


int main(int argc, char *argv[])
{
	signal(SIGINT, ex_program);
	printf("press enter to exit:\n\n");

	// bump priority up so we get time from the sim
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// ask for 1ms timer so sleeps are more precise
	timeBeginPeriod(1);

	while(!_kbhit())
	{
		if(irsdkClient::instance().waitForData(16)){
			printf("Throttle: %f\n", Throttle.getFloat());
			printf("Breake: %f\n", Brake.getFloat());
			printf("BrakeABSactive: %d\n", BrakeABSactive.getBool());
		}
	}
	return 0;
}

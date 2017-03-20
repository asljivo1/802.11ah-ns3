#pragma once

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/extension-headers.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <fstream>
#include <sys/stat.h>
#include <string>

using namespace ns3;
using namespace std;

struct Configuration {
	double simulationTime = 200;
	bool useV6 = false; //false
	uint32_t seed = 1;
	uint32_t Nsta = 1;
	int NRawSta = 96; //-1
	int SlotFormat = -1; //0;
	int NRawSlotCount = -1; //162;
	uint32_t NRawSlotNum = 5;
	uint32_t NGroup = 4;
	uint32_t BeaconInterval = 102400;

	uint32_t MinRTO = 819200;
	uint32_t TCPConnectionTimeout = 6000000;
	uint32_t TCPSegmentSize = 536;
	uint32_t TCPInitialSlowStartThreshold = 0xffff;
	uint32_t TCPInitialCwnd = 1;

	int ContentionPerRAWSlot = 1; //-1
	bool ContentionPerRAWSlotOnlyInFirstGroup = true; //false

	double propagationLossExponent = 3.76;
	double propagationLossReferenceLoss = 8;


	bool APAlwaysSchedulesForNextSlot = false;
	uint32_t APScheduleTransmissionForNextSlotIfLessThan = 5000;

	string DataMode = "MCS2_8";
	//double datarate = 0.65;
	//double bandWidth = 2;

	string visualizerIP = "10.0.2.15"; /// prayan string ""
	int visualizerPort = 7707;
	double visualizerSamplingInterval = 1;

	string rho = "100.0";

	string name = "test"; //payan string

	string APPcapFile = "appcap"; //prayan string
	string NSSFile = "test.nss";

	uint32_t trafficInterval = 1000;
	uint32_t trafficIntervalDeviation = 1000;

	int trafficPacketSize = -1;
	string trafficType = "coap"; //tcpfirmware

	double ipcameraMotionPercentage = 1; //0.1
	uint16_t ipcameraMotionDuration = 10; //60
	uint16_t ipcameraDataRate = 96; //20

	uint32_t firmwareSize = 1024 * 500;
	uint16_t firmwareBlockSize = 1024;
	double firmwareNewUpdateProbability = 0.01;
	double firmwareCorruptionProbability = 0.01;
	uint32_t firmwareVersionCheckInterval = 1000;


	uint16_t sensorMeasurementSize = 1024;

	uint16_t MaxTimeOfPacketsInQueue = 1000; //100

	uint16_t CoolDownPeriod = 60;

	Configuration();
	Configuration(int argc, char** argv);

};

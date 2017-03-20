#pragma once

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
//#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/sixlowpan-module.h"

#include "ns3/internet-module.h"
#include "ns3/extension-headers.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#include "Helper.h"
#include "Configuration.h"
#include "NodeEntry.h"
#include "SimpleTCPClient.h"
#include "Statistics.h"
#include "SimulationEventManager.h"

#include "TCPPingPongClient.h"
#include "TCPPingPongServer.h"
#include "TCPIPCameraClient.h"
#include "TCPIPCameraServer.h"
#include "TCPFirmwareClient.h"
#include "TCPFirmwareServer.h"
#include "TCPSensorClient.h"
#include "TCPSensorServer.h"


using namespace std;
using namespace ns3;

Statistics stats;
Configuration config;

SimulationEventManager eventManager;

Ptr<YansWifiChannel> channel;

NodeContainer staNodes;
NetDeviceContainer staDevices;
NetDeviceContainer sixStaDevices;

NodeContainer apNodes;
NetDeviceContainer apDevices;
NetDeviceContainer sixApDevices;

Ipv4InterfaceContainer staNodeInterfaces;
Ipv4InterfaceContainer apNodeInterfaces;

Ipv6InterfaceContainer staNodeInterfaces6;
Ipv6InterfaceContainer apNodeInterfaces6;

ApplicationContainer serverApp;

vector<NodeEntry*> nodes;

vector<long> transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval;
vector<long> transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval;

uint16_t currentTIMGroup = 0;
uint16_t currentRawSlot = 0;

void configureChannel();

void configureSTANodes(Ssid& ssid);

void configureAPNode(Ssid& ssid);

void configureIPStack();

void configureNodes();

void configureUDPServer();
void configureUDPClients();

void configureUDPEchoClients();
void configureUDPEchoServer();
void configureUdpEchoClientHelper(UdpEchoClientHelper& clientHelper);

void configureTCPEchoClients();
void configureTCPEchoServer();
void configureTCPEchoClientHelper(TcpEchoClientHelper& clientHelper);

void configureTCPPingPongServer();
void configureTCPPingPongClients();

void configureTCPIPCameraServer();
void configureTCPIPCameraClients();

void configureTCPFirmwareServer();
void configureTCPFirmwareClients();

void configureTCPSensorServer();
void configureTCPSensorClients();

void configureCoapServer(); /**/
void configureCoapClients(); /**/
void configureCoapClientHelper(CoapClientHelper& clientHelper);

void wireTCPServer(ApplicationContainer serverApp);
void wireTCPClient(ApplicationContainer clientApp, int i);


void onSTAAssociated(int i);
void onSTADeassociated(int i);

void onChannelTransmission(Ptr<NetDevice> senderDevice, Ptr<Packet> packet);

void updateNodesQueueLength();


int getSTAIdFromAddress(Ipv4Address from);
int getSTAIdFromAddress(Ipv6Address from);

int main(int argc, char** argv);

void printStatistics();

void sendStatistics(bool schedule);


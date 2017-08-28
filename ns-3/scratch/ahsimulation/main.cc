#include "main.h"
using namespace std;
using namespace ns3;

#ifdef __GNUC__
#define UNUSED_PARAM __attribute__ ((unused))
#else /* not a GCC */
#define UNUSED_PARAM
#endif /* GCC */

#define COAP_SIM
/* We use CoAP through the libcoap library which is written in C and uses OS sockets.
 * When doing the simulations with more than ~500 nodes make sure you have set the limit of socket descriptors to a value large enough.
 * If not, when there is no more fd available on the system, simulation crashes.
 * In Linux check: ulimit -n
 * 		at /etc/sysctl.conf add: net.core.somaxconn=131072
 * 								 fs.file-max=131072
 * 		at /usr/include/linux/limits.h change NR_OPEN = 65536
 * */
NS_LOG_COMPONENT_DEFINE ("UdpOnHalow");


int main(int argc, char** argv) {

	PacketMetadata::Enable();
	TypeId tid = TypeId::LookupByName ("ns3::Ipv6RawSocketFactory");
	Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpWestwood"));
	//LogComponentEnable("DcfManager", LOG_LEVEL_DEBUG);
	//LogComponentEnable("S1gApWifiMac", LOG_LEVEL_ALL);
	//LogComponentEnable("StaWifiMac", LOG_LEVEL_ALL);

	//LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_ALL);
	//LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_ALL);


	LogComponentEnable("CoapClient", LOG_LEVEL_ALL);
	LogComponentEnable("CoapServer", LOG_LEVEL_ALL);
#ifdef false
	LogComponentEnable("CoapPdu", LOG_LEVEL_ALL);
	LogComponentEnable("StaWifiMac", LOG_LEVEL_ALL);
	LogComponentEnable("S1gApWifiMac", LOG_LEVEL_ALL);
	LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
	LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_ALL);
	LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
	LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
	LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
#endif

    config = Configuration(argc, argv);

    while (!calculateParameters(config));

    stats = Statistics(config.Nsta);

    transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval = vector<long>(config.NGroup * config.NRawSlotNum, 0);
    transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval = vector<long>(config.NGroup * config.NRawSlotNum, 0);

    eventManager = SimulationEventManager(config.visualizerIP, config.visualizerPort, config.NSSFile);

    RngSeedManager::SetSeed(config.seed);

	Config::SetDefault ("ns3::TcpSocketBase::MinRto",TimeValue(MicroSeconds(config.MinRTO)));
	// don't change the delayed ack timeout, for high values this causes the AP to retransmit
	//Config::SetDefault ("ns3::TcpSocket::DelAckTimeout",TimeValue(MicroSeconds(config.MinRTO)));
	Config::SetDefault ("ns3::TcpSocket::ConnTimeout",TimeValue(MicroSeconds(config.TCPConnectionTimeout)));

	Config::SetDefault ("ns3::TcpSocket::SegmentSize",UintegerValue(config.TCPSegmentSize));
	Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold",UintegerValue(config.TCPInitialSlowStartThreshold));
	Config::SetDefault ("ns3::TcpSocket::InitialCwnd",UintegerValue(config.TCPInitialCwnd));

    configureChannel();

    Ssid ssid = Ssid("ns380211ah");

    // configure STA nodes
    configureSTANodes(ssid);

    // configure AP nodes
    configureAPNode(ssid);

#ifdef COAP_SIM
    externalNodes.Create(1);
    externalNodes.Add(apNodes.Get(0));

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    externalDevices = pointToPoint.Install (externalNodes);

    InternetStackHelper stack;
    stack.Install(externalNodes.Get(0));
#endif
    //in ip stack one command
    // configure IP addresses
    configureIPStack();
#ifdef COAP_SIM
    CoapServerHelper myServer(5683);
    serverApp = myServer.Install(externalNodes.Get(0));
    //serverApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&coapPacketReceivedAtServer));
    serverApp.Start(Seconds(0));
    serverApp.Stop (Seconds (config.simulationTime));
#endif
    // prepopulate routing tables and arp cache
    if (!config.useV6)
    {
    	PopulateArpCache();
    	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }
    else
    {
    	PopulateNdiscCache(true);
    }

    // configure tracing for associations & other metrics
    configureNodes();

    // configure position of AP
    Ptr<MobilityModel> mobility1 = apNodes.Get(0)->GetObject<MobilityModel>();
    Vector apposition = mobility1->GetPosition();
    std::cout << "AP node, position = " << apposition << std::endl;

    // configure position of stations
    uint32_t i (0);
    while (i < config.Nsta) {
        Ptr<MobilityModel> mobility = staNodes.Get(i)->GetObject<MobilityModel>();
        Vector position = mobility->GetPosition();
        double distance = sqrt((position.x - apposition.x)*(position.x - apposition.x) + (position.y - apposition.y)*(position.y - apposition.y));
        std::cout << "Sta node#" << i << ", " << "position = " << position << "(distance to AP: " << distance << ")" << std::endl;
        nodes[i]->x = position.x;
        nodes[i]->y = position.y;
        i++;
    }

    eventManager.onStartHeader();
    eventManager.onStart(config);


    for (uint32_t i = 0; i < config.Nsta; i++)
    	eventManager.onSTANodeCreated(*nodes[i]);

    eventManager.onAPNodeCreated(apposition.x, apposition.y);

    eventManager.onStatisticsHeader();
    // start sending statistics every second
    sendStatistics(true);

    Simulator::Stop(Seconds(config.simulationTime + config.CoolDownPeriod)); // allow up to a minute after the client & server apps are finished to process the queue
    Simulator::Run();
    Simulator::Destroy();

    stats.TotalSimulationTime = Seconds(config.simulationTime);

    printStatistics();

    return (EXIT_SUCCESS);
}

bool calculateParameters (Configuration &config)
{
	bool good (true);
	config.NSSFile = config.trafficType + "_" +
				std::to_string(config.Nsta) + "sta_"+
				std::to_string(config.trafficInterval/1000)+ "sec_" +
				std::to_string(config.NGroup) + "tim_" +
				std::to_string(config.NRawSlotNum)+ "slots_" +
				std::to_string(config.coapPayloadSize) + "payload" + ".nss";
	// calculate parameters
	if(config.trafficPacketSize == -1)
		config.trafficPacketSize = ((int)config.TCPSegmentSize - 100) < 0 ? 100 : (config.TCPSegmentSize - 100);

	if(config.ContentionPerRAWSlot != -1)
	{
		// override the NSta based on the TIM groups and raw slot count
		// to match the contention per slot.
		int totalNrOfSlots = config.NGroup * config.NRawSlotNum;
		int totalSta = totalNrOfSlots * (config.ContentionPerRAWSlot+1);

		// only fill the first TIM group RAW if true, to reduce the number of stations overall
		// to speed up the simulation. All the other TIM groups are behaving the same.
		// Note that this is different from specifying only 1 TIM group because the DTIM cycle is still
		// longer with more groups!
		if(config.ContentionPerRAWSlotOnlyInFirstGroup)
			config.Nsta = totalSta / config.NGroup;
		else
			config.Nsta = totalSta;

		config.NRawSta = totalSta;
	}

	//config.nControlLoops = config.Nsta / 2;
	/*if (config.Nsta < config.nControlLoops * 2)
	{
		config.nControlLoops = config.Nsta / 2;
		std::cout << "Number of stations:" << config.Nsta << " New number of control loops: " << config.nControlLoops << std::endl;
	}*/
	if (config.BeaconInterval % 1024 != 0)
	{
		//because its stored in the broadcast that way. If not it leads to rounding issues at the STA MAC causing them to be out of sync
		config.BeaconInterval = ceil((config.BeaconInterval)/1024.)*1024;
		std::cout << "New larger beacon interval divisible by 1024:" << config.BeaconInterval << std::endl;
	}
	/*if ( config.BeaconInterval < 1024*3 )
	{
		std::cout << "Bad configuration: Minimal beacon interval is 3072 for 1 slot and beacon interval must be divisible bz 1024." << std::endl;
		do {
			config.BeaconInterval += 1024;
		}
		while (config.BeaconInterval < 3072);
		std::cout << "New larger beacon interval:" << config.BeaconInterval << std::endl;
	}*/

	// CONFIGURATION PARAMETERS
	autoSetNRawSlotCount(config);


	if(config.NRawSta == -1)
		config.NRawSta = config.Nsta;

	while (config.NRawSlotNum * (500 + config.NRawSlotCount * 120) > config.BeaconInterval)
		config.BeaconInterval += 1024 * 2;

	if (config.nControlLoops*2 > config.Nsta)
	{
		std::cout << "Bad configuration: number of stations must be at least 2 * number of control loops. Abort elegantly." << std::endl;
		return (EXIT_SUCCESS);
	}
	// NRawSta should be divisible by NGroup
	if (config.NRawSta % config.NGroup != 0)
	{
		config.NRawSta = round(static_cast<double>(config.NRawSta/config.NGroup)) * config.NGroup;
		std::cout << "Bad configuration: NRawSta must be divisible by NGroup. New NRawSta is "<< config.NRawSta << std::endl;
	}
	return good;
}

void autoSetNRawSlotCount (Configuration& config)
{
	if(config.NRawSlotCount == -1)
	{
		//config.NRawSlotCount = ceil(162 * 5 / config.NRawSlotNum);
		config.NRawSlotCount = (config.BeaconInterval - 500 * config.NRawSlotNum)/(120 * config.NRawSlotNum);
	}
	// check parameters = only for 2MHz
	if (config.DataMode == "MCS2_0")
	{
		if (config.NRawSlotCount < 15)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_0 is " << config.NRawSlotCount << ". Set to 15." << std::endl;
			config.NRawSlotCount = 15;
		}
	}
	else if (config.DataMode == "MCS2_1"){
		if (config.NRawSlotCount < 10)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_1 is " << config.NRawSlotCount << ". Set to 10." << std::endl;
			config.NRawSlotCount = 10;
		}
	}
	else if (config.DataMode == "MCS2_2"){
		if (config.NRawSlotCount < 8)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_2 is " << config.NRawSlotCount << ". Set to 8." << std::endl;
			config.NRawSlotCount = 8;
		}
	}
	else if (config.DataMode == "MCS2_3"){
		if (config.NRawSlotCount < 7)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_3 is " << config.NRawSlotCount << ". Set to 7." << std::endl;
			config.NRawSlotCount = 7;
		}
	}
	else if (config.DataMode == "MCS2_4"){
		if (config.NRawSlotCount < 6)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_4 is " << config.NRawSlotCount << ". Set to 6." << std::endl;
			config.NRawSlotCount = 6;
		}
	}
	else if (config.DataMode == "MCS2_5"){
		if (config.NRawSlotCount < 6)
		{
			std::cerr << "Bad configuration: Minimal NRawSlotCount for MCS2_5 is " << config.NRawSlotCount << ". Set to 6." << std::endl;
			config.NRawSlotCount = 6;
		}
	}
	else if (config.DataMode == "MCS2_6"){
		if (config.NRawSlotCount < 6)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_6 is " << config.NRawSlotCount << ". Set to 6." << std::endl;
			config.NRawSlotCount = 6;
		}
	}
	else if (config.DataMode == "MCS2_7"){
		if (config.NRawSlotCount < 6)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_7 is " << config.NRawSlotCount << ". Set to 6." << std::endl;
			config.NRawSlotCount = 6;
		}
	}
	else if (config.DataMode == "MCS2_8"){
		if (config.NRawSlotCount < 6)
		{
			std::cout << "Bad configuration: Minimal NRawSlotCount for MCS2_8 is " << config.NRawSlotCount << ". Set to 6." << std::endl;
			config.NRawSlotCount = 6;
		}
	}
	// For long RAW slots (slot format 1) max number of slots is < 8     =>   max RAW period <= 1 722 980 us
	// For short RAW slots (slot format 0) max number of slots is < 64   =>   max RAW period <= 1 951 740 us
	// Beacon interval should be the smallest multiple of 1024 larger or equal than RAW period

	// Max NRawSlotCount is 2047 and min depends on the data rate.
	if (config.NRawSlotCount >= 256 && config.NRawSlotCount < 2048)
	{
		config.SlotFormat = 1;
		if (config.NRawSlotNum >= 8)
		{
			std::cout << "Bad configuration: if SlotFormat == 1 then NRawSlotNum must be < 8. Value NRawSlotNum = " << config.NRawSlotNum << " is invalid. New NRawSlotNum is 8." << std::endl;
			config.NRawSlotNum = 7;
			config.NRawSlotCount = -1;
			autoSetNRawSlotCount (config);
		}
	}
	else if (config.NRawSlotCount > -1 && config.NRawSlotCount < 256)
	{
		config.SlotFormat = 0;
		if (config.NRawSlotNum >= 64)
		{
			std::cout << "Bad configuration: if SlotFormat == 0 then NRawSlotNum must be < 64. Value NRawSlotNum = " << config.NRawSlotNum << " is invalid. New NRawSlotNum is 63." << std::endl;
			config.NRawSlotNum = 63;
			config.NRawSlotCount = -1;
			autoSetNRawSlotCount (config);
		}
	}
	else if (config.NRawSlotCount >= 2048)
	{
		config.SlotFormat = 1;
		config.NRawSlotCount = 2047;
		while ((config.NRawSlotNum = config.BeaconInterval / (500 + 120 * config.NRawSlotCount)) == 0){
			config.NRawSlotCount -= 10;
			config.BeaconInterval += 1024;
		}
		autoSetNRawSlotCount (config);
	}
}

void configureChannel() {
	// setup wifi channel
	YansWifiChannelHelper channelBuilder = YansWifiChannelHelper();
	channelBuilder.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(config.propagationLossExponent), "ReferenceLoss", DoubleValue(config.propagationLossReferenceLoss), "ReferenceDistance", DoubleValue(1.0));
	channelBuilder.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	channel = channelBuilder.Create();
    channel->TraceConnectWithoutContext("Transmission", MakeCallback(&onChannelTransmission));
}

void onChannelTransmission(Ptr<NetDevice> senderDevice, Ptr<Packet> packet) {
	int timGroup = currentTIMGroup;
	int slotIndex = currentRawSlot;

	if(senderDevice->GetAddress() == apDevices.Get(0)->GetAddress()) {
		// from AP
		transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval[timGroup * config.NRawSlotNum + slotIndex]+= packet->GetSerializedSize();
	}
	else {
		// from STA
		transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval[timGroup * config.NRawSlotNum + slotIndex]+= packet->GetSerializedSize();

	}
}

int getBandwidth(string dataMode) {
	if(dataMode == "MCS1_0" ||
	   dataMode == "MCS1_1" || dataMode == "MCS1_2" ||
		dataMode == "MCS1_3" || dataMode == "MCS1_4" ||
		dataMode == "MCS1_5" || dataMode == "MCS1_6" ||
		dataMode == "MCS1_7" || dataMode == "MCS1_8" ||
		dataMode == "MCS1_9" || dataMode == "MCS1_10")
			return 1;

	else if(dataMode == "MCS2_0" ||
	    dataMode == "MCS2_1" || dataMode == "MCS2_2" ||
		dataMode == "MCS2_3" || dataMode == "MCS2_4" ||
		dataMode == "MCS2_5" || dataMode == "MCS2_6" ||
		dataMode == "MCS2_7" || dataMode == "MCS2_8")
			return 2;

	return 0;
}

string getWifiMode(string dataMode) {
	if(dataMode == "MCS1_0") return "OfdmRate300KbpsBW1MHz";
	else if(dataMode == "MCS1_1") return "OfdmRate600KbpsBW1MHz";
	else if(dataMode == "MCS1_2") return "OfdmRate900KbpsBW1MHz";
	else if(dataMode == "MCS1_3") return "OfdmRate1_2MbpsBW1MHz";
	else if(dataMode == "MCS1_4") return "OfdmRate1_8MbpsBW1MHz";
	else if(dataMode == "MCS1_5") return "OfdmRate2_4MbpsBW1MHz";
	else if(dataMode == "MCS1_6") return "OfdmRate2_7MbpsBW1MHz";
	else if(dataMode == "MCS1_7") return "OfdmRate3MbpsBW1MHz";
	else if(dataMode == "MCS1_8") return "OfdmRate3_6MbpsBW1MHz";
	else if(dataMode == "MCS1_9") return "OfdmRate4MbpsBW1MHz";
	else if(dataMode == "MCS1_10") return "OfdmRate150KbpsBW1MHz";


	else if(dataMode == "MCS2_0") return "OfdmRate650KbpsBW2MHz";
	else if(dataMode == "MCS2_1") return "OfdmRate1_3MbpsBW2MHz";
	else if(dataMode == "MCS2_2") return "OfdmRate1_95MbpsBW2MHz";
	else if(dataMode == "MCS2_3") return "OfdmRate2_6MbpsBW2MHz";
	else if(dataMode == "MCS2_4") return "OfdmRate3_9MbpsBW2MHz";
	else if(dataMode == "MCS2_5") return "OfdmRate5_2MbpsBW2MHz";
	else if(dataMode == "MCS2_6") return "OfdmRate5_85MbpsBW2MHz";
	else if(dataMode == "MCS2_7") return "OfdmRate6_5MbpsBW2MHz";
	else if(dataMode == "MCS2_8") return "OfdmRate7_8MbpsBW2MHz";
	return "";
}

void configureSTANodes(Ssid& ssid) {
	cout << "Configuring STA Nodes " << endl;
    // create STA nodes
    staNodes.Create(config.Nsta);

    // setup physical layer
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetErrorRateModel("ns3::YansErrorRateModel");
    phy.SetChannel(channel);
    phy.Set("ShortGuardEnabled", BooleanValue(false));
    phy.Set("ChannelWidth", UintegerValue(getBandwidth(config.DataMode)));
    phy.Set("EnergyDetectionThreshold", DoubleValue(-116.0));
    phy.Set("CcaMode1Threshold", DoubleValue(-119.0));
    phy.Set("TxGain", DoubleValue(0.0));
    phy.Set("RxGain", DoubleValue(0.0));
    phy.Set("TxPowerLevels", UintegerValue(1));
    phy.Set("TxPowerEnd", DoubleValue(0.0));
    phy.Set("TxPowerStart", DoubleValue(0.0));
    phy.Set("RxNoiseFigure", DoubleValue(3.0));
    phy.Set("LdpcEnabled", BooleanValue(true));

    // setup S1g MAC
    S1gWifiMacHelper mac = S1gWifiMacHelper::Default();
    mac.SetType("ns3::StaWifiMac",
            "Ssid", SsidValue(ssid),
            "ActiveProbing", BooleanValue(false),
			"MaxMissedBeacons", UintegerValue (10 *config.NGroup),
			"MaxTimeInQueue", TimeValue(Seconds(config.MaxTimeOfPacketsInQueue)),
			"RawDuration", TimeValue (MicroSeconds (config.BeaconInterval)));///ami TODO test

    // create wifi
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ah);
    StringValue dataRate = StringValue(getWifiMode(config.DataMode));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", dataRate, "ControlMode", dataRate);

    cout << "Installing STA Node devices" << endl;
    // install wifi device
    staDevices = wifi.Install(phy, mac, staNodes);
    //staDevices4sixlp = wifi.Install(phy, mac, staNodes);

    cout << "Configuring STA Node mobility" << endl;
    // mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
            "X", StringValue("1000.0"),
            "Y", StringValue("1000.0"),
            "rho", StringValue(config.rho));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    phy.EnablePcap("stafile", staNodes, 0);
}

void OnAPPhyRxDrop(std::string context, Ptr<const Packet> packet, DropReason reason) {
	// THIS REQUIRES PACKET METADATA ENABLE!
	unused (context);
	auto pCopy = packet->Copy();
	auto it = pCopy->BeginItem();
	while(it.HasNext()) {

		auto item = it.Next();
		Callback<ObjectBase *> constructor = item.tid.GetConstructor ();

		ObjectBase *instance = constructor ();
		Chunk *chunk = dynamic_cast<Chunk *> (instance);
		chunk->Deserialize (item.current);

		if(dynamic_cast<WifiMacHeader*>(chunk)) {
			WifiMacHeader* hdr = (WifiMacHeader*)chunk;

			int staId = -1;
			if (!config.useV6){
				for (uint32_t i = 0; i < staNodeInterfaces.GetN(); i++) {
					if(staNodes.Get(i)->GetDevice(0)->GetAddress() == hdr->GetAddr2()) {
						staId = i;
						break;
					}
				}
			}
			else
			{
				for (uint32_t i = 0; i < staNodeInterfaces6.GetN(); i++) {
					if(staNodes.Get(i)->GetDevice(0)->GetAddress() == hdr->GetAddr2()) {
						staId = i;
						break;
					}
				}
			}
			if(staId != -1) {
				stats.get(staId).NumberOfDropsByReasonAtAP[reason]++;
			}
			delete chunk;
			break;
		}
		else
			delete chunk;
	}


}

void OnAPRAWSlotStarted(string context, uint16_t timGroup, uint16_t rawSlot) {
	unused(context);
	currentTIMGroup = timGroup;
	currentRawSlot = rawSlot;
}

void OnAPPacketToTransmitReceived(string context, Ptr<const Packet> packet, Mac48Address to, bool isScheduled, bool isDuringSlotOfSTA, Time timeLeftInSlot) {
	unused(context);
	unused(packet);
	unused(isDuringSlotOfSTA);
	int staId = -1;
	if (!config.useV6){
		for (uint32_t i = 0; i < staNodeInterfaces.GetN(); i++) {
			if(staNodes.Get(i)->GetDevice(0)->GetAddress() == to) {
				staId = i;
				break;
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < staNodeInterfaces6.GetN(); i++) {
			if(staNodes.Get(i)->GetDevice(0)->GetAddress() == to) {
				staId = i;
				break;
			}
		}
	}
	if(staId != -1) {
		if(isScheduled)
			stats.get(staId).NumberOfAPScheduledPacketForNodeInNextSlot++;
		else {
			stats.get(staId).NumberOfAPSentPacketForNodeImmediately++;
			stats.get(staId).APTotalTimeRemainingWhenSendingPacketInSameSlot += timeLeftInSlot;
		}
	}
}

void configureAPNode (Ssid& ssid) {
	cout << "Configuring AP Node " << endl;
    // create AP node
    apNodes.Create(1);
    uint32_t NGroupStas = config.NRawSta / config.NGroup;

    // setup mac
    S1gWifiMacHelper mac = S1gWifiMacHelper::Default();
    mac.SetType("ns3::S1gApWifiMac",
            "Ssid", SsidValue(ssid),
            "BeaconInterval", TimeValue(MicroSeconds(config.BeaconInterval)),
            "NRawGroupStas", UintegerValue(NGroupStas),
            "NRawStations", UintegerValue(config.NRawSta),
            "SlotFormat", UintegerValue(config.SlotFormat),
            "SlotDurationCount", UintegerValue(config.NRawSlotCount),
            "SlotNum", UintegerValue(config.NRawSlotNum),
			"ScheduleTransmissionForNextSlotIfLessThan", TimeValue(MicroSeconds(config.APScheduleTransmissionForNextSlotIfLessThan)),
			"AlwaysScheduleForNextSlot", BooleanValue(config.APAlwaysSchedulesForNextSlot),
			"MaxTimeInQueue", TimeValue(Seconds(config.MaxTimeOfPacketsInQueue)));

    // setup physical layer
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetErrorRateModel("ns3::YansErrorRateModel");
    phy.SetChannel(channel);
    phy.Set("ShortGuardEnabled", BooleanValue(false));
    phy.Set("ChannelWidth", UintegerValue(getBandwidth(config.DataMode)));
    phy.Set("EnergyDetectionThreshold", DoubleValue(-116.0));
    phy.Set("CcaMode1Threshold", DoubleValue(-119.0));
    phy.Set("TxGain", DoubleValue(3.0));
    phy.Set("RxGain", DoubleValue(3.0));
    phy.Set("TxPowerLevels", UintegerValue(1));
    phy.Set("TxPowerEnd", DoubleValue(30.0));
    phy.Set("TxPowerStart", DoubleValue(30.0));
    phy.Set("RxNoiseFigure", DoubleValue(5));
    phy.Set("LdpcEnabled", BooleanValue(true));
    phy.Set("Transmitters", UintegerValue(1));
    phy.Set("Receivers", UintegerValue(1));


    // create wifi
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ah);
    StringValue dataRate = StringValue(getWifiMode(config.DataMode));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", dataRate, "ControlMode", dataRate);
    //wifi.EnableLogComponents();

    apDevices = wifi.Install(phy, mac, apNodes);

    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add(Vector(1000.0, 1000.0, 0.0));
    mobilityAp.SetPositionAllocator(positionAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNodes);

	Config::Connect("/NodeList/" + std::to_string(config.Nsta) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxDropWithReason", MakeCallback(&OnAPPhyRxDrop));
	Config::Connect("/NodeList/" + std::to_string(config.Nsta) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::S1gApWifiMac/PacketToTransmitReceivedFromUpperLayer", MakeCallback(&OnAPPacketToTransmitReceived));
	Config::Connect("/NodeList/" + std::to_string(config.Nsta) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::S1gApWifiMac/RAWSlotStarted", MakeCallback(&OnAPRAWSlotStarted));

	if(config.APPcapFile != "") {
		//phy.EnablePcap(config.APPcapFile, apNodes, 0);
	}
}

void configureIPStack() {
	cout << "Configuring IP stack " << endl;
	InternetStackHelper stack;
	stack.Install(apNodes);
	stack.Install(staNodes);

    if (!config.useV6)
    {
    	//stack.SetIpv6StackInstall(false);
    	Ipv4AddressHelper address;
    	address.SetBase("192.168.0.0", "255.255.0.0");
    	staNodeInterfaces = address.Assign(staDevices);
    	apNodeInterfaces = address.Assign(apDevices);
#ifdef COAP_SIM
    	address.SetBase("169.200.0.0", "255.255.0.0");
        externalInterfaces = address.Assign (externalDevices);
#endif
    }
    else
    {
    	SixLowPanHelper sixlowpan;
    	sixlowpan.SetDeviceAttribute("ForceEtherType", ns3::BooleanValue(true));
		//sixlowpan.SetDeviceAttribute("EtherType", ns3::UintegerValue(2269)); // 2269 = 0x8dd for IPv6; or 2048 = 0x0800 for IPv4

		sixStaDevices = sixlowpan.Install(staDevices);
		sixApDevices = sixlowpan.Install(apDevices);

        Ipv6AddressHelper address6;
        address6.SetBase("2001:0000:f00d:cafe::", Ipv6Prefix (64));

        staNodeInterfaces6 = address6.Assign(sixStaDevices);
        apNodeInterfaces6 = address6.Assign(sixApDevices);
        //TODO routing doesnt work with ipv6 here

    	/*std::cout << "    AP     --IPv6: " << apNodeInterfaces6.GetAddress(0, 0) << "  --MAC: " 	<< Mac48Address::ConvertFrom(sixApDevices.Get(0)->GetAddress())
    	<< std::endl;
    	for (uint32_t i(0); i < staNodeInterfaces6.GetN(); i++) {
    		Mac48Address addr48 = Mac48Address::ConvertFrom(sixStaDevices.Get(i)->GetAddress());
    		std::cout << "  Node #" << i << "  --IPv6: "<< staNodeInterfaces6.GetAddress(i,0) << "--MAC: " << addr48 	<< std::endl;
    	}*/
#ifdef COAP_SIM
    	address6.SetBase("2001:2::", Ipv6Prefix (64));
        externalInterfaces6 = address6.Assign (externalDevices);
#endif
        for (uint32_t i=0; i<staNodes.GetN(); i++)
        	staNodes.Get(i)->GetObject<Icmpv6L4Protocol> ()->SetAttribute ("DAD", BooleanValue (false));
        apNodes.Get(0)->GetObject<Icmpv6L4Protocol> ()->SetAttribute ("DAD", BooleanValue (false));

    }
    cout << "IP stack configured " << endl;
}

void configureNodes() {
	cout << "Configuring STA Node trace sources" << endl;

    for (uint32_t i = 0; i < config.Nsta; i++) {

    	cout << "Hooking up trace sources for STA " << i << endl;

        NodeEntry* n = new NodeEntry(i, &stats, staNodes.Get(i), staDevices.Get(i));

        n->SetAssociatedCallback([ = ]{onSTAAssociated(i);});
        n->SetDeassociatedCallback([ = ]{onSTADeassociated(i);});

        nodes.push_back(n);
        // hook up Associated and Deassociated events
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc", MakeCallback(&NodeEntry::SetAssociation, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc", MakeCallback(&NodeEntry::UnsetAssociation, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/NrOfTransmissionsDuringRAWSlot", MakeCallback(&NodeEntry::OnNrOfTransmissionsDuringRAWSlotChanged, n));

        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/S1gBeaconMissed", MakeCallback(&NodeEntry::OnS1gBeaconMissed, n));

        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/PacketDropped", MakeCallback(&NodeEntry::OnMacPacketDropped, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Collision", MakeCallback(&NodeEntry::OnCollision, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/TransmissionWillCrossRAWBoundary", MakeCallback(&NodeEntry::OnTransmissionWillCrossRAWBoundary, n));


        // hook up TX
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&NodeEntry::OnPhyTxBegin, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxEnd", MakeCallback(&NodeEntry::OnPhyTxEnd, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxDropWithReason", MakeCallback(&NodeEntry::OnPhyTxDrop, n));

        // hook up RX
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback(&NodeEntry::OnPhyRxBegin, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback(&NodeEntry::OnPhyRxEnd, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxDropWithReason", MakeCallback(&NodeEntry::OnPhyRxDrop, n));


        // hook up MAC traces
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxRtsFailed", MakeCallback(&NodeEntry::OnMacTxRtsFailed, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxDataFailed", MakeCallback(&NodeEntry::OnMacTxDataFailed, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxFinalRtsFailed", MakeCallback(&NodeEntry::OnMacTxFinalRtsFailed, n));
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxFinalDataFailed", MakeCallback(&NodeEntry::OnMacTxFinalDataFailed, n));

        // hook up PHY State change
        Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Phy/State/State", MakeCallback(&NodeEntry::OnPhyStateChange, n));

    }
}

int getSTAIdFromAddress(Ipv4Address from) {
    int staId = -1;
    for (uint32_t i = 0; i < staNodeInterfaces.GetN(); i++) {
        if (staNodeInterfaces.GetAddress(i) == from) {
            staId = i;
            break;
        }
    }
    return staId;
}

int getSTAIdFromAddress(Ipv6Address from) {
    int staId = -1;
    for (uint32_t i = 0; i < staNodeInterfaces6.GetN(); i++) {
        if (staNodeInterfaces6.GetAddress(i,0) == from) {
            staId = i;
            break;
        }
    }
    return staId;
}

void udpPacketReceivedAtServer(Ptr<const Packet> packet, Address from) {
	int staId;
	if (!config.useV6)
		staId = getSTAIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
	else
		staId = getSTAIdFromAddress(Inet6SocketAddress::ConvertFrom(from).GetIpv6());
    if (staId != -1)
        nodes[staId]->OnUdpPacketReceivedAtAP(packet);
    else
    	cout << "*** Node could not be determined from received packet at AP " << endl;
}

void coapPacketReceivedAtServer(Ptr<const Packet> packet, Address from) {
	int staId;
	if (!config.useV6)
		staId = getSTAIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
	else
		staId = getSTAIdFromAddress(Inet6SocketAddress::ConvertFrom(from).GetIpv6());
    if (staId != -1)
        nodes[staId]->OnCoapPacketReceivedAtServer(packet); ///ami OnCoapPacketReceivedAtAP
    else
    	cout << "*** Node could not be determined from received packet at server " << endl;
}

void tcpPacketReceivedAtServer (Ptr<const Packet> packet, Address from) {
	int staId;
	if (!config.useV6)
		staId = getSTAIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
	else
		staId = getSTAIdFromAddress(Inet6SocketAddress::ConvertFrom(from).GetIpv6());
	if (staId != -1)
        nodes[staId]->OnTcpPacketReceivedAtAP(packet);
    else
    	cout << "*** Node could not be determined from received packet at AP " << endl;
}

void tcpRetransmissionAtServer(Address to) {
	int staId;
	std::cout << "tcpRetransmissionAtServer OK****************************"; //ami

	if (!config.useV6)
		staId = getSTAIdFromAddress(Ipv4Address::ConvertFrom(to));
	else staId = getSTAIdFromAddress(Ipv6Address::ConvertFrom(to));
	if (staId != -1)
		nodes[staId]->OnTcpRetransmissionAtAP();
	else
		cout << "*** Node could not be determined from received packet at AP " << endl;
}

void tcpPacketDroppedAtServer(Address to, Ptr<Packet> packet, DropReason reason) {
	unused(packet);
	int staId;
	if (!config.useV6)
		staId = getSTAIdFromAddress(Ipv4Address::ConvertFrom(to));
	else staId = getSTAIdFromAddress(Ipv6Address::ConvertFrom(to));
	if(staId != -1) {
		stats.get(staId).NumberOfDropsByReasonAtAP[reason]++;
	}
}

void tcpStateChangeAtServer(TcpSocket::TcpStates_t oldState, TcpSocket::TcpStates_t newState, Address to) {
	int staId;
	if (!config.useV6)
		staId = getSTAIdFromAddress(InetSocketAddress::ConvertFrom(to).GetIpv4());
	else staId = getSTAIdFromAddress(Inet6SocketAddress::ConvertFrom(to).GetIpv6());
    if(staId != -1)
			nodes[staId]->OnTcpStateChangedAtAP(oldState, newState);
		else
			cout << "*** Node could not be determined from received packet at AP " << endl;
}

void tcpIPCameraDataReceivedAtServer(Address from, uint16_t nrOfBytes) {
	int staId;
	if (!config.useV6)
		staId = getSTAIdFromAddress(InetSocketAddress::ConvertFrom(from).GetIpv4());
	else staId = getSTAIdFromAddress(Inet6SocketAddress::ConvertFrom(from).GetIpv6());
	if(staId != -1)
			nodes[staId]->OnTcpIPCameraDataReceivedAtAP(nrOfBytes);
		else
			cout << "*** Node could not be determined from received packet at AP " << endl;
}

void configureUDPServer() {
    UdpServerHelper myServer(9);
    serverApp = myServer.Install(apNodes);
    serverApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&udpPacketReceivedAtServer));
    serverApp.Start(Seconds(0));

}

void configureUDPEchoServer() {
    UdpEchoServerHelper myServer(9);
    serverApp = myServer.Install(staNodes.Get(0));//apNodes
    serverApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&udpPacketReceivedAtServer));
    serverApp.Start(Seconds(0));
}

void configureTCPEchoServer() {
	TcpEchoServerHelper myServer(80);
	serverApp = myServer.Install(apNodes);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
}


void configureTCPPingPongServer() {
	// TCP ping pong is a test for the new base tcp-client and tcp-server applications
	ObjectFactory factory;
	factory.SetTypeId (TCPPingPongServer::GetTypeId ());
	factory.Set("Port", UintegerValue (81));

	Ptr<Application> tcpServer = factory.Create<TCPPingPongServer>();
	apNodes.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
}

void configureTCPPingPongClients() {

	ObjectFactory factory;
	factory.SetTypeId (TCPPingPongClient::GetTypeId ());
	factory.Set("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	factory.Set("PacketSize", UintegerValue(config.trafficPacketSize));
	factory.Set("RemoteAddress", Ipv4AddressValue (apNodeInterfaces.GetAddress(0)));


	factory.Set("RemotePort", UintegerValue (81));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	for (uint16_t i = 0; i < config.Nsta; i++) {

		Ptr<Application> tcpClient = factory.Create<TCPPingPongClient>();
		staNodes.Get(i)->AddApplication(tcpClient);
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp,i);
		nodes[i]->m_nodeType = NodeEntry::CLIENT;

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0+random));
	}
}

void configureTCPIPCameraServer() {
	ObjectFactory factory;
	factory.SetTypeId (TCPIPCameraServer::GetTypeId ());
	factory.Set("Port", UintegerValue (82));

	Ptr<Application> tcpServer = factory.Create<TCPIPCameraServer>();
	apNodes.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);

	wireTCPServer(serverApp);

	serverApp.Start(Seconds(0));

}

void configureTCPIPCameraClients() {

	ObjectFactory factory;
	factory.SetTypeId (TCPIPCameraClient::GetTypeId ());
	factory.Set("MotionPercentage", DoubleValue(config.ipcameraMotionPercentage));
	factory.Set("MotionDuration", TimeValue(Seconds(config.ipcameraMotionDuration)));
	factory.Set("DataRate", UintegerValue(config.ipcameraDataRate));

	factory.Set("PacketSize", UintegerValue(config.trafficPacketSize));

	factory.Set("RemoteAddress", Ipv4AddressValue (apNodeInterfaces.GetAddress(0)));

	factory.Set("RemotePort", UintegerValue (82));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	for (uint16_t i = 0; i < config.Nsta; i++) {
		Ptr<Application> tcpClient = factory.Create<TCPIPCameraClient>();
		staNodes.Get(i)->AddApplication(tcpClient);
		nodes[i]->m_nodeType = NodeEntry::CLIENT;
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp,i);
		clientApp.Start(MilliSeconds(0));
		clientApp.Stop(Seconds(config.simulationTime));
	}
}

void configureTCPFirmwareServer() {
	ObjectFactory factory;
	factory.SetTypeId (TCPFirmwareServer::GetTypeId ());
	factory.Set("Port", UintegerValue (83));
	factory.Set("FirmwareSize", UintegerValue (config.firmwareSize));
	factory.Set("BlockSize", UintegerValue (config.firmwareBlockSize));
	factory.Set("NewUpdateProbability", DoubleValue (config.firmwareNewUpdateProbability));

	Ptr<Application> tcpServer = factory.Create<TCPFirmwareServer>();
	apNodes.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
}

void configureTCPFirmwareClients() {
	ObjectFactory factory;
	factory.SetTypeId (TCPFirmwareClient::GetTypeId ());
	factory.Set("CorruptionProbability", DoubleValue(config.firmwareCorruptionProbability));
	factory.Set("VersionCheckInterval", TimeValue(MilliSeconds(config.firmwareVersionCheckInterval)));
	factory.Set("PacketSize", UintegerValue(config.trafficPacketSize));
	factory.Set("RemoteAddress", Ipv4AddressValue (apNodeInterfaces.GetAddress(0)));
	factory.Set("RemotePort", UintegerValue (83));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	for (uint16_t i = 0; i < config.Nsta; i++) {
		Ptr<Application> tcpClient = factory.Create<TCPFirmwareClient>();
		staNodes.Get(i)->AddApplication(tcpClient);
		nodes[i]->m_nodeType = NodeEntry::CLIENT;
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp,i);

		clientApp.Start(MilliSeconds(0));
		clientApp.Stop(Seconds(config.simulationTime));
	}
}

void configureTCPSensorServer() {
	ObjectFactory factory;
	factory.SetTypeId (TCPSensorServer::GetTypeId ());
	factory.Set("Port", UintegerValue (84));

	Ptr<Application> tcpServer = factory.Create<TCPSensorServer>();
	apNodes.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
}

void configureTCPSensorClients() {
	ObjectFactory factory;
	factory.SetTypeId (TCPSensorClient::GetTypeId ());

	factory.Set("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	factory.Set("PacketSize", UintegerValue(config.trafficPacketSize));
	factory.Set("MeasurementSize", UintegerValue(config.sensorMeasurementSize));
	factory.Set("RemoteAddress", Ipv4AddressValue (apNodeInterfaces.GetAddress(0)));
	factory.Set("RemotePort", UintegerValue (84));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	for (uint16_t i = 0; i < config.Nsta; i++) {
		Ptr<Application> tcpClient = factory.Create<TCPSensorClient>();
		staNodes.Get(i)->AddApplication(tcpClient);
		nodes[i]->m_nodeType = NodeEntry::CLIENT;
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp,i);

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0+random));
		clientApp.Stop(Seconds(config.simulationTime));
	}
}

void wireTCPServer(ApplicationContainer serverApp) {

	serverApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&tcpPacketReceivedAtServer));
	serverApp.Get(0)->TraceConnectWithoutContext("Retransmission", MakeCallback(&tcpRetransmissionAtServer));
	serverApp.Get(0)->TraceConnectWithoutContext("PacketDropped", MakeCallback(&tcpPacketDroppedAtServer));
	serverApp.Get(0)->TraceConnectWithoutContext("TCPStateChanged", MakeCallback(&tcpStateChangeAtServer));

	if(config.trafficType == "tcpipcamera") {
		serverApp.Get(0)->TraceConnectWithoutContext("DataReceived", MakeCallback(&tcpIPCameraDataReceivedAtServer));
	}
}

void wireTCPClient(ApplicationContainer clientApp, int i) {

	clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&NodeEntry::OnTcpPacketSent, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&NodeEntry::OnTcpEchoPacketReceived, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&NodeEntry::OnTcpCongestionWindowChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("RTO", MakeCallback(&NodeEntry::OnTcpRTOChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("RTT", MakeCallback(&NodeEntry::OnTcpRTTChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&NodeEntry::OnTcpSlowStartThresholdChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("EstimatedBW", MakeCallback(&NodeEntry::OnTcpEstimatedBWChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("TCPStateChanged", MakeCallback(&NodeEntry::OnTcpStateChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("Retransmission", MakeCallback(&NodeEntry::OnTcpRetransmission, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("PacketDropped", MakeCallback(&NodeEntry::OnTcpPacketDropped, nodes[i]));

	if(config.trafficType == "tcpfirmware") {
		clientApp.Get(0)->TraceConnectWithoutContext("FirmwareUpdated", MakeCallback(&NodeEntry::OnTcpFirmwareUpdated, nodes[i]));
	}
	else if(config.trafficType == "tcpipcamera") {
	    clientApp.Get(0)->TraceConnectWithoutContext("DataSent", MakeCallback(&NodeEntry::OnTcpIPCameraDataSent, nodes[i]));
	    clientApp.Get(0)->TraceConnectWithoutContext("StreamStateChanged", MakeCallback(&NodeEntry::OnTcpIPCameraStreamStateChanged, nodes[i]));
	}
}

void configureTCPEchoClients() {
	if (!config.useV6){
		TcpEchoClientHelper clientHelper(apNodeInterfaces.GetAddress(0), 80); //address of remote node
		configureTCPEchoClientHelper(clientHelper);
	}
	else { // this doesnt work because TcpEchoClientHelper doesnt support ipv6 - TODO
		//TcpEchoClientHelper clientHelper(apNodeInterfaces6.GetAddress(0, 0), 80); //address of remote node
		//configureTCPEchoClientHelper(clientHelper);
	}

}

void configureTCPEchoClientHelper(TcpEchoClientHelper& clientHelper){
	clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295u));
	clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	clientHelper.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficIntervalDeviation)));
	clientHelper.SetAttribute("PacketSize", UintegerValue(config.trafficPacketSize));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	for (uint16_t i = 0; i < config.Nsta; i++) {
		ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
		wireTCPClient(clientApp,i);
		nodes[i]->m_nodeType = NodeEntry::CLIENT;

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0+random));
	}
}

void configureUDPClients() {
	UdpClientHelper clientHelper;
	if (!config.useV6)
		clientHelper = UdpClientHelper(apNodeInterfaces.GetAddress(0), 9); //address of remote node
	else clientHelper = UdpClientHelper(apNodeInterfaces6.GetAddress(0, 0), 9); //address of remote node
    clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(config.trafficPacketSize));

    Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

    for (uint16_t i = 0; i < config.Nsta; i++) {
        ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
        clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&NodeEntry::OnUdpPacketSent, nodes[i]));
		nodes[i]->m_nodeType = NodeEntry::CLIENT;

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0+random));
        //clientApp.Stop(Seconds(simulationTime + 1));
    }
}

void configureUDPEchoClients() {
	if (!config.useV6)
	{
		Ptr<Ipv4> ip = staNodes.Get(0)->GetObject<Ipv4>();
		Ipv4InterfaceAddress iAddr = ip->GetAddress(1,0);
		UdpEchoClientHelper clientHelper (iAddr.GetLocal(), 9); //address of remote node, before it was AP
		configureUdpEchoClientHelper(clientHelper);
	}
	else
	{
		UdpEchoClientHelper clientHelper (apNodeInterfaces6.GetAddress(0,0), 9); //address of remote node
		configureUdpEchoClientHelper(clientHelper);
	}
}

void configureUdpEchoClientHelper(UdpEchoClientHelper& clientHelper){
	clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295u));
	clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	clientHelper.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficIntervalDeviation)));
	clientHelper.SetAttribute("PacketSize", UintegerValue(config.trafficPacketSize));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	for (uint16_t i = 0; i < config.Nsta; i++) {
		ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
		clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&NodeEntry::OnUdpPacketSent, nodes[i]));
		clientApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&NodeEntry::OnUdpEchoPacketReceived, nodes[i]));
		nodes[i]->m_nodeType = NodeEntry::CLIENT;

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0+random));
	}
}


/* In control loops, odd nodes (1, 3, 5, ...) are clients
 * even nodes and zeroth node can be servers (0, 2, 4, ...)
 *
 * */
void configureCoapServer() {
    CoapServerHelper myServer(5683);
    for (uint32_t i = 0; i < config.nControlLoops*2; i += 2)
    {
        serverApp = myServer.Install(staNodes.Get(i));
        nodes[i]->m_nodeType = NodeEntry::SERVER;
        serverApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&coapPacketReceivedAtServer));
        serverApp.Start(Seconds(0));
    }
}

void configureCoapClients()
{
	for (uint32_t i = 0; i < config.Nsta; i++)
	{
		if (!config.useV6)
		{
			// Configure client (cpntroller) that sends PUT control value to the server (process = sensor+actuator)
			if (i % 2 == 0 && i < 2*config.nControlLoops)
			{
				// address of remote node n0 (server)
				Ptr<Ipv4> ip = staNodes.Get(i)->GetObject<Ipv4>();
				Ipv4InterfaceAddress iAddr = ip->GetAddress(1,0);
				CoapClientHelper clientHelper (iAddr.GetLocal(), 5683);
				configureCoapClientHelper(clientHelper, i+1);
				nodes[i+1]->m_nodeType = NodeEntry::CLIENT;
			}
			else if (i >= 2*config.nControlLoops)
			{
				// Dummy clients for network congestion send packets to some external service over AP
				CoapClientHelper clientHelperDummy (externalInterfaces.GetAddress(0), 5683); //address of AP
				//std::cout << " external node address is " << externalInterfaces.GetAddress(0) << std::endl;
				clientHelperDummy.SetAttribute("MaxPackets", config.maxNumberOfPackets); //4294967295u
				clientHelperDummy.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval*2)));
				clientHelperDummy.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficInterval*2/10)));
				clientHelperDummy.SetAttribute("PayloadSize", UintegerValue(config.coapPayloadSize));
				clientHelperDummy.SetAttribute("RequestMethod", UintegerValue(1));
				clientHelperDummy.SetAttribute("MessageType", UintegerValue(1));
				Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

				ApplicationContainer clientApp = clientHelperDummy.Install(staNodes.Get(i));
				clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&NodeEntry::OnCoapPacketSent, nodes[i]));
				clientApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&NodeEntry::OnCoapPacketReceived, nodes[i]));
				nodes[i]->m_nodeType = NodeEntry::DUMMY;

				double random = m_rv->GetValue(0, 3000);
				clientApp.Start(MilliSeconds(0+random));

			}

		}
		else
		{
			// Configure client (cpntroller) that sends PUT control value to the server (process = sensor+actuator)
			if (i % 2 == 0 && i < 2*config.nControlLoops)
			{
				// address of remote node n0 (server)
				Ptr<Ipv6> ip = staNodes.Get(i)->GetObject<Ipv6>();
				Ipv6InterfaceAddress iAddr = ip->GetAddress(1,0);
				CoapClientHelper clientHelper (iAddr.GetAddress(), 5683);
				configureCoapClientHelper(clientHelper, i+1);
			}
			else if (i >= 2*config.nControlLoops)
			{
				// Dummy clients for network congestion send packets to some external service over AP
				CoapClientHelper clientHelperDummy (externalInterfaces6.GetAddress(0, 0), 5683); //address of AP
				//std::cout << " external node address is " << externalInterfaces.GetAddress(0) << std::endl;
				clientHelperDummy.SetAttribute("MaxPackets", config.maxNumberOfPackets); //4294967295u
				clientHelperDummy.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval*2)));
				clientHelperDummy.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficInterval*2)));
				clientHelperDummy.SetAttribute("PayloadSize", UintegerValue(config.coapPayloadSize));
				clientHelperDummy.SetAttribute("RequestMethod", UintegerValue(1));
				clientHelperDummy.SetAttribute("MessageType", UintegerValue(1));
				Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

				ApplicationContainer clientApp = clientHelperDummy.Install(staNodes.Get(i));
				clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&NodeEntry::OnCoapPacketSent, nodes[i]));
				clientApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&NodeEntry::OnCoapPacketReceived, nodes[i]));

				double random = m_rv->GetValue(0, 3000);
				clientApp.Start(MilliSeconds(0+random));
			}

		}
	}
}

void configureCoapClientHelper(CoapClientHelper& clientHelper, uint32_t n)
{
	clientHelper.SetAttribute("MaxPackets", config.maxNumberOfPackets);//4294967295u
	clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	clientHelper.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficIntervalDeviation)));
	clientHelper.SetAttribute("PayloadSize", UintegerValue(config.coapPayloadSize));
	clientHelper.SetAttribute("RequestMethod", UintegerValue(3));
	clientHelper.SetAttribute("MessageType", UintegerValue(0));


	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();

	ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(n));
	clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&NodeEntry::OnCoapPacketSent, nodes[n]));
	clientApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&NodeEntry::OnCoapPacketReceived, nodes[n]));
	double random = m_rv->GetValue(0, config.trafficInterval);
	clientApp.Start(MilliSeconds(0+random));

}

void onSTAAssociated(int i) {
    nodes[i]->rawGroupNumber = ((nodes[i]->aId - 1) / (config.NRawSta / config.NGroup));
    nodes[i]->rawSlotIndex = nodes[i]->aId % config.NRawSlotNum;
	cout << "Node " << std::to_string(i) << " is associated and has aId " << nodes[i]->aId << " and falls in RAW group " << std::to_string(nodes[i]->rawGroupNumber) << endl;
    eventManager.onNodeAssociated(*nodes[i]);

    uint32_t nrOfSTAAssociated (0);
    for (uint32_t i = 0; i < config.Nsta; i++) {
        if (nodes[i]->isAssociated)
            nrOfSTAAssociated++;
    }

    if (nrOfSTAAssociated == config.Nsta) {
    	cout << "All stations associated, configuring clients & server" << endl;
        // association complete, start sending packets
    	stats.TimeWhenEverySTAIsAssociated = Simulator::Now();

    	if(config.trafficType == "udp") {
    		configureUDPServer();
    		configureUDPClients();
    	}
    	else if(config.trafficType == "udpecho") {
    		configureUDPEchoServer();
    		configureUDPEchoClients();
    	}
    	else if(config.trafficType == "tcpecho") {
			configureTCPEchoServer();
			configureTCPEchoClients();
    	}
    	else if(config.trafficType == "tcppingpong") {
    		configureTCPPingPongServer();
			configureTCPPingPongClients();
		}
    	else if(config.trafficType == "tcpipcamera") {
			configureTCPIPCameraServer();
			configureTCPIPCameraClients();
		}
    	else if(config.trafficType == "tcpfirmware") {
			configureTCPFirmwareServer();
			configureTCPFirmwareClients();
		}
    	else if(config.trafficType == "tcpsensor") {
			configureTCPSensorServer();
			configureTCPSensorClients();
    	}
    	else if (config.trafficType == "coap") {
			configureCoapServer();
			configureCoapClients();
		}
        updateNodesQueueLength();
    }
}

void onSTADeassociated(int i) {
	eventManager.onNodeDeassociated(*nodes[i]);
}

void updateNodesQueueLength() {
	for (uint32_t i = 0; i < config.Nsta; i++) {
		nodes[i]->UpdateQueueLength();
		stats.get(i).EDCAQueueLength = nodes[i]->queueLength;
	}
	Simulator::Schedule(Seconds(0.5), &updateNodesQueueLength);
}

void printStatistics() {
	cout << "Statistics" << endl;
	cout << "----------" << endl;
	cout << "Total simulation time: " << std::to_string(stats.TotalSimulationTime.GetMilliSeconds()) << "ms" << endl;
	cout << "Time every station associated: " << std::to_string(stats.TimeWhenEverySTAIsAssociated.GetMilliSeconds()) << "ms" << endl;
	cout << "" << endl;

	for (uint32_t i = 0; i < config.Nsta; i++) {
		if (nodes[i]->m_nodeType == NodeEntry::CLIENT){
			cout << "Node " << std::to_string(i) << endl;
			cout << "X: " << nodes[i]->x << ", Y: " << nodes[i]->y << endl;
			cout << "Tx Remaining Queue size: " << nodes[i]->queueLength << endl;
			cout << "Tcp congestion window value: " <<  std::to_string(stats.get(i).TCPCongestionWindow) << endl;
			cout << "--------------" << endl;
			cout << "Total transmit time: " << std::to_string(stats.get(i).TotalTransmitTime.GetMilliSeconds()) << "ms" << endl;
			cout << "Total receive time: " << std::to_string(stats.get(i).TotalReceiveTime.GetMilliSeconds()) << "ms" << endl;
			cout << "" << endl;
			cout << "Total active time: " << std::to_string(stats.get(i).TotalActiveTime.GetMilliSeconds()) << "ms" << endl;
			cout << "Total doze time: " << std::to_string(stats.get(i).TotalDozeTime.GetMilliSeconds()) << "ms" << endl;
			cout << "" << endl;
			cout << "Number of transmissions: " << std::to_string(stats.get(i).NumberOfTransmissions) << endl;
			cout << "Number of transmissions dropped: " << std::to_string(stats.get(i).NumberOfTransmissionsDropped) << endl;
			cout << "Number of receives: " << std::to_string(stats.get(i).NumberOfReceives) << endl;
			cout << "Number of receives dropped: " << std::to_string(stats.get(i).NumberOfReceivesDropped) << endl;
			cout << "" << endl;
			cout << "Number of packets sent: " << std::to_string(stats.get(i).NumberOfSentPackets) << endl; // CORRECT
			cout << "Number of packets successfuly arrived to the dst: " << std::to_string(stats.get(i).NumberOfSuccessfulPackets) << endl; //CORRECT
			cout << "Number of packets dropped: " << std::to_string(stats.get(i).getNumberOfDroppedPackets()) << endl; // NOT CORRECT
			cout << "Number of roundtrip packets successful: " << std::to_string(stats.get(i).NumberOfSuccessfulRoundtripPackets) << endl; //CORRECT
			cout << "Average packet sent/receive time: " << std::to_string(stats.get(i).getAveragePacketSentReceiveTime().GetMicroSeconds()) << "µs" << std::endl; // CORRECT
			cout << "Average packet roundtrip time: " << std::to_string(stats.get(i).getAveragePacketRoundTripTime(config.trafficType).GetMicroSeconds()) << "µs" << std::endl; //not
			// Average packet roundtrip time NOT CORRECT, uracunato je send-rcv vrijeme dropped paketa a dijeljeno je samo sa brojem successfull RTT paketa
			cout << "IP Camera Data sending rate: " << stats.get(i).getIPCameraSendingRate() << "kbps" << std::endl;
			cout << "IP Camera Data receiving rate: " << std::to_string(stats.get(i).getIPCameraAPReceivingRate()) << "kbps" << std::endl;
			cout << endl;
			cout << "    global max Latency (C->S)= " << std::to_string(NodeEntry::maxLatency.GetMicroSeconds()) << "µs" << endl; // CORRECT
			cout << "    global min Latency (C->S) = " << std::to_string(NodeEntry::minLatency.GetMicroSeconds()) << "µs" << endl; // CORRECT
			cout << "    max diference in RTT between 2 subsequent packets = " << std::to_string(NodeEntry::maxJitter.GetMicroSeconds()) << "µs" << endl;
			cout << "    min diference in RTT between 2 subsequent packets = " << std::to_string(NodeEntry::minJitter.GetMicroSeconds()) << "µs" << endl;
			cout << "    Jitter (RTT)= " << std::to_string(stats.get(i).GetAverageJitter()) << "ms" << endl;
			cout << "    Average inter packet delay at server is " << std::to_string(stats.get(i).GetAverageInterPacketDelay(stats.get(i).m_interPacketDelayServer).GetMicroSeconds()) << "µs" << endl;
			cout << "    Inter-packet-delay at the server standard deviation is " << stats.get(i).GetInterPacketDelayDeviation(stats.get(i).m_interPacketDelayServer) << " which is " << stats.get(i).GetInterPacketDelayDeviationPercentage(stats.get(i).m_interPacketDelayServer) << "%" <<endl;
			cout << "    Average inter packet delay at client is " << std::to_string(stats.get(i).GetAverageInterPacketDelay(stats.get(i).m_interPacketDelayClient).GetMicroSeconds()) << "µs" << endl;
			cout << "    Inter-packet-delay at the client standard deviation is " << stats.get(i).GetInterPacketDelayDeviation(stats.get(i).m_interPacketDelayClient) << " which is " << stats.get(i).GetInterPacketDelayDeviationPercentage(stats.get(i).m_interPacketDelayClient) << "%" <<endl;
			//calculate the deviation between inter packet arrival times at the server
			cout << "    Reliability " << std::to_string(stats.get(i).GetReliability()) << "%" << endl; //CORRECT

			/*cout << "    Total packet send receive time " << std::to_string(stats.get(i).TotalPacketSentReceiveTime.GetMilliSeconds()) << " ms" << endl;
			cout << "    Total packet payload size " << std::to_string(stats.get(i).TotalPacketPayloadSize) << endl;*/


			cout << endl;
			cout << "Goodput: " << (stats.get(i).getGoodputKbit()) << "Kbit" << endl; //CORRECT
			cout << "*********************" << endl;
			/*std::vector<Time> x = stats.get(i).m_interPacketDelay;
		for (uint32_t i=0; i< x.size(); i++)
		{
			cout << "S: " << (x[i]).GetMicroSeconds() << ", ";
		}*/
			cout << endl;
		}
		else if (nodes[i]->m_nodeType == NodeEntry::SERVER)
		{
			cout << "Node " << std::to_string(i) << endl;
			cout << "X: " << nodes[i]->x << ", Y: " << nodes[i]->y << endl;
			cout << "Tx Remaining Queue size: " << nodes[i]->queueLength << endl;
		}

	}
}

void sendStatistics(bool schedule) {
	eventManager.onUpdateStatistics(stats);
	eventManager.onUpdateSlotStatistics(transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval, transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval);
	// reset
	transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval = vector<long>(config.NGroup * config.NRawSlotNum, 0);
	transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval = vector<long>(config.NGroup * config.NRawSlotNum, 0);

	if(schedule)
		Simulator::Schedule(Seconds(config.visualizerSamplingInterval), &sendStatistics, true);
}

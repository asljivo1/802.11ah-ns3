#include "s1g-ap-wifi-mac.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/boolean.h"
#include "qos-tag.h"
#include "wifi-phy.h"
#include "dcf-manager.h"
#include "mac-rx-middle.h"
#include "mac-tx-middle.h"
#include "mgt-headers.h"
#include "extension-headers.h"
#include "mac-low.h"
#include "amsdu-subframe-header.h"
#include "msdu-aggregator.h"
#include "ns3/uinteger.h"
#include "wifi-mac-queue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("S1gApWifiMac");

NS_OBJECT_ENSURE_REGISTERED(S1gApWifiMac);

#define LOG_TRAFFIC(msg)	if(true) NS_LOG_DEBUG(Simulator::Now().GetMicroSeconds() << " " << msg << std::endl);

TypeId S1gApWifiMac::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::S1gApWifiMac").SetParent<RegularWifiMac>().SetGroupName(
					"Wifi").AddConstructor<S1gApWifiMac>()

			.AddAttribute("BeaconInterval", "Delay between two beacons",
					TimeValue(MicroSeconds(102400)),
					MakeTimeAccessor(&S1gApWifiMac::GetBeaconInterval,
							&S1gApWifiMac::SetBeaconInterval),
					MakeTimeChecker())

			.AddAttribute("BeaconJitter",
					"A uniform random variable to cause the initial beacon starting time (after simulation time 0) "
							"to be distributed between 0 and the BeaconInterval.",
					StringValue("ns3::UniformRandomVariable"),
					MakePointerAccessor(&S1gApWifiMac::m_beaconJitter),
					MakePointerChecker<UniformRandomVariable>())

			.AddAttribute("EnableBeaconJitter",
					"If beacons are enabled, whether to jitter the initial send event.",
					BooleanValue(false),
					MakeBooleanAccessor(&S1gApWifiMac::m_enableBeaconJitter),
					MakeBooleanChecker())

			.AddAttribute("ScheduleTransmissionForNextSlotIfLessThan",
					"If the AP is trying to send the transmission in the same slot to the STA, make sure to have at least x time remaining inside the slot",
					TimeValue(MilliSeconds(2)),
					MakeTimeAccessor(
							&S1gApWifiMac::m_scheduleTransmissionForNextSlotIfLessThan),
					MakeTimeChecker())

			.AddAttribute("BeaconGeneration",
					"Whether or not beacons are generated.", BooleanValue(true),
					MakeBooleanAccessor(&S1gApWifiMac::SetBeaconGeneration,
							&S1gApWifiMac::GetBeaconGeneration),
					MakeBooleanChecker()).AddAttribute(
					"AlwaysScheduleForNextSlot",
					"If true never transmit immediately if it's possible to transmit during the slot period",
					BooleanValue(false),
					MakeBooleanAccessor(
							&S1gApWifiMac::SetAlwaysScheduleForNextSlot,
							&S1gApWifiMac::GetAlwaysScheduleForNextSlot),
					MakeBooleanChecker())

			.AddAttribute("NRawGroupStas",
					"Number of stations in one Raw Group", UintegerValue(6000),
					MakeUintegerAccessor(&S1gApWifiMac::GetRawGroupInterval,
							&S1gApWifiMac::SetRawGroupInterval),
					MakeUintegerChecker<uint32_t>()).AddAttribute(
					"NRawStations", "Number of total stations support RAW",
					UintegerValue(100),
					MakeUintegerAccessor(&S1gApWifiMac::GetTotalStaNum,
							&S1gApWifiMac::SetTotalStaNum),
					MakeUintegerChecker<uint32_t>()).AddAttribute("SlotFormat",
					"Slot format", UintegerValue(1),
					MakeUintegerAccessor(&S1gApWifiMac::GetSlotFormat,
							&S1gApWifiMac::SetSlotFormat),
					MakeUintegerChecker<uint32_t>()).AddAttribute(
					"SlotCrossBoundary", "cross slot boundary or not",
					UintegerValue(0),
					MakeUintegerAccessor(&S1gApWifiMac::GetSlotCrossBoundary,
							&S1gApWifiMac::SetSlotCrossBoundary),
					MakeUintegerChecker<uint32_t>())

			.AddAttribute("SlotDurationCount", "slot duration count",
					UintegerValue(1000),
					MakeUintegerAccessor(&S1gApWifiMac::GetSlotDurationCount,
							&S1gApWifiMac::SetSlotDurationCount),
					MakeUintegerChecker<uint32_t>())

			.AddAttribute("SlotNum", "Number of slot", UintegerValue(2),
					MakeUintegerAccessor(&S1gApWifiMac::GetSlotNum,
							&S1gApWifiMac::SetSlotNum),
					MakeUintegerChecker<uint32_t>())

			.AddTraceSource("S1gBeaconBroadcasted",
					"Fired when a beacon is transmitted",
					MakeTraceSourceAccessor(
							&S1gApWifiMac::m_transmitBeaconTrace),
					"ns3::S1gApWifiMac::S1gBeaconTracedCallback")

			.AddTraceSource("RAWSlotStarted",
					"Fired when a RAW slot has started",
					MakeTraceSourceAccessor(&S1gApWifiMac::m_rawSlotStarted),
					"ns3::S1gApWifiMac::RawSlotStartedCallback")

			.AddTraceSource("PacketToTransmitReceivedFromUpperLayer",
					"Fired when packet is received from the upper layer",
					MakeTraceSourceAccessor(
							&S1gApWifiMac::m_packetToTransmitReceivedFromUpperLayer),
					"ns3::S1gApWifiMac::PacketToTransmitReceivedFromUpperLayerCallback")

			.AddAttribute("MaxTimeInQueue",
					"The max. time a packet stays in the DCA queue before it's dropped",
					TimeValue(MilliSeconds(10000)),
					MakeTimeAccessor(&S1gApWifiMac::m_maxTimeInQueue),
					MakeTimeChecker())

					;

	return tid;
}

S1gApWifiMac::S1gApWifiMac() {
	NS_LOG_FUNCTION(this);
	m_beaconDca = CreateObject<DcaTxop>();
	m_beaconDca->SetAifsn(1);
	m_beaconDca->SetMinCw(0);
	m_beaconDca->SetMaxCw(0);
	m_beaconDca->SetLow(m_low);
	m_beaconDca->SetManager(m_dcfManager);
	m_beaconDca->SetTxMiddle(m_txMiddle);

	//Let the lower layers know that we are acting as an AP.
	SetTypeOfStation(AP);

	m_enableBeaconGeneration = false;
	AuthenThreshold = 0;
	//m_SlotFormat = 0;
}

S1gApWifiMac::~S1gApWifiMac() {
	NS_LOG_FUNCTION(this);
}

void S1gApWifiMac::DoDispose() {
	NS_LOG_FUNCTION(this);
	m_beaconDca = 0;
	m_enableBeaconGeneration = false;

	if (strategy != nullptr)
		delete strategy;
	strategy = nullptr;

	for (uint32_t i = 0; i < rawSlotsDCA.size(); i++) {
		rawSlotsDCA[i] = 0;
	}

	m_beaconEvent.Cancel();
	RegularWifiMac::DoDispose();
}

void S1gApWifiMac::SetAddress(Mac48Address address) {
	NS_LOG_FUNCTION(this << address);
	//As an AP, our MAC address is also the BSSID. Hence we are
	//overriding this function and setting both in our parent class.
	RegularWifiMac::SetAddress(address);
	RegularWifiMac::SetBssid(address);
}

void S1gApWifiMac::SetBeaconGeneration(bool enable) {
	NS_LOG_FUNCTION(this << enable);
	if (!enable) {
		m_beaconEvent.Cancel();
	} else if (enable && !m_enableBeaconGeneration) {
		m_beaconEvent = Simulator::ScheduleNow(&S1gApWifiMac::SendOneBeacon,
				this);
	}
	m_enableBeaconGeneration = enable;
}

bool S1gApWifiMac::GetBeaconGeneration(void) const {
	NS_LOG_FUNCTION(this);
	return m_enableBeaconGeneration;
}

Time S1gApWifiMac::GetBeaconInterval(void) const {
	NS_LOG_FUNCTION(this);
	return m_beaconInterval;
}

bool S1gApWifiMac::GetAlwaysScheduleForNextSlot(void) const {
	NS_LOG_FUNCTION(this);
	return m_alwaysScheduleForNextSlot;
}

void S1gApWifiMac::SetAlwaysScheduleForNextSlot(bool value) {
	NS_LOG_FUNCTION(this);
	m_alwaysScheduleForNextSlot = value;
}

uint32_t S1gApWifiMac::GetRawGroupInterval(void) const {
	NS_LOG_FUNCTION(this);
	return m_rawGroupInterval;
}

uint32_t S1gApWifiMac::GetTotalStaNum(void) const {
	NS_LOG_FUNCTION(this);
	return m_totalStaNum;
}

uint32_t S1gApWifiMac::GetSlotFormat(void) const {
	return m_SlotFormat;
}

uint32_t S1gApWifiMac::GetSlotCrossBoundary(void) const {
	return m_slotCrossBoundary;
}

uint32_t S1gApWifiMac::GetSlotDurationCount(void) const {
	return m_slotDurationCount;
}

uint32_t S1gApWifiMac::GetSlotNum(void) const {
	return m_slotNum;
}

void S1gApWifiMac::SetWifiRemoteStationManager(
		Ptr<WifiRemoteStationManager> stationManager) {
	NS_LOG_FUNCTION(this << stationManager);
	m_beaconDca->SetWifiRemoteStationManager(stationManager);

	for (auto& p : rawSlotsDCA) {
		p->SetWifiRemoteStationManager(stationManager);
	}

	RegularWifiMac::SetWifiRemoteStationManager(stationManager);
}

void S1gApWifiMac::SetLinkUpCallback(Callback<void> linkUp) {
	NS_LOG_FUNCTION(this << &linkUp);
	RegularWifiMac::SetLinkUpCallback(linkUp);

	//The approach taken here is that, from the point of view of an AP,
	//the link is always up, so we immediately invoke the callback if
	//one is set
	linkUp();
}

void S1gApWifiMac::SetBeaconInterval(Time interval) {
	NS_LOG_FUNCTION(this << interval);
	if ((interval.GetMicroSeconds() % 1024) != 0) {
		NS_LOG_WARN(
				"beacon interval should be multiple of 1024us (802.11 time unit), see IEEE Std. 802.11-2012");
	}
	m_beaconInterval = interval;
}

void S1gApWifiMac::SetRawGroupInterval(uint32_t interval) {
	NS_LOG_FUNCTION(this << interval);
	m_rawGroupInterval = interval;
}

void S1gApWifiMac::SetTotalStaNum(uint32_t num) {
	NS_LOG_FUNCTION(this << num);
	m_totalStaNum = num;
}

void S1gApWifiMac::SetSlotFormat(uint32_t format) {
	NS_ASSERT(format <= 1);
	m_SlotFormat = format;
}

void S1gApWifiMac::SetSlotCrossBoundary(uint32_t cross) {
	NS_ASSERT(cross <= 1);
	m_slotCrossBoundary = cross;
}

void S1gApWifiMac::SetSlotDurationCount(uint32_t count) {
	NS_ASSERT(
			(!m_SlotFormat & (count < 256)) || (m_SlotFormat & (count < 2048)));
	m_slotDurationCount = count;
}

void S1gApWifiMac::SetSlotNum(uint32_t count) {
	NS_ASSERT((!m_SlotFormat & (count < 64)) || (m_SlotFormat & (count < 8)));
	m_slotNum = count;
}

void S1gApWifiMac::StartBeaconing(void) {
	NS_LOG_FUNCTION(this);
	SendOneBeacon();
}

int64_t S1gApWifiMac::AssignStreams(int64_t stream) {
	NS_LOG_FUNCTION(this << stream);
	m_beaconJitter->SetStream(stream);
	return 1;
}

void S1gApWifiMac::ForwardDown(Ptr<const Packet> packet, Mac48Address from, Mac48Address to) {
	NS_LOG_FUNCTION(this << packet << from << to);
	//If we are not a QoS AP then we definitely want to use AC_BE to
	//transmit the packet. A TID of zero will map to AC_BE (through \c
	//QosUtilsMapTidToAc()), so we use that as our default here.
	uint8_t tid = 0;

	uint16_t aId = macToAIDMap[to];
	pendingDataSizeForStations[aId - 1]--;
	LOG_TRAFFIC(
			Simulator::Now().GetMicroSeconds() << " Data for [" << aId << "] --" << std::endl);

	//If we are a QoS AP then we attempt to get a TID for this packet
	if (m_qosSupported) {
		tid = QosUtilsGetTidForPacket(packet);
		//Any value greater than 7 is invalid and likely indicates that
		//the packet had no QoS tag, so we revert to zero, which'll
		//mean that AC_BE is used.
		if (tid > 7) {
			tid = 0;
		}
	}

	ForwardDown(packet, from, to, tid);
}

void S1gApWifiMac::ForwardDown(Ptr<const Packet> packet, Mac48Address from,	Mac48Address to, uint8_t tid) {
	NS_LOG_FUNCTION(
			this << packet << from << to << static_cast<uint32_t> (tid));
	WifiMacHeader hdr;

	//For now, an AP that supports QoS does not support non-QoS
	//associations, and vice versa. In future the AP model should
	//support simultaneously associated QoS and non-QoS STAs, at which
	//point there will need to be per-association QoS state maintained
	//by the association state machine, and consulted here.
	if (m_qosSupported) {
		hdr.SetType(WIFI_MAC_QOSDATA);
		hdr.SetQosAckPolicy(WifiMacHeader::NORMAL_ACK);
		hdr.SetQosNoEosp();
		hdr.SetQosNoAmsdu();
		//Transmission of multiple frames in the same TXOP is not
		//supported for now
		hdr.SetQosTxopLimit(0);
		//Fill in the QoS control field in the MAC header
		hdr.SetQosTid(tid);
	} else {
		hdr.SetTypeData();
	}

	if (m_htSupported) {
		hdr.SetNoOrder();
	}
	hdr.SetAddr1(to);
	hdr.SetAddr2(GetAddress());
	hdr.SetAddr3(from);
	hdr.SetDsFrom();
	hdr.SetDsNotTo();

	if (macToAIDMap.find(to) != macToAIDMap.end()) {
		auto aId = macToAIDMap[to];

		auto targetTIMGroup = strategy->GetTIMGroupFromAID(aId,
				m_rawGroupInterval);
		auto targetSlotIndex = strategy->GetSlotIndexFromAID(aId, m_slotNum);

		// queue the packet in the specific raw slot period DCA
		rawSlotsDCA[targetTIMGroup * m_slotNum + targetSlotIndex]->Queue(packet,
				hdr);
	} else {
		if (m_qosSupported) {
			//Sanity check that the TID is valid
			NS_ASSERT(tid < 8);
			m_edca[QosUtilsMapTidToAc(tid)]->Queue(packet, hdr);
		} else {
			m_dca->Queue(packet, hdr);
		}
	}
}

void S1gApWifiMac::Enqueue(Ptr<const Packet> packet, Mac48Address to, Mac48Address from) {
	NS_LOG_FUNCTION(this << packet << to << from);
	if (to.IsBroadcast() || to.IsMulticast6() || m_stationManager->IsAssociated(to)) {  //ami
		uint16_t aId = macToAIDMap[to];

		pendingDataSizeForStations[aId - 1]++;
		LOG_TRAFFIC(Simulator::Now().GetMicroSeconds() << " Data for [" << aId << "] ++");

		uint8_t slotIndex = strategy->GetSlotIndexFromAID(aId, m_slotNum);
		// RAW period start is 0 at the moment
		Time slotDuration = strategy->GetSlotDuration(m_slotDurationCount);
		Time slotTimeOffset = MicroSeconds(0) + slotDuration * slotIndex;
		int timGroup = strategy->GetTIMGroupFromAID(aId, m_rawGroupInterval);

		bool schedulePacketForNextSlot = true;
		Time timeRemaining = Time(0);
		bool inSlot = false;

		if (!m_alwaysScheduleForNextSlot && staIsActiveDuringCurrentCycle[aId - 1]) {

			// station is active in its respective slot until at least the next DTIM beacon is sent
			// calculate if we are still inside the appropriate slot and transmit immediately if we are
			// that way we can avoid having to wait an entire cycle until the next slot comes up
			if (timGroup == m_currentBeaconTIMGroup) {
				// we're still in the correct TIM group, let's see if we're still inside the slot
				Time currentOffsetSinceLastBeacon = (Simulator::Now() - lastBeaconTime);
				if (currentOffsetSinceLastBeacon >= slotTimeOffset && currentOffsetSinceLastBeacon <= slotTimeOffset + slotDuration) {
					// still inside the slot too!, send packet immediately, if there is still enough time
					timeRemaining = slotTimeOffset + slotDuration - currentOffsetSinceLastBeacon;
					inSlot = true;
					if (timeRemaining > m_scheduleTransmissionForNextSlotIfLessThan) {
						schedulePacketForNextSlot = false;
						LOG_TRAFFIC(Simulator::Now().GetMicroSeconds() << " Data for [" << aId << "] is transmitted immediately because AP can still get it out during the STA slot, in which the STA is actively listening, there's " << timeRemaining.GetMicroSeconds() << "µs remaining until slot is over");
					} else {
						//std::cout << "AP can't send the transmission directly, not enough time left (" << timeRemaining.GetMicroSeconds() << "µs while " << m_scheduleTransmissionForNextSlotIfLessThan.GetMicroSeconds() << " was required " << std::endl;
					}

				}
			}
		}

		m_packetToTransmitReceivedFromUpperLayer(packet, to, schedulePacketForNextSlot, inSlot, timeRemaining);

		if (schedulePacketForNextSlot) {
			int remainingBeacons = 0;
			int curStart = current_aid_start;
			int curTIMBeaconNr = m_currentBeaconTIMGroup;
			bool dtimBeaconPassed = false;

			while (!(curTIMBeaconNr == timGroup && dtimBeaconPassed)) {

				remainingBeacons++;
				curTIMBeaconNr = (curTIMBeaconNr + 1);
				curStart += m_rawGroupInterval;
				if (curStart + m_rawGroupInterval > m_totalStaNum)
					curStart = 1;

				// ensure that the packet is enqueued to be sent AFTER
				// a DTIM beacon has passed or the STA might not be listening
				// because the bitmap of the previous DTIM will have indicated
				// there's no data for that group

				if (curTIMBeaconNr >= m_nrOfTIMGroups) {
					curTIMBeaconNr = 0;
					dtimBeaconPassed = true;
				}
			}

			//std::cout << "Current TIM beacon " << std::to_string(m_currentBeaconTIMGroup) << ", scheduling packet in " << remainingBeacons << " beacons" << std::endl;
			Time packetSendTime = m_beaconInterval * remainingBeacons;
			packetSendTime += slotTimeOffset;

			//align with beacon interval
			packetSendTime -= (Simulator::Now() - lastBeaconTime);

			// choose a time between 0 and 50% of the slot time
			//packetSendTime = packetSendTime + MicroSeconds(rand() % (strategy->GetSlotDuration(m_slotDurationCount) / 2).GetMicroSeconds());

			// transmit in second half of RAW window
			//packetSendTime += MilliSeconds(m_beaconInterval/2);
			void (S1gApWifiMac::*fp)(Ptr<const Packet>, Mac48Address,
					Mac48Address) = &S1gApWifiMac::ForwardDown;
			Simulator::Schedule(packetSendTime, fp, this, packet, from, to);
		} else {
			// still within the slot, transmit immediately, gogogo
			ForwardDown(packet, from, to);
		}
	}
}

void S1gApWifiMac::Enqueue(Ptr<const Packet> packet, Mac48Address to) {
	NS_LOG_FUNCTION(this << packet << to);
	//We're sending this packet with a from address that is our own. We
	//get that address from the lower MAC and make use of the
	//from-spoofing Enqueue() method to avoid duplicated code.
	Enqueue(packet, to, m_low->GetAddress());
}

bool S1gApWifiMac::SupportsSendFrom(void) const {
	NS_LOG_FUNCTION(this);
	return true;
}

SupportedRates S1gApWifiMac::GetSupportedRates(void) const {
	NS_LOG_FUNCTION(this);
	SupportedRates rates;
	//If it is an HT-AP then add the BSSMembershipSelectorSet
	//which only includes 127 for HT now. The standard says that the BSSMembershipSelectorSet
	//must have its MSB set to 1 (must be treated as a Basic Rate)
	//Also the standard mentioned that at leat 1 element should be included in the SupportedRates the rest can be in the ExtendedSupportedRates
	if (m_htSupported) {
		for (uint32_t i = 0; i < m_phy->GetNBssMembershipSelectors(); i++) {
			rates.SetBasicRate(m_phy->GetBssMembershipSelector(i));
		}
	}
	//Send the set of supported rates and make sure that we indicate
	//the Basic Rate set in this set of supported rates.
	// NS_LOG_LOGIC ("ApWifiMac::GetSupportedRates  1 " ); //for test
	for (uint32_t i = 0; i < m_phy->GetNModes(); i++) {
		WifiMode mode = m_phy->GetMode(i);
		rates.AddSupportedRate(mode.GetDataRate());
		//Add rates that are part of the BSSBasicRateSet (manufacturer dependent!)
		//here we choose to add the mandatory rates to the BSSBasicRateSet,
		//exept for 802.11b where we assume that only the two lowest mandatory rates are part of the BSSBasicRateSet
		if (mode.IsMandatory()
				&& ((mode.GetModulationClass() != WIFI_MOD_CLASS_DSSS)
						|| mode == WifiPhy::GetDsssRate1Mbps()
						|| mode == WifiPhy::GetDsssRate2Mbps())) {
			m_stationManager->AddBasicMode(mode);
		}
	}
	// NS_LOG_LOGIC ("ApWifiMac::GetSupportedRates  2 " ); //for test
	//set the basic rates
	for (uint32_t j = 0; j < m_stationManager->GetNBasicModes(); j++) {
		WifiMode mode = m_stationManager->GetBasicMode(j);
		rates.SetBasicRate(mode.GetDataRate());
	}
	//NS_LOG_LOGIC ("ApWifiMac::GetSupportedRates   " ); //for test
	return rates;
}

HtCapabilities S1gApWifiMac::GetHtCapabilities(void) const {
	HtCapabilities capabilities;
	capabilities.SetHtSupported(1);
	capabilities.SetLdpc(m_phy->GetLdpc());
	capabilities.SetShortGuardInterval20(m_phy->GetGuardInterval());
	capabilities.SetGreenfield(m_phy->GetGreenfield());
	for (uint8_t i = 0; i < m_phy->GetNMcs(); i++) {
		capabilities.SetRxMcsBitmask(m_phy->GetMcs(i));
	}
	return capabilities;
}

void S1gApWifiMac::SendProbeResp(Mac48Address to) {
	NS_LOG_FUNCTION(this << to);
	WifiMacHeader hdr;
	hdr.SetProbeResp();
	hdr.SetAddr1(to);
	hdr.SetAddr2(GetAddress());
	hdr.SetAddr3(GetAddress());
	hdr.SetDsNotFrom();
	hdr.SetDsNotTo();
	Ptr<Packet> packet = Create<Packet>();
	MgtProbeResponseHeader probe;
	probe.SetSsid(GetSsid());
	probe.SetSupportedRates(GetSupportedRates());
	probe.SetBeaconIntervalUs(m_beaconInterval.GetMicroSeconds());
	if (m_htSupported) {
		probe.SetHtCapabilities(GetHtCapabilities());
		hdr.SetNoOrder();
	}
	packet->AddHeader(probe);

	//The standard is not clear on the correct queue for management
	//frames if we are a QoS AP. The approach taken here is to always
	//use the DCF for these regardless of whether we have a QoS
	//association or not.
	m_dca->Queue(packet, hdr);
}

void S1gApWifiMac::SendAssocResp(Mac48Address to, bool success) {
	NS_LOG_FUNCTION(this << to << success);
	WifiMacHeader hdr;
	hdr.SetAssocResp();
	hdr.SetAddr1(to);
	hdr.SetAddr2(GetAddress());
	hdr.SetAddr3(GetAddress());
	hdr.SetDsNotFrom();
	hdr.SetDsNotTo();
	Ptr<Packet> packet = Create<Packet>();
	MgtAssocResponseHeader assoc;

	uint16_t aid = strategy->GetAIDFromMacAddress(to);
	// keep track of a mac -> aid map
	macToAIDMap[to] = aid;

	assoc.SetAID(aid); //
	StatusCode code;
	if (success) {
		code.SetSuccess();
	} else {
		code.SetFailure();
	}
	assoc.SetSupportedRates(GetSupportedRates());
	assoc.SetStatusCode(code);

	if (m_htSupported) {
		assoc.SetHtCapabilities(GetHtCapabilities());
		hdr.SetNoOrder();
	}
	packet->AddHeader(assoc);

	//The standard is not clear on the correct queue for management
	//frames if we are a QoS AP. The approach taken here is to always
	//use the DCF for these regardless of whether we have a QoS
	//association or not.
	m_dca->Queue(packet, hdr);
}

void S1gApWifiMac::SendOneBeacon(void) {
	NS_LOG_FUNCTION(this);
	WifiMacHeader hdr;

	lastBeaconTime = Simulator::Now();

	m_currentBeaconTIMGroup = (m_currentBeaconTIMGroup + 1) % m_nrOfTIMGroups;

	hdr.SetS1gBeacon();
	//hdr.SetAddr1(Mac48Address::GetBroadcast());
	hdr.SetAddr1(Mac48Address::GetMulticast(Ipv6Address::GetAllNodesMulticast())); // to work with IPv6 as well
	hdr.SetAddr2(GetAddress()); // for debug, not accordance with draft, need change
	hdr.SetAddr3(GetAddress()); // for debug

	Ptr<Packet> packet = Create<Packet>();
	S1gBeaconHeader beacon;
	S1gBeaconCompatibility compatibility;
	compatibility.SetBeaconInterval(m_beaconInterval.GetMicroSeconds());
	beacon.SetBeaconCompatibility(compatibility);
	RPS m_rps;
	RPS::RawAssignment raw;
	uint8_t control = 0;
	raw.SetRawControl(control); //support paged STA or not
	raw.SetSlotFormat(m_SlotFormat);
	raw.SetSlotCrossBoundary(m_slotCrossBoundary);
	raw.SetSlotDurationCount(m_slotDurationCount);
	raw.SetSlotNum(m_slotNum);
	raw.SetRawStart(0); // immediately after the beacon;

	uint32_t page = 0;

	uint32_t rawinfo = (current_aid_end << 13) | (current_aid_start << 2) | page;

	raw.SetRawGroup(rawinfo); // (b0-b1, page index) (b2-b12, raw start AID) (b13-b23, raw end AID)

	// TODO set partial bitmap for TIM beacon for individual station indices

	m_rps.SetRawAssignment(raw);

	beacon.SetRPS(m_rps);

	AuthenticationCtrl AuthenCtrl;
	AuthenCtrl.SetControlType(false); //centralized
	Ptr<WifiMacQueue> MgtQueue = m_dca->GetQueue();
	uint32_t MgtQueueSize = MgtQueue->GetSize();
	if (MgtQueueSize < 10) {
		if (AuthenThreshold <= 950) {
			AuthenThreshold += 50;
		}
	} else {
		if (AuthenThreshold > 50) {
			AuthenThreshold -= 50;
		}
	}
	AuthenCtrl.SetThreshold(AuthenThreshold); //centralized
	beacon.SetAuthCtrl(AuthenCtrl);

	if (int(m_currentBeaconTIMGroup) == 0) {
		//DTIM header
		TIM tim;
		tim.SetDTIMPeriod(m_nrOfTIMGroups);
		tim.SetDTIMCount(0);

		// support up to 32 TIM groups
		uint32_t vmap = 0;

		// check the DCA queues if there is pending data
		for (int group = 0; group < m_nrOfTIMGroups; group++) {
			bool hasPendingData = false;
			for (uint32_t slot = 0; slot < m_slotNum; slot++) {
				if (rawSlotsDCA[group * m_slotNum + slot]->NeedsAccess()) {
					// pending data
					hasPendingData = true;
					break;
				}
			}

			// if there is no pending data in the DCA queues, check the panding data that is scheduled
			// to be forwarded to the next layer. Both systems are still required because otherwise
			// it's possible that data is sent directly through the DCA because it had RAW slot access
			// but the station wasn't listening because the DTIM didn't flag that

			if (hasPendingData) {
				vmap = vmap | (1 << group);
			}
		}

		for (auto& pair : macToAIDMap) {
			uint16_t aId = pair.second;

			if (pendingDataSizeForStations.at(aId - 1) > 0) {
				int group = strategy->GetTIMGroupFromAID(aId, m_rawGroupInterval);
				vmap = vmap | (1 << group);
			}
		}

		// determine if stations will be active
		for (auto& pair : macToAIDMap) {
			uint16_t aId = pair.second;
			int group = strategy->GetTIMGroupFromAID(aId, m_rawGroupInterval);

			if (((vmap >> group) & 0x01) == 0x01) {
				///std::cout << Simulator::Now().GetMicroSeconds() << " there is data for aId " << aId << "( " << " TIM group " << group << ")" << std::endl;
				staIsActiveDuringCurrentCycle[aId - 1] = true;
			} else
				staIsActiveDuringCurrentCycle[aId - 1] = false;
		}

		/*
		 std::cout << Simulator::Now().GetMicroSeconds() << " DTIM beacon send, VMAP: ";
		 for(int i = 31; i >= 0; i--)
		 std::cout << ((vmap >> i) & 0x01);
		 std::cout << std::endl;
*/

		tim.SetPartialVBitmap(vmap);

		beacon.SetTIM(tim);
	} else {
		TIM tim;
		tim.SetDTIMPeriod(m_nrOfTIMGroups);
		tim.SetDTIMCount(m_nrOfTIMGroups - m_currentBeaconTIMGroup);
		tim.SetPartialVBitmap(0); // no map
		beacon.SetTIM(tim);
	}

	packet->AddHeader(beacon);

	m_beaconDca->Queue(packet, hdr);

	m_transmitBeaconTrace(beacon, raw);

	current_aid_start += m_rawGroupInterval;
	current_aid_end += m_rawGroupInterval;
	if (current_aid_end > m_totalStaNum) {
		current_aid_start = 1;
		current_aid_end = m_rawGroupInterval;
	}

	// broadcast so disable rts, ack & fragmentation
	// sending the beacon and starting the RAW at the same time will always mismatch
	// a while due to the travel time of the beacon, try to compensate beacon travel time

	MacLowTransmissionParameters params;
	params.DisableRts();
	params.DisableAck();
	params.DisableNextData();
	Time txTime = m_low->CalculateOverallTxTime(packet, &hdr, params);
	NS_LOG_DEBUG(
			"Transmission of beacon will take " << txTime << ", delaying RAW start for that amount");
	Time bufferTimeToAllowBeaconToBeReceived = txTime;

	// schedule the slot start & ends
	Time slotDuration = strategy->GetSlotDuration(m_slotDurationCount);
	for (uint32_t i = 0; i < m_slotNum; i++) {


	Simulator::Schedule(
				bufferTimeToAllowBeaconToBeReceived + (slotDuration * i),
				&S1gApWifiMac::OnRAWSlotStart, this, m_currentBeaconTIMGroup,
				i);
	Simulator::Schedule(
				bufferTimeToAllowBeaconToBeReceived + (slotDuration * (i + 1)),
				&S1gApWifiMac::OnRAWSlotEnd, this, m_currentBeaconTIMGroup, i);
	}


	m_beaconEvent = Simulator::Schedule(m_beaconInterval,
			&S1gApWifiMac::SendOneBeacon, this);
}

void S1gApWifiMac::OnRAWSlotStart(uint8_t timGroup, uint8_t slot) {
	LOG_TRAFFIC(
			"AP RAW SLOT START FOR TIM GROUP " << std::to_string(timGroup) << " SLOT " << std::to_string(slot));
	m_currentTIMGroupSlot = slot;

	m_rawSlotStarted(timGroup, slot);

	rawSlotsDCA[timGroup * m_slotNum + slot]->AccessAllowedIfRaw(true);
	rawSlotsDCA[timGroup * m_slotNum + slot]->RawStart(
			strategy->GetSlotDuration(m_slotDurationCount));
}

void S1gApWifiMac::OnRAWSlotEnd(uint8_t timGroup, uint8_t slot) {
	LOG_TRAFFIC(
			"AP RAW SLOT END FOR TIM GROUP " << std::to_string(timGroup) << " SLOT " << std::to_string(slot));

	rawSlotsDCA[timGroup * m_slotNum + slot]->AccessAllowedIfRaw(false);
	rawSlotsDCA[timGroup * m_slotNum + slot]->OutsideRawStart();
}

void S1gApWifiMac::TxOk(const WifiMacHeader &hdr) {
	NS_LOG_FUNCTION(this);
	RegularWifiMac::TxOk(hdr);

	if (hdr.IsAssocResp()
			&& m_stationManager->IsWaitAssocTxOk(hdr.GetAddr1())) {
		NS_LOG_DEBUG("associated with sta=" << hdr.GetAddr1());
		m_stationManager->RecordGotAssocTxOk(hdr.GetAddr1());
	}
}

void S1gApWifiMac::TxFailed(const WifiMacHeader &hdr) {
	NS_LOG_FUNCTION(this);
	RegularWifiMac::TxFailed(hdr);

	if (hdr.IsAssocResp()
			&& m_stationManager->IsWaitAssocTxOk(hdr.GetAddr1())) {
		NS_LOG_DEBUG("assoc failed with sta=" << hdr.GetAddr1());
		m_stationManager->RecordGotAssocTxFailed(hdr.GetAddr1());
	}
}

void S1gApWifiMac::Receive(Ptr<Packet> packet, const WifiMacHeader *hdr) {
	NS_LOG_FUNCTION(this << packet << hdr);
	//uint16_t segg =  hdr->GetFrameControl (); // for test
	//NS_LOG_LOGIC ("AP waiting   " << segg); //for test
	Mac48Address from = hdr->GetAddr2();



	if (macToAIDMap.find(from) != macToAIDMap.end()) {
		// we've received data from the STA, which means it's active during its slot
		auto aId = macToAIDMap[from];
		staIsActiveDuringCurrentCycle[aId - 1] = true;
	}

	if (hdr->IsData()) {
		Mac48Address bssid = hdr->GetAddr1();
		if (!hdr->IsFromDs() && hdr->IsToDs() && bssid == GetAddress() && m_stationManager->IsAssociated(from)) {
			Mac48Address to = hdr->GetAddr3();
			if (to == GetAddress()) {
				NS_LOG_DEBUG("frame for me from=" << from);
				if (hdr->IsQosData()) {
					if (hdr->IsQosAmsdu()) {
						NS_LOG_DEBUG(
								"Received A-MSDU from=" << from << ", size=" << packet->GetSize());
						DeaggregateAmsduAndForward(packet, hdr);
						packet = 0;
					} else {
						std::cout << "forwarding frame from=" << from << ", to=" << to << " at " << Simulator::Now() << std::endl;
						ForwardUp(packet, from, bssid);
					}
				} else {
					ForwardUp(packet, from, bssid);//ami
				}
			} else if (to.IsGroup() || m_stationManager->IsAssociated(to)) {
				NS_LOG_DEBUG("forwarding frame from=" << from << ", to=" << to);
				Ptr<Packet> copy = packet->Copy();

				//If the frame we are forwarding is of type QoS Data,
				//then we need to preserve the UP in the QoS control
				//header...
				if (hdr->IsQosData()) {
					ForwardDown(packet, from, to, hdr->GetQosTid());
				} else {
					ForwardDown(packet, from, to);
				}
				ForwardUp(copy, from, to);
			} else {
				ForwardUp(packet, from, to);
			}
		} else if (hdr->IsFromDs() && hdr->IsToDs()) {
			//this is an AP-to-AP frame
			//we ignore for now.
			NotifyRxDrop(packet, DropReason::MacAPToAPFrame);
		} else {
			//we can ignore these frames since
			//they are not targeted at the AP
			NotifyRxDrop(packet, DropReason::MacNotForAP);
		}
		return;
	} else if (hdr->IsMgt()) {
		if (hdr->IsProbeReq()) {
			///NS_ASSERT(hdr->GetAddr1().IsBroadcast()); //ami

			NS_ASSERT(hdr->GetAddr1().IsMulticast6() || hdr->GetAddr1().IsBroadcast());
			SendProbeResp(from);
			return;
		} else if (hdr->GetAddr1() == GetAddress()) {
			if (hdr->IsAssocReq()) {
				if (m_stationManager->IsAssociated(from)) {
					return; //test, avoid repeate assoc
				}
				//NS_LOG_LOGIC ("Received AssocReq "); // for test
				//first, verify that the the station's supported
				//rate set is compatible with our Basic Rate set
				MgtAssocRequestHeader assocReq;
				packet->RemoveHeader(assocReq);
				SupportedRates rates = assocReq.GetSupportedRates();
				bool problem = false;
				for (uint32_t i = 0; i < m_stationManager->GetNBasicModes();
						i++) {
					WifiMode mode = m_stationManager->GetBasicMode(i);
					if (!rates.IsSupportedRate(mode.GetDataRate())) {
						problem = true;
						break;
					}
				}
				if (m_htSupported) {
					//check that the STA supports all MCSs in Basic MCS Set
					HtCapabilities htcapabilities =
							assocReq.GetHtCapabilities();
					for (uint32_t i = 0; i < m_stationManager->GetNBasicMcs();
							i++) {
						uint8_t mcs = m_stationManager->GetBasicMcs(i);
						if (!htcapabilities.IsSupportedMcs(mcs)) {
							problem = true;
							break;
						}
					}

				}
				if (problem) {
					//One of the Basic Rate set mode is not
					//supported by the station. So, we return an assoc
					//response with an error status.
					SendAssocResp(hdr->GetAddr2(), false);
				} else {
					//station supports all rates in Basic Rate Set.
					//record all its supported modes in its associated WifiRemoteStation
					for (uint32_t j = 0; j < m_phy->GetNModes(); j++) {
						WifiMode mode = m_phy->GetMode(j);
						if (rates.IsSupportedRate(mode.GetDataRate())) {
							m_stationManager->AddSupportedMode(from, mode);
						}
					}
					if (m_htSupported) {
						HtCapabilities htcapabilities =
								assocReq.GetHtCapabilities();
						m_stationManager->AddStationHtCapabilities(from,
								htcapabilities);
						for (uint32_t j = 0; j < m_phy->GetNMcs(); j++) {
							uint8_t mcs = m_phy->GetMcs(j);
							if (htcapabilities.IsSupportedMcs(mcs)) {
								m_stationManager->AddSupportedMcs(from, mcs);
							}
						}
					}
					m_stationManager->RecordWaitAssocTxOk(from);
					//send assoc response with success status.
					SendAssocResp(hdr->GetAddr2(), true);
				}
				return;
			} else if (hdr->IsDisassociation()) {
				m_stationManager->RecordDisassociated(from);
				return;
			}
		}
	}

	//Invoke the receive handler of our parent class to deal with any
	//other frames. Specifically, this will handle Block Ack-related
	//Management Action frames.
	RegularWifiMac::Receive(packet, hdr);
}

void S1gApWifiMac::DeaggregateAmsduAndForward(Ptr<Packet> aggregatedPacket,
		const WifiMacHeader *hdr) {
	NS_LOG_FUNCTION(this << aggregatedPacket << hdr);
	MsduAggregator::DeaggregatedMsdus packets = MsduAggregator::Deaggregate(
			aggregatedPacket);

	for (MsduAggregator::DeaggregatedMsdusCI i = packets.begin();
			i != packets.end(); ++i) {
		if ((*i).second.GetDestinationAddr() == GetAddress()) {
			ForwardUp((*i).first, (*i).second.GetSourceAddr(),
					(*i).second.GetDestinationAddr());
		} else {
			Mac48Address from = (*i).second.GetSourceAddr();
			Mac48Address to = (*i).second.GetDestinationAddr();
			NS_LOG_DEBUG("forwarding QoS frame from=" << from << ", to=" << to);
			ForwardDown((*i).first, from, to, hdr->GetQosTid());
		}
	}
}

void S1gApWifiMac::DoInitialize(void) {
	NS_LOG_FUNCTION(this);
	m_beaconDca->Initialize();
	m_beaconEvent.Cancel();
	if (m_enableBeaconGeneration) {
		if (m_enableBeaconJitter) {
			int64_t jitter = m_beaconJitter->GetValue(0,
					m_beaconInterval.GetMicroSeconds());
			NS_LOG_DEBUG(
					"Scheduling initial beacon for access point " << GetAddress() << " at time " << jitter << " microseconds");
			m_beaconEvent = Simulator::Schedule(MicroSeconds(jitter),
					&S1gApWifiMac::SendOneBeacon, this);
		} else {
			NS_LOG_DEBUG(
					"Scheduling initial beacon for access point " << GetAddress() << " at time 0");
			m_beaconEvent = Simulator::ScheduleNow(&S1gApWifiMac::SendOneBeacon,
					this);
		}
	}

	// TODO subclass multiple strategies
	strategy = new S1gStrategy();

	// also apply the correct backoff slot durations, same as for STA wifi mac.
	auto rawSlotDuration =
			strategy->GetSlotDuration(m_slotDurationCount).GetMicroSeconds();

	// CWMax is 1023 so max backoff slot duration has to be RAWslotduration / 1023
	uint16_t backoffSlotDuration = rawSlotDuration / 1023;

	SetSifs(MicroSeconds(160));
	SetSlot(MicroSeconds(backoffSlotDuration));
	SetEifsNoDifs(MicroSeconds(160 + 1120));
	SetPifs(MicroSeconds(160 + backoffSlotDuration));
	SetCtsTimeout(
			MicroSeconds(
					160 + 1120 + backoffSlotDuration
							+ GetDefaultMaxPropagationDelay().GetMicroSeconds()
									* 2));	//
	SetAckTimeout(
			MicroSeconds(
					160 + 1120 + backoffSlotDuration
							+ GetDefaultMaxPropagationDelay().GetMicroSeconds()
									* 2));	//
	SetBasicBlockAckTimeout(
			GetSifs() + GetSlot() + GetDefaultBasicBlockAckDelay()
					+ GetDefaultMaxPropagationDelay() * 2);
	SetCompressedBlockAckTimeout(
			GetSifs() + GetSlot() + GetDefaultCompressedBlockAckDelay()
					+ GetDefaultMaxPropagationDelay() * 2);

	m_nrOfTIMGroups = ceil(m_totalStaNum / (float) m_rawGroupInterval);
	// initialize queue
	pendingDataSizeForStations = std::vector<int>(m_totalStaNum, 0);
	staIsActiveDuringCurrentCycle = std::vector<bool>(m_totalStaNum, false);

	current_aid_start = 1;
	current_aid_end = m_rawGroupInterval;

	rawSlotsDCA = std::vector<Ptr<DcaTxop>>();
	for (uint32_t i = 0; i < (m_nrOfTIMGroups * m_slotNum); i++) {

		Ptr<DcaTxop> dca = CreateObject<DcaTxop>();
		dca->SetLow(m_low);
		dca->SetManager(m_dcfManager);
		dca->SetTxMiddle(m_txMiddle);
		dca->SetTxOkCallback(MakeCallback(&S1gApWifiMac::TxOk, this));
		dca->SetTxFailedCallback(MakeCallback(&S1gApWifiMac::TxFailed, this));

		dca->SetWifiRemoteStationManager(m_stationManager);

		dca->GetQueue()->TraceConnect("PacketDropped", "",
				MakeCallback(&S1gApWifiMac::OnQueuePacketDropped, this));

		dca->TraceConnect("Collision", "",
				MakeCallback(&S1gApWifiMac::OnCollision, this));

		// ensure queues don't expire too fast
		dca->GetQueue()->SetMaxDelay(m_maxTimeInQueue);
		dca->Initialize();

		ConfigureDcf(dca, 15, 1023, AC_BE_NQOS);

		rawSlotsDCA.push_back(dca);
	}

	RegularWifiMac::DoInitialize();
}

} //namespace ns3

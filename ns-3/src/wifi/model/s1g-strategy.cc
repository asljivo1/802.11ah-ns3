/*
 * S1gStrategy.cc
 *
 *  Created on: Jul 11, 2016
 *      Author: dwight
 */

#include "s1g-strategy.h"

namespace ns3 {

S1gStrategy::S1gStrategy() {
}

S1gStrategy::~S1gStrategy() {

}

uint16_t S1gStrategy::GetAIDFromMacAddress(Mac48Address to) {
	uint8_t mac[6];
	to.CopyTo(mac);
	uint8_t aid_l = mac[5];
	uint8_t aid_h = mac[4] & 0x1f;
	uint16_t aid = (aid_h << 8) | (aid_l << 0); //assign mac address as AID
	return aid;
}

uint16_t S1gStrategy::GetTIMGroupFromAID(uint16_t aId,
		uint32_t rawGroupInterval) {
	return (aId - 1) / rawGroupInterval;
}

uint8_t S1gStrategy::GetSlotIndexFromAID(uint16_t aId, uint8_t nrOfSlots) {
	return aId % nrOfSlots;
}

Time S1gStrategy::GetSlotDuration(uint16_t slotDurationCount) {
	return MicroSeconds(500 + 120 * slotDurationCount);
}

bool S1gStrategy::STABelongsToRAWGroup(uint16_t aid, RPS& rps) {

	auto rawObj = rps.GetRawAssigmentObj();
	auto pageindex = rawObj.GetRawGroupPage();

	if (pageindex == ((aid >> 11) & 0x0003)) //in the page indexed
	{
		if (aid >= rawObj.GetRawGroupAIDStart()
				&& aid <= rawObj.GetRawGroupAIDEnd()) {

			return true;
		}
	}
	return false;
}

Time S1gStrategy::GetEarlyWakeTime() {
	return MilliSeconds(10);
}

} /* namespace ns3 */

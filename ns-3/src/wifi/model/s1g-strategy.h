/*
 * S1gStrategy.h
 *
 *  Created on: Jul 11, 2016
 *      Author: dwight
 */

#ifndef SRC_WIFI_MODEL_S1G_STRATEGY_H_
#define SRC_WIFI_MODEL_S1G_STRATEGY_H_


#include "regular-wifi-mac.h"
#include "ht-capabilities.h"
#include "amsdu-subframe-header.h"
#include "supported-rates.h"
#include "ns3/random-variable-stream.h"
#include "extension-headers.h"

namespace ns3 {

class S1gStrategy {
public:
	S1gStrategy();
	virtual ~S1gStrategy();

	virtual uint16_t GetAIDFromMacAddress(Mac48Address mac);

	virtual uint16_t GetTIMGroupFromAID(uint16_t aid, uint32_t rawGroupInterval);

	virtual uint8_t GetSlotIndexFromAID(uint16_t aId, uint8_t nrOfSlots);

	virtual Time GetSlotDuration(uint16_t slotDurationCount);

	virtual bool STABelongsToRAWGroup(uint16_t aid, RPS& rps);

	/**
	 * Time to wake early for events such as beacons etc
	 * This is time needed for the radio to go from sleeping to on and actively receive data
	 */
	virtual Time GetEarlyWakeTime();
};

} /* namespace ns3 */

#endif /* SRC_WIFI_MODEL_S1G_STRATEGY_H_ */


#ifndef S1G_AP_QUEUE_H
#define S1G_AP_QUEUE_H

#include <list>
#include <utility>
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "wifi-mac-header.h"

namespace ns3 {
class QosBlockedDestinations;


class S1gAPQueue : public Object
{
public:
  static TypeId GetTypeId (void);
  S1gAPQueue ();
  ~S1gAPQueue ();

  /**
   * Set the maximum queue size.
   *
   * \param maxSize the maximum queue size
   */
  void SetMaxSize (uint32_t maxSize);
  /**
   * Set the maximum delay before the packet is discarded.
   *
   * \param delay the maximum delay
   */
  void SetMaxDelay (Time delay);
  /**
   * Return the maximum queue size.
   *
   * \return the maximum queue size
   */
  uint32_t GetMaxSize (void) const;
  /**
   * Return the maximum delay before the packet is discarded.
   *
   * \return the maximum delay
   */
  Time GetMaxDelay (void) const;

  /**
   * Enqueue the given packet and its corresponding WifiMacHeader at the <i>end</i> of the queue.
   *
   * \param packet the packet to be euqueued at the end
   * \param hdr the header of the given packet
   */
  void Enqueue (Ptr<const Packet> packet, const WifiMacHeader &hdr);
  /**
   * Enqueue the given packet and its corresponding WifiMacHeader at the <i>front</i> of the queue.
   *
   * \param packet the packet to be euqueued at the end
   * \param hdr the header of the given packet
   */
  void PushFront (Ptr<const Packet> packet, const WifiMacHeader &hdr);
  /**
   * Dequeue the packet in the front of the queue.
   *
   * \param hdr the WifiMacHeader of the packet
   *
   * \return the packet
   */
  Ptr<const Packet> Dequeue (WifiMacHeader *hdr);
  /**
   * Peek the packet in the front of the queue. The packet is not removed.
   *
   * \param hdr the WifiMacHeader of the packet
   *
   * \return the packet
   */
  Ptr<const Packet> Peek (WifiMacHeader *hdr);
  /**
   * Searchs and returns, if is present in this queue, first packet having
   * address indicated by <i>type</i> equals to <i>addr</i>, and tid
   * equals to <i>tid</i>. This method removes the packet from this queue.
   * Is typically used by ns3::EdcaTxopN in order to perform correct MSDU
   * aggregation (A-MSDU).
   *
   * \param hdr the header of the dequeued packet
   * \param tid the given TID
   * \param type the given address type
   * \param addr the given destination
   *
   * \return packet
   */
  Ptr<const Packet> DequeueByTidAndAddress (WifiMacHeader *hdr,
                                            uint8_t tid,
                                            WifiMacHeader::AddressType type,
                                            Mac48Address addr);
  /**
   * Searchs and returns, if is present in this queue, first packet having
   * address indicated by <i>type</i> equals to <i>addr</i>, and tid
   * equals to <i>tid</i>. This method doesn't remove the packet from this queue.
   * Is typically used by ns3::EdcaTxopN in order to perform correct MSDU
   * aggregation (A-MSDU).
   *
   * \param hdr the header of the dequeued packet
   * \param tid the given TID
   * \param type the given address type
   * \param addr the given destination
   * \param timestamp
   *
   * \return packet
   */
  Ptr<const Packet> PeekByTidAndAddress (WifiMacHeader *hdr,
                                         uint8_t tid,
                                         WifiMacHeader::AddressType type,
                                         Mac48Address addr,
                                         Time *timestamp);
  /**
   * If exists, removes <i>packet</i> from queue and returns true. Otherwise it
   * takes no effects and return false. Deletion of the packet is
   * performed in linear time (O(n)).
   *
   * \param packet the packet to be removed
   *
   * \return true if the packet was removed, false otherwise
   */
  bool Remove (Ptr<const Packet> packet);
  /**
   * Returns number of QoS packets having tid equals to <i>tid</i> and address
   * specified by <i>type</i> equals to <i>addr</i>.
   *
   * \param tid the given TID
   * \param type the given address type
   * \param addr the given destination
   *
   * \return the number of QoS packets
   */
  uint32_t GetNPacketsByTidAndAddress (uint8_t tid,
                                       WifiMacHeader::AddressType type,
                                       Mac48Address addr);
  /**
   * Returns first available packet for transmission. A packet could be no available
   * if it's a QoS packet with a tid and an address1 fields equal to <i>tid</i> and <i>addr</i>
   * respectively that index a pending agreement in the BlockAckManager object.
   * So that packet must not be transmitted until reception of an ADDBA response frame from station
   * addressed by <i>addr</i>. This method removes the packet from queue.
   *
   * \param hdr the header of the dequeued packet
   * \param tStamp
   * \param blockedPackets
   *
   * \return packet
   */
  Ptr<const Packet> DequeueFirstAvailable (WifiMacHeader *hdr,
                                           Time &tStamp,
                                           const QosBlockedDestinations *blockedPackets);
  /**
   * Returns first available packet for transmission. The packet isn't removed from queue.
   *
   * \param hdr the header of the dequeued packet
   * \param tStamp
   * \param blockedPackets
   *
   * \return packet
   */
  Ptr<const Packet> PeekFirstAvailable (WifiMacHeader *hdr,
                                        Time &tStamp,
                                        const QosBlockedDestinations *blockedPackets);
  /**
   * Flush the queue.
   */
  void Flush (void);

  /**
   * Return if the queue is empty.
   *
   * \return true if the queue is empty, false otherwise
   */
  bool IsEmpty (void);
  /**
   * Return the current queue size.
   *
   * \return the current queue size
   */
  uint32_t GetSize (void);


protected:
  /**
   * Clean up the queue by removing packets that exceeded the maximum delay.
   */
  virtual void Cleanup (void);

  /**
   * A struct that holds information about a packet for putting
   * in a packet queue.
   */
  struct Item
  {
    /**
     * Create a struct with the given parameters.
     *
     * \param packet
     * \param hdr
     * \param tstamp
     */
    Item (Ptr<const Packet> packet,
          const WifiMacHeader &hdr,
          Time tstamp);
    Ptr<const Packet> packet; //!< Actual packet
    WifiMacHeader hdr;        //!< Wifi MAC header associated with the packet
    Time tstamp;              //!< timestamp when the packet arrived at the queue
  };

  /**
   * typedef for packet (struct Item) queue.
   */
  typedef std::list<struct Item> PacketQueue;
  /**
   * typedef for packet (struct Item) queue reverse iterator.
   */
  typedef std::list<struct Item>::reverse_iterator PacketQueueRI;
  /**
   * typedef for packet (struct Item) queue iterator.
   */
  typedef std::list<struct Item>::iterator PacketQueueI;
  /**
   * Return the appropriate address for the given packet (given by PacketQueue iterator).
   *
   * \param type
   * \param it
   *
   * \return the address
   */
  Mac48Address GetAddressForPacket (enum WifiMacHeader::AddressType type, PacketQueueI it);

  PacketQueue m_queue; //!< Packet (struct Item) queue
  uint32_t m_size;     //!< Current queue size
  uint32_t m_maxSize;  //!< Queue capacity
  Time m_maxDelay;     //!< Time to live for packets in the queue
};

} //namespace ns3

#endif /* S1G_AP_QUEUE_H */

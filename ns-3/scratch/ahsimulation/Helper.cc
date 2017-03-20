#include "Helper.h"

using namespace ns3;
using namespace std;

void PopulateArpCache() {
    Ptr<ArpCache> arp = CreateObject<ArpCache> ();
    arp->SetAliveTimeout(Seconds(3600 * 24 * 365));
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
        NS_ASSERT(ip != 0);
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++) {
            Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
            NS_ASSERT(ipIface != 0);
            Ptr<NetDevice> device = ipIface->GetDevice();
            NS_ASSERT(device != 0);
            Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress());
            for (uint32_t k = 0; k < ipIface->GetNAddresses(); k++) {
                Ipv4Address ipAddr = ipIface->GetAddress(k).GetLocal();
                if (ipAddr == Ipv4Address::GetLoopback())
                    continue;
                ArpCache::Entry * entry = arp->Add(ipAddr);
                entry->MarkWaitReply(0);
                entry->MarkAlive(addr);
                std::cout << "Arp Cache: Adding the pair (" << addr << "," << ipAddr << ")" << std::endl;
            }
        }
    }
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
        NS_ASSERT(ip != 0);
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++) {
            Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
            ipIface->SetAttribute("ArpCache", PointerValue(arp));
        }
    }
}


void PopulateNdiscCache(bool print) {

	Ptr<NdiscCache> ndisc (CreateObject<NdiscCache> ());
	for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
	{
		Ptr<Ipv6L3Protocol> ipv6 ((*i)->GetObject<Ipv6L3Protocol> ());
		Ptr<Icmpv6L4Protocol> icmpv6 (ipv6->GetIcmpv6());
		NS_ASSERT(icmpv6);

		ObjectVectorValue interfaces;
		ipv6->GetAttribute("InterfaceList", interfaces);
		for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++)
		{

			Ptr<Ipv6Interface> ipv6Iface = (j->second)->GetObject<Ipv6Interface> ();
			NS_ASSERT(ipv6Iface != 0);
			Ptr<NetDevice> device (ipv6Iface->GetDevice());
			NS_ASSERT(device != 0);
			Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress());

			Ipv6Address ipv6Addr = ipv6Iface->GetAddress(0).GetAddress();
			ndisc->SetDevice(device, ipv6Iface);
			if (ipv6Addr == Ipv6Address::GetLoopback())
				continue;
			NdiscCache::Entry* entry  (ndisc->Add(ipv6Addr));
			entry->SetIpv6Address(ipv6Addr);
			entry->SetMacAddress(addr);
			entry->MarkReachable();
			std::cout << "Ndisc Cache: Adding the pair (" << addr << "," << ipv6Addr << ")" << std::endl;
			ipv6Iface->SetNdiscCache(ndisc);

		}
	}

	if (print){
		Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> (&std::cout);
		std::ostream* os = stream->GetStream ();
		*os << "NDISC Cache of node ";
		for  (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
			Ptr<Ipv6L3Protocol> ipv6 = (*i)->GetObject<Ipv6L3Protocol> ();
			std::string found = Names::FindName (*i);
			if (Names::FindName (*i) != "")
			{
				*os << found;
			}
			else
			{
				*os << static_cast<int> ((*i)->GetId ());
			}

			*os << " at time " << Simulator::Now ().GetSeconds () << "\n";

			for (uint32_t k=0; k<(ipv6->GetNInterfaces()); k++)
			{

				Ptr<NdiscCache> ndiscCache = ipv6->GetInterface (k)->GetNdiscCache ();
				if (ndiscCache)
				{
					ndiscCache->PrintNdiscCache (stream);
				}
			}
		}
	}
}

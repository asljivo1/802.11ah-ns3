/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//       n0    n1
//       |     |
//       =======
//         LAN
//
// - UDP flows from n0 to n1

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"

#include "ns3/address-utils.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CoapExample");

int
main (int argc, char *argv[])
{
//
// Enable logging
//
	if (true){
		LogComponentEnable ("CoapClient", LOG_LEVEL_INFO);
		LogComponentEnable ("CoapServer", LOG_LEVEL_INFO);
		LogComponentEnable ("CoapPdu", LOG_LEVEL_INFO);
		/*LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
			LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);

			LogComponentEnable("WifiNetDevice", LOG_LEVEL_ALL);
			LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);*/
	}


  bool useV6 = false;
  Address serverAddress;

  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);

  NS_LOG_INFO ("Create channels.");
//
// Explicitly create the channels required by the topology (shown above).
//
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));
  NetDeviceContainer d = csma.Install (n);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  if (!useV6)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer i = ipv4.Assign (d);
      serverAddress = Address (i.GetAddress (1));
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:0000:f00d:cafe::", Ipv6Prefix (64));
      Ipv6InterfaceContainer i6 = ipv6.Assign (d);
      serverAddress = Address(i6.GetAddress (1,0));
      //std::cout << "GetLinkLocalAddress(0) " << i6.GetLinkLocalAddress(0) << std::endl;
      std::cout << "GetAddress(1,0) " << i6.GetAddress(1,0) << std::endl;
      //std::cout << "GetLinkLocalAddress(i6.GetAddress(0,1)) " << i6.GetLinkLocalAddress(i6.GetAddress(0,0)) << std::endl;
      n.Get(0)->GetObject<Icmpv6L4Protocol> ()->SetAttribute ("DAD", BooleanValue (false));
      n.Get(1)->GetObject<Icmpv6L4Protocol> ()->SetAttribute ("DAD", BooleanValue (false));
    }

  NS_LOG_INFO ("Create Applications.");
//
// Create one udpServer applications on node one.
//
  uint16_t port = 5683;
  CoapServerHelper server (port);
  NodeContainer sn (n.Get(1));
  ApplicationContainer apps = server.Install(sn);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.1); //0.05
  uint32_t maxPacketCount = 10;
  CoapClientHelper client (serverAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
  NodeContainer cn (n.Get(0));
  apps = client.Install (cn);

  /*Ptr<Ipv4> ipv4 = cn.Get(0)->GetObject<Ipv4>();
  Ipv4InterfaceAddress iAddr = ipv4->GetAddress(1,0);
  Ipv4Address ipAddr = iAddr.GetLocal();
  std::cout << "Adresa je " << ipAddr << std::endl;*/

  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("coap-test.tr"));
  csma.EnablePcapAll ("coap-test", false);

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

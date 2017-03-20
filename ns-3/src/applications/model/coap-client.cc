/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
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
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/network-module.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"

#include "ns3/coap-client.h"

#include "ns3/output-stream-wrapper.h"
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>
#include <climits>
#include "arpa/inet.h"
#include "seq-ts-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CoapClient");

NS_OBJECT_ENSURE_REGISTERED (CoapClient);

TypeId
CoapClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CoapClient")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<CoapClient> ()
    .AddAttribute ("MaxPackets",
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&CoapClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval",
                   "The time to wait between packets", TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&CoapClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&CoapClient::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", "The destination port of the outbound packets",
                   UintegerValue (COAP_DEFAULT_PORT),
                   MakeUintegerAccessor (&CoapClient::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("PacketSize", "Size of echo data in outbound packets",
				   UintegerValue (436),
				   MakeUintegerAccessor (&CoapClient::m_size),
				   MakeUintegerChecker<uint32_t> (4,COAP_MAX_PDU_SIZE))
	.AddAttribute ("IntervalDeviation",
					"The possible deviation from the interval to send packets",
					TimeValue (Seconds (0)),
					MakeTimeAccessor (&CoapClient::m_intervalDeviation),
					MakeTimeChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&CoapClient::m_packetSent),
                     "ns3::Packet::TracedCallback")
	.AddTraceSource("Rx","Response from CoAP server received",
					MakeTraceSourceAccessor(&CoapClient::m_packetReceived),
					"ns3::CoapClient::PacketReceivedCallback");
  ;
  return tid;
}

CoapClient::CoapClient ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();

  m_rv = CreateObject<UniformRandomVariable> ();
  m_coapCtx = NULL;
  m_peerPort = COAP_DEFAULT_PORT;
  //m_payload = { 0, NULL };
  //m_optList = NULL;
}

CoapClient::~CoapClient ()
{
  NS_LOG_FUNCTION (this);

  /*delete [] m_data;
  m_data = 0;
  m_dataSize = 0;*/
  /*if (m_coapCtx)
	  coap_free_context(m_coapCtx);*/
}

void
CoapClient::SetRemote (Ipv4Address ip, uint16_t port = COAP_DEFAULT_PORT)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address(ip);
  m_peerPort = port;
}

void
CoapClient::SetRemote (Ipv6Address ip, uint16_t port = COAP_DEFAULT_PORT)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = Address(ip);
  m_peerPort = port;
}

void
CoapClient::SetRemote (Address ip, uint16_t port = COAP_DEFAULT_PORT)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

coap_context_t* CoapClient::GetContext(void)
{
	  NS_LOG_FUNCTION (this);
	  return m_coapCtx;
}

bool CoapClient::PrepareContext(void)
{
	  // Prepare dummy source address for libcoap
	  coap_address_t srcAddr;
	  coap_address_init(&srcAddr);

	  if (Ipv4Address::IsMatchingType (m_peerAddress))
	  {
		  Ptr<Ipv4> ip = GetNode()->GetObject<Ipv4>();
		  Ipv4InterfaceAddress iAddr = ip->GetAddress(1,0);
		  std::stringstream addressStringStream;
		  addressStringStream << Ipv4Address::ConvertFrom (iAddr.GetLocal());

		  srcAddr.addr.sin.sin_family      = AF_INET;
		  srcAddr.addr.sin.sin_port        = htons(0);
		  //std::cout << "************src ip4Addr " << addressStringStream.str().c_str() << std::endl;
		  srcAddr.addr.sin.sin_addr.s_addr = inet_addr("0.0.0.0"); //"0.0.0.0"
		  m_coapCtx = coap_new_context(&srcAddr);


	  }
	  else if (Ipv6Address::IsMatchingType (m_peerAddress))
	  {
		  srcAddr.addr.sin.sin_family      = AF_INET6;
		  srcAddr.addr.sin.sin_port        = htons(0);
		  srcAddr.addr.sin.sin_addr.s_addr = inet_addr("::");
		  m_coapCtx = coap_new_context(&srcAddr);
	  }

	  m_coapCtx->network_send = CoapClient::coap_network_send;

	  m_coapCtx->ns3_coap_client_obj_ptr = this;
	  //m_coapCtx->network_read = CoapClient::coap_network_read;
	  if (m_coapCtx) return true;
	  else return false;
}

void
CoapClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_rv = 0;
  Application::DoDispose ();
}

void
CoapClient::StartApplication (void)
{
	NS_LOG_FUNCTION (this);

	if (m_socket == 0)
	{
		TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
		m_socket = Socket::CreateSocket (GetNode (), tid);
		if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
		{
			m_socket->Bind ();
			m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
		}
		else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
		{
			m_socket->Bind6 ();
			m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
		}
	}

	// Prepare context for IPv4 and IPv6
	if (!m_coapCtx){
		if (PrepareContext()) {
			NS_LOG_INFO("CoapClient::Coap context ready.");

		} else{
			NS_LOG_WARN("CoapClient::No context available for the interface. Abort.");
			return;
		}
	}

	// BLOCK2 pertrains to the response payload
	coap_register_option(m_coapCtx, COAP_OPTION_BLOCK2);
	m_socket->SetRecvCallback (MakeCallback (&CoapClient::HandleRead, this));
	PrepareMsg();
}

void
CoapClient::PrepareMsg (void)
{
	std::string ipv4AdrString(""), ipv6AdrString("");

	// Prepare URI
	std::string serverUri("coap://");

	std::stringstream peerAddressStringStream;
	if (Ipv4Address::IsMatchingType (m_peerAddress)){
		peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);
		ipv4AdrString = peerAddressStringStream.str ();
		serverUri.append(ipv4AdrString);
	}
	else if (Ipv6Address::IsMatchingType (m_peerAddress)){
		peerAddressStringStream << Ipv6Address::ConvertFrom (m_peerAddress);
		serverUri.append("[");
		ipv6AdrString = peerAddressStringStream.str ();
		serverUri.append(ipv6AdrString);
		serverUri.append("]");
	}

	//serverUri.append("");
	serverUri.append("/hello");
	static coap_uri_t uri;
	int res = coap_split_uri((const unsigned char*)serverUri.c_str(), strlen(serverUri.c_str()), &uri);
	if (res < 0){
		NS_LOG_ERROR("Invalid CoAP URI. Abort.");
		return;
	}
	// Prepare request

	m_coapMsg.SetType(COAP_MESSAGE_CON);
	m_coapMsg.SetCode(COAP_REQUEST_GET);
	m_coapMsg.SetId(coap_new_message_id(m_coapCtx));

	//make a string of zerofilled data for payload of size m_size
	std::string payload(448-2-12-4, '0');
	/*std::cout << "velicina m_size je " << m_size << std::endl;
	std::cout << "velicina payload je " << payload.length() << std::endl;
	std::cout << "velicina payload.c_str() je " << strlen(payload.c_str()) << std::endl;*/


	m_coapMsg.SetSize(sizeof(coap_hdr_t) + uri.path.length + 1 + strlen(payload.c_str()) + 1); //allocate space for the options (uri) and payload +1 ?
	//m_coapMsg.SetSize(m_size);
	m_coapMsg.AddOption(COAP_OPTION_URI_PATH, uri.path.length, uri.path.s); //try with host as well - see the difference?

	coap_add_data(m_coapMsg.GetPdu(), strlen(payload.c_str()), (const unsigned char *)payload.c_str());
	//m_coapMsg.AddData(m_coapMsg.GetSerializedSize() - sizeof(coap_hdr_t) - uri.path.length);
	std::cout << "velicina pdu je " << m_coapMsg.GetSerializedSize() << std::endl;

	coap_address_init(&m_dst_addr);
	// prepare destination for libcoap ipv4
	m_dst_addr.addr.sin.sin_family = AF_INET;
	m_dst_addr.addr.sin.sin_port = htons(m_peerPort);

	if (ipv4AdrString != "")
		m_dst_addr.addr.sin.sin_addr.s_addr = inet_addr(ipv4AdrString.c_str());
	else
		m_dst_addr.addr.sin.sin_addr.s_addr = inet_addr(ipv6AdrString.c_str());

	if (m_coapMsg.GetType() == COAP_MESSAGE_CON)
	{
		m_tid = coap_send_confirmed(m_coapCtx, m_coapCtx->endpoint, &m_dst_addr, m_coapMsg.GetPdu());
	}
	else {
		m_tid = coap_send(m_coapCtx, m_coapCtx->endpoint, &m_dst_addr, m_coapMsg.GetPdu());
	}
	if (m_coapMsg.GetPdu()->hdr->type != COAP_MESSAGE_CON || m_tid == COAP_INVALID_TID)
	    coap_delete_pdu(m_coapMsg.GetPdu());
}

void
CoapClient::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

ssize_t
CoapClient::Send (uint8_t *data, size_t datalen)
{
	NS_LOG_FUNCTION (this);
	NS_ASSERT (m_sendEvent.IsExpired ());

	Ptr<Packet> p = Create<Packet> (data, datalen);
	// size of packet shoud be m_size. Later make this happen: empty payload of size m_size-hdr-options

	// add sequence header to the packet
	SeqTsHeader seqTs;
	seqTs.SetSeq (m_sent);
	p->AddHeader (seqTs);

	std::stringstream peerAddressStringStream;
	if (Ipv4Address::IsMatchingType (m_peerAddress))
		peerAddressStringStream << Ipv4Address::ConvertFrom (m_peerAddress);
	else if (Ipv6Address::IsMatchingType (m_peerAddress))
		peerAddressStringStream << Ipv6Address::ConvertFrom (m_peerAddress);

	// call to the trace sinks before the packet is actually sent,
	// so that tags added to the packet can be sent as well
	m_packetSent(p);

	ssize_t retval;
	if (m_sent < m_count)
	{
		if ((m_socket->Send (p)) >= 0)
		{
			++m_sent;

			NS_LOG_INFO ("At time "<< (Simulator::Now ()).GetSeconds ()<< " client sent " << p->GetSize() << " bytes to "
					<< peerAddressStringStream.str () << " Uid: "
					<< p->GetUid ());
			retval = p->GetSize();
		}
		else
		{
			NS_LOG_INFO ("Error while sending " << m_size << " bytes to "
					<< peerAddressStringStream.str ());
			retval = -1;
		}
	}
	if (m_sent < m_count)
	{
		double deviation = m_rv->GetValue(-m_intervalDeviation.GetMicroSeconds(), m_intervalDeviation.GetMicroSeconds());
		m_sendEvent = Simulator::Schedule (m_interval + MicroSeconds(deviation), &CoapClient::PrepareMsg, this);
	}
	return retval;
}

ssize_t
CoapClient::coap_network_send(struct coap_context_t *context UNUSED_PARAM,
		  const coap_endpoint_t *local_interface,
		  const coap_address_t *dst,
		  unsigned char *data,
		  size_t datalen)
{
	unused(local_interface);
	unused(dst);
	unused(data);
	unused(datalen);
	CoapClient* clientPtr = static_cast<CoapClient*>(context->ns3_coap_client_obj_ptr);

	return (*clientPtr).Send(data, datalen);
}

ssize_t CoapClient::CoapHandleMessage(Address from, Ptr<Packet> packet){ //coap_network_read extracts coap_packet_t - this is coap_handle_message
	uint32_t msg_len (packet->GetSize());
	ssize_t bytesRead(0);
	uint8_t* msg = new uint8_t[msg_len];
	bytesRead = packet->CopyData (msg, msg_len);

	if (msg_len < sizeof(coap_hdr_t)) {
		NS_LOG_ERROR(this << "Discarded invalid frame CoAP" );
		return 0;
	}

	// check version identifier
	if (((*msg >> 6) & 0x03) != COAP_DEFAULT_VERSION) {
		NS_LOG_ERROR(this << "Unknown CoAP protocol version " << ((*msg >> 6) & 0x03));
		return 0;
	}

	coap_queue_t * node = coap_new_node();
	if (!node) {
		return 0;
	}

	node->pdu = coap_pdu_init(0, 0, 0, msg_len);
	if (!node->pdu) {
		coap_delete_node(node);
		return 0;
	}

	NS_ASSERT(node->pdu);
	coap_pdu_t* sent (m_coapMsg.GetPdu());
	coap_pdu_t* received (node->pdu);
	if (coap_pdu_parse(msg, msg_len, node->pdu)){
		coap_show_pdu(node->pdu);
		if (COAP_RESPONSE_CLASS(node->pdu->hdr->code) == 2)	{
			//set obs timer if we have successfully subscribed a resource
			/*if (sent && coap_check_option(received, COAP_OPTION_SUBSCRIPTION, &opt_iter)) {
				NS_LOG_DEBUG("Observation relationship established, set timeout to" << m_obsSeconds);
				//set_timeout(&obs_wait, obs_seconds);
			}*/
		}
		else if(COAP_RESPONSE_CLASS(node->pdu->hdr->code) > 2 && COAP_RESPONSE_CLASS(node->pdu->hdr->code) <= 5)
		{
			// Error response received. Drop response.
			std::cout << "---------------------------- "  << std::endl;
			coap_delete_pdu(node->pdu);
			node->pdu = NULL;
			return bytesRead;
		}
	}
	else {
		NS_LOG_ERROR("Coap client cannot parse CoAP packet. Abort.");
		return 0;
	}

	coap_transaction_id(from, node->pdu, &node->id);
	std::cout << "---------------------------- "  << std::endl;

	// drop if this was just some message, or send reset in case of notification - TODO!

	if (!sent && (received->hdr->type == COAP_MESSAGE_CON ||
			received->hdr->type == COAP_MESSAGE_NON))
		coap_send_rst(m_coapCtx, m_coapCtx->endpoint, &m_dst_addr, node->pdu);
	//return 0;
	if (received->hdr->type == COAP_MESSAGE_RST) {
		NS_LOG_INFO("Coap client got RST.");
		return 0;
	}
	return bytesRead;
}

void
CoapClient::coap_transaction_id(Address from, const coap_pdu_t *pdu, coap_tid_t *id) {
	coap_key_t h;

	memset(h, 0, sizeof(coap_key_t));

	if (InetSocketAddress::IsMatchingType(from)) {
		std::stringstream addressStringStream;
		addressStringStream << InetSocketAddress::ConvertFrom (from).GetPort();
		std::string port (addressStringStream.str());
		coap_hash((const unsigned char *)port.c_str(), sizeof(port.c_str()), h);
		addressStringStream.str("");
		addressStringStream << InetSocketAddress::ConvertFrom (from).GetIpv4 ();
		std::string ipv4 (addressStringStream.str());
		coap_hash((const unsigned char *)ipv4.c_str(), sizeof(ipv4.c_str()), h);
	}
	else if (Inet6SocketAddress::IsMatchingType(from)) {
		std::stringstream addressStringStream;
		addressStringStream << Inet6SocketAddress::ConvertFrom (from).GetPort();
		std::string port (addressStringStream.str());
		coap_hash((const unsigned char *)port.c_str(), sizeof(port.c_str()), h);
		addressStringStream.str("");
		addressStringStream << Inet6SocketAddress::ConvertFrom (from).GetIpv6();
		std::string ipv6 (addressStringStream.str());
		coap_hash((const unsigned char *)ipv6.c_str(), sizeof(ipv6.c_str()), h);
	}
	else return;

	coap_hash((const unsigned char *)&pdu->hdr->id, sizeof(unsigned short), h);

	*id = (((h[0] << 8) | h[1]) ^ ((h[2] << 8) | h[3])) & INT_MAX;
}

void CoapClient::HandleRead(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	Ptr<Packet> packet;
	Address from;

	while ((packet = socket->RecvFrom(from))) {
		m_packetReceived(packet, from);

		if (InetSocketAddress::IsMatchingType (from)) {
			NS_LOG_INFO(
					"At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (from).GetPort ());
		} else if (Inet6SocketAddress::IsMatchingType(from)) {
			NS_LOG_INFO(
					"At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " << Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ());
		}
		SeqTsHeader seqTs;
		packet->RemoveHeader(seqTs);
		uint32_t currentSequenceNumber = seqTs.GetSeq();
		// msg handling (parsing) not obligatory but for analysis
		if (!this->CoapHandleMessage(from, packet)) {
			NS_LOG_ERROR("Cannot handle message. Abort.");
			return;
		}
	}
}

} // Namespace ns3

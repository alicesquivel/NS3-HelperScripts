#include "ns3/mobility-model.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "custom-application.h"
#include "custom-data-tag.h"

namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("CustomApplication");
  NS_OBJECT_ENSURE_REGISTERED(CustomApplication);

TypeId CustomApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::CustomApplication")
                .SetParent <Application> ()
                .AddConstructor<CustomApplication> ()
                .AddAttribute ("Interval", "Broadcast Interval",
                      TimeValue (MilliSeconds(100)),
                      MakeTimeAccessor (&CustomApplication::m_broadcast_time),
                      MakeTimeChecker()
                      )
                ;
    return tid;
}

TypeId CustomApplication::GetInstanceTypeId() const
{
    return CustomApplication::GetTypeId();
}

CustomApplication::CustomApplication()
{
    m_broadcast_time = MilliSeconds (100); //every 100ms
    m_packetSize = 1000; //1000 bytes
}
CustomApplication::~CustomApplication()
{

}
void
CustomApplication::StartApplication()
{
    NS_LOG_FUNCTION (this);
    //Set A Receive callback
    Ptr<Node> n = GetNode ();
    for (uint32_t i = 0; i < n->GetNDevices (); i++)
    {
        Ptr<NetDevice> dev = n->GetDevice (i);
        if (dev->GetInstanceTypeId () == WaveNetDevice::GetTypeId())
        {
            m_waveDevice = DynamicCast <WaveNetDevice> (dev);
            //ReceivePacket will be called when a packet is received
            dev->SetReceiveCallback (MakeCallback (&CustomApplication::ReceivePacket, this));

            /*
            If you want promiscous receive callback, connect to this trace. 
            For every packet received, both functions ReceivePacket & PromiscRx will be called. with PromicRx being called first!
            */
            Ptr<WifiPhy> phy = m_waveDevice->GetPhys()[0]; //default, there's only one PHY in a WaveNetDevice
            phy->TraceConnectWithoutContext ("MonitorSnifferRx", MakeCallback(&CustomApplication::PromiscRx, this));
            break;
        } 
    }
    if (m_waveDevice)
    {
        //Let's create a bit of randomness with the first broadcast packet time to avoid collision
        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
        Time random_offset = MicroSeconds (rand->GetValue(50,200));

        Simulator::Schedule (m_broadcast_time+random_offset, &CustomApplication::BroadcastInformation, this);
    }
    else
    {
        NS_FATAL_ERROR ("There's no WaveNetDevice in your node");
    }  
}
void 
CustomApplication::SetBroadcastInterval (Time interval)
{
    m_broadcast_time = interval;
}

void
CustomApplication::BroadcastInformation()
{
    NS_LOG_FUNCTION (this);
    //Setup transmission parameters
    TxInfo tx;
    tx.channelNumber = CCH; 
    tx.priority = 7; //highest priority.
    tx.txPowerLevel = 7;
    tx.dataRate = WifiMode("OfdmRate6MbpsBW10MHz");
    
    Ptr<Packet> packet = Create <Packet> (m_packetSize);
    
    //let's attach our custom data tag to it
    CustomDataTag tag;
    tag.SetNodeId ( GetNode()->GetId() );
    tag.SetPosition ( GetNode()->GetObject<MobilityModel>()->GetPosition());
    //timestamp is set in the default constructor of the CustomDataTag class as Simulator::Now()

    //attach the tag to the packet
    packet->AddPacketTag (tag);

    //Broadcast the packet as WSMP (0x88dc)
    m_waveDevice->SendX (packet, Mac48Address::GetBroadcast(), 0x88dc, tx);

    //Schedule next broadcast 
    Simulator::Schedule (m_broadcast_time, &CustomApplication::BroadcastInformation, this);
}

bool
CustomApplication::ReceivePacket (Ptr<NetDevice> device, Ptr<const Packet> packet,uint16_t protocol, const Address &sender)
{
    NS_LOG_FUNCTION (device << packet << protocol << sender);
    /*
        Packets received here only have Application data, no WifiMacHeader. 
        We created packets with 1000 bytes payload, so we'll get 1000 bytes of payload.
    */
    NS_LOG_UNCOND ("ReceivePacket() : Node " << GetNode()->GetId() << " : Received a packet from " << sender << " Size:" << packet->GetSize());
    
    //Let's check if packet has a tag attached!
    CustomDataTag tag;
    if (packet->PeekPacketTag (tag))
    {
        NS_LOG_UNCOND ("\tFrom Node Id: " << tag.GetNodeId() << " at " << tag.GetPosition() 
                        << "\tPacket Timestamp: " << tag.GetTimestamp() << " delay="<< Now()-tag.GetTimestamp());
    }

    return true;
}
void 
CustomApplication::PromiscRx (Ptr<const Packet> packet, uint16_t channelFreq, WifiTxVector tx, MpduInfo mpdu, SignalNoiseDbm sn)
{
    //This is a promiscous trace. It will pick broadcast packets, and packets not directed to this node's MAC address.
    /*
        Packets received here have MAC headers and payload.
        If packets are created with 1000 bytes payload, the size here is about 38 bytes larger. 
    */
    NS_LOG_UNCOND ("PromiscRx() : Node " << GetNode()->GetId() << " : ChannelFreq: " << channelFreq << " Mode: " << tx.GetMode()
                 << " Signal: " << sn.signal << " Noise: " << sn.noise << " Size: " << packet->GetSize());    
    WifiMacHeader hdr;
    if (packet->PeekHeader (hdr))
    {
        //Let's see if this packet is intended to this node
        Mac48Address destination = hdr.GetAddr1();
        Mac48Address source = hdr.GetAddr2();
        Mac48Address myMacAddress = m_waveDevice->GetMac(CCH)->GetAddress();
        //A packet is intened to me if it targets my MAC address, or it's a broadcast message
        if ( destination==Mac48Address::GetBroadcast() || destination==myMacAddress)
        {
            NS_LOG_UNCOND ("\tFrom: " << source << "\n\tSeq. No. " << hdr.GetSequenceNumber() );
            //Do something for this type of packets
        }
        else //Well, this packet is not intended for me
        {
            //Maybe record some information about neighbors
        }    
    }
}
}//end of ns3


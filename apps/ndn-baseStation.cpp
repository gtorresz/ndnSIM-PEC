/*
 * Copyright ( C ) 2020 New Mexico State University- Board of Regents
 *
 * See AUTHORS.md for complete list of authors and contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * ( at your option ) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ndn-baseStation.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/integer.h"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include <ndn-cxx/lp/tags.hpp>

#include <memory>
#include <fstream>

NS_LOG_COMPONENT_DEFINE("ndn.BaseStation");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(BaseStation);

TypeId
BaseStation::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::BaseStation")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<BaseStation>()
      .AddAttribute( "StartSeq", "Initial sequence number", IntegerValue( 0 ),
                    MakeIntegerAccessor( &BaseStation::m_seq ), MakeIntegerChecker<int32_t>() )
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&BaseStation::m_prefix), MakeNameChecker())
      .AddAttribute( "UpdatePrefix", "Name to be used for BaseStation querys to nerby servers", StringValue( "/" ),
                    MakeNameAccessor( &BaseStation::m_interestName ), MakeNameChecker() )
      .AddAttribute( "HopLimit", "Name to be used for BaseStation querys to nerby servers", UintegerValue(1),
                    MakeUintegerAccessor( &BaseStation::m_hoplimit ), MakeUintegerChecker<uint32_t>() )
      .AddAttribute("Postfix", "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         	    StringValue("/"), MakeNameAccessor(&BaseStation::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&BaseStation::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("DataFreshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&BaseStation::m_freshness),
                    MakeTimeChecker())
      .AddAttribute("Frequency", "Frequency of data packets, if 0, then no spontaneous publish",
                    TimeValue(Seconds(5)), MakeTimeAccessor(&BaseStation::m_frequency),
                    MakeTimeChecker())
      .AddAttribute( "LifeTime", "LifeTime for subscription packet", StringValue( "5400s" ),
                    MakeTimeAccessor( &BaseStation::m_interestLifeTime ), MakeTimeChecker() )

      .AddAttribute( "Proactive", "Proactive 0-false, all  else true", IntegerValue( 1 ),
                    MakeIntegerAccessor( &BaseStation::m_proactive ), MakeIntegerChecker<int32_t>() )

      .AddAttribute("Signature","Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&BaseStation::m_signature),
         MakeUintegerChecker<uint32_t>())

      .AddAttribute("KeyLocator", "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&BaseStation::m_keyLocator), MakeNameChecker())

      .AddAttribute("QueryFreshness", "Freshness of query results",
                    TimeValue(Seconds(0.1)), MakeTimeAccessor(&BaseStation::m_qFresh),
                    MakeTimeChecker())


      .AddTraceSource( "SentInterest", "SentInterest",
                      MakeTraceSourceAccessor( &BaseStation::m_sentInterest ),
                      "ns3::ndn::BaseStationConsumer::SentInterestTraceCallback" )


      .AddTraceSource("SentData", "SentData",
                      MakeTraceSourceAccessor(&BaseStation::m_sentData),
                      "ns3::ndn::BaseStation::SentDataTraceCallback")

      .AddTraceSource("ReceivedInterest", "ReceivedInterest",
                      MakeTraceSourceAccessor(&BaseStation::m_receivedInterest),
                      "ns3::ndn::BaseStation::ReceivedInterestTraceCallback");

  return tid;
}

BaseStation::BaseStation()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_seq(0)
  ,m_firstTime(true)
  , m_subscription(0)
  , m_receivedpayload(0)
  , m_subDataSize (1)
{
    NS_LOG_FUNCTION_NOARGS();
  m_rtt = CreateObject<RttMeanDeviation>();

}

// inherited from Application base class.
void
BaseStation::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    App::StartApplication();

    m_prefixWithoutSequence = m_prefix; //store prefix without sequence using to be used in log output

    FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
    ScheduleNextPacket();
    //SendTimeout();
}

void
BaseStation::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    App::StopApplication();
}

void
BaseStation::ScheduleNextPacket()
{
	if(m_proactive){
  		if ( m_firstTime ) {

			m_sendEvent = Simulator::Schedule( Seconds( double( 0.001 ) ), &BaseStation::SendPacket, this );
			m_firstTime = false;

		} else if ( !m_sendEvent.IsRunning() ) {
	
			m_sendEvent = Simulator::Schedule( m_frequency, &BaseStation::SendPacket, this );
                        Simulator::Schedule( m_frequency, &BaseStation::SendToInServers, this );

		}
	}
}


void
BaseStation::SetRetxTimer( Time retxTimer )
{

	// Do not restranmit lost packets, if set to 0
	if ( m_doRetransmission == 1 ) {

		m_retxTimer = retxTimer;

		if ( m_retxEvent.IsRunning() ) {

			//m_retxEvent.Cancel (); // cancel any scheduled cleanup events
			Simulator::Remove( m_retxEvent ); // slower, but better for memory
		}

		// Schedule even with new timeout
		m_retxEvent = Simulator::Schedule( m_retxTimer, &BaseStation::CheckRetxTimeout, this );
	}
}


Time
BaseStation::GetRetxTimer() const
{
	return m_retxTimer;
}

void
BaseStation::CheckRetxTimeout()
{
	Time now = Simulator::Now();

	Time rto = m_rtt->RetransmitTimeout();

	//NS_LOG_DEBUG ( "Current RTO: " << rto.ToDouble ( Time::S ) << "s" );

	while ( !m_seqTimeouts.empty() ) {

		SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
			m_seqTimeouts.get<i_timestamp>().begin();

		// timeout expired?
		if ( entry->time + rto <= now ) {
			uint32_t seqNo = entry->seq;
			m_seqTimeouts.get<i_timestamp>().erase( entry );
			OnTimeout( seqNo );
		} else {
			break; // nothing else to do. All later packets need not be retransmitted
		}
	}

	m_retxEvent = Simulator::Schedule( m_retxTimer, &BaseStation::CheckRetxTimeout, this );
}

void
BaseStation::SendPacket()
{
	// Set default size for payload interets
	if (  m_virtualPayloadSize == 0 ) {
		m_virtualPayloadSize = 4;
	}

	if ( !m_active ) {
		return;
	}
        newServers.clear();
	NS_LOG_FUNCTION_NOARGS();

	uint32_t seq = std::numeric_limits<uint32_t>::max();

	while ( m_retxSeqs.size() ) {

		seq = *m_retxSeqs.begin();
		m_retxSeqs.erase( m_retxSeqs.begin() );
		break;
	}

	/*if ( m_seqMax != std::numeric_limits<uint32_t>::max() ) {

		if ( m_seq >= m_seqMax ) {
			return;
		}
	}*/

	seq = m_seq++;

	shared_ptr<Interest> interest = make_shared<Interest>();
	interest->setNonce( m_rand->GetValue( 0, std::numeric_limits<uint32_t>::max() ) );
	interest->setSubscription( 0 );
        shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);

	nameWithSequence->appendSequenceNumber( seq );


	interest->setName( *nameWithSequence );
	time::milliseconds interestLifeTime( m_interestLifeTime.GetMilliSeconds() );
	interest->setInterestLifetime( interestLifeTime );
	interest->setHopLimit(m_hoplimit);
        //std::cout<<"oth "<<interest->getName()<<std::endl;
	NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) > sending Interest: " << interest->getName() /*m_interestName*/ << " with Payload = " << interest->getPayloadLength() << "bytes" );
	WillSendOutInterest( seq );

	m_transmittedInterests( interest, this, m_face );
	m_appLink->onReceiveInterest( *interest );

	// Callback for sent payload interests
	m_sentInterest( GetNode()->GetId(), interest );

	ScheduleNextPacket();
}

void
BaseStation::SendToInServers()
{
  for(int i; i<inServers.size();i++){
        // Set default size for payload interets
        if ( !m_active ) {
                return;
        }

        NS_LOG_FUNCTION_NOARGS();

        uint32_t seq = std::numeric_limits<uint32_t>::max();

        while ( m_retxSeqs.size() ) {

                seq = *m_retxSeqs.begin();
                m_retxSeqs.erase( m_retxSeqs.begin() );
                break;
        }

        seq = m_seq++;

        shared_ptr<Interest> interest = make_shared<Interest>();
        interest->setNonce( m_rand->GetValue( 0, std::numeric_limits<uint32_t>::max() ) );
        interest->setSubscription( 0 );
        shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName.getSubName(0,1));
        nameWithSequence->append("service");
        std::string temp = inServers[i].toUri();
	nameWithSequence->append("server" + temp.substr(1));
        nameWithSequence->append(m_interestName.getSubName(2,1));

        nameWithSequence->appendSequenceNumber( seq );
        
        
        interest->setName( *nameWithSequence );
        //std::cout<<interest->getName()<<std::endl;
        time::milliseconds interestLifeTime( m_interestLifeTime.GetMilliSeconds() );
        interest->setInterestLifetime( interestLifeTime );
        WillSendOutInterest( seq );

        m_transmittedInterests( interest, this, m_face );
        m_appLink->onReceiveInterest( *interest );

        // Callback for sent payload interests
        m_sentInterest( GetNode()->GetId(), interest );

  }
}



///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
BaseStation::OnData( shared_ptr<const Data> data )
{

	if ( !m_active ) {
		return;
	}
       
	App::OnData( data ); // tracing inside

        NS_LOG_FUNCTION( this << data );	
	std::vector<uint8_t> payloadVector( &data->getContent().value()[0], &data->getContent().value()[data->getContent().value_size()] );
        std::string payload( payloadVector.begin(), payloadVector.end() );
         
       if(data->getName().getSubName(1,1).toUri()== "/baseQuery"){
          return;
        }
        if (newServers.empty()){
           Simulator::Schedule( Seconds( double( 0.005 ) ), &BaseStation::SendGathered, this );
         }
	
        //std::cout<<"Datata "<<data->getName()<<" "<<payload<<"hmm"<<std::endl;

	std::vector<std::string> server = SplitString(payload, ',');
	newServers[server[0]] = std::stoi(server[1]);
	
	// This could be a problem......
	//uint32_t seq = data->getName().at( -1 ).toSequenceNumber();

	NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) < Received DATA for " << data->getName() << " TIME: " << Simulator::Now() );

	// Callback for received subscription data
	m_receivedData( GetNode()->GetId(), data );

	int hopCount = 0;
	auto hopCountTag = data->getTag<lp::HopCountTag>();

	if ( hopCountTag != nullptr ) { // e.g., packet came from local node's cache
		hopCount = *hopCountTag;
	}

	NS_LOG_DEBUG( "Hop count: " << hopCount );

	// Enable trace file for Interests with sequence number ( subscription = 0 )

		// This could be a problem......
		uint32_t seq = data->getName().at( -1 ).toSequenceNumber();
		SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find( seq );

		if ( entry != m_seqLastDelay.end() ) {
			m_lastRetransmittedInterestDataDelay( this, seq, Simulator::Now() - entry->time, hopCount );
		}

		entry = m_seqFullDelay.find( seq );

		if ( entry != m_seqFullDelay.end() ) {
			m_firstInterestDataDelay( this, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount );
		}

		m_seqRetxCounts.erase( seq );
		m_seqFullDelay.erase( seq );
		m_seqLastDelay.erase( seq );
		m_seqTimeouts.erase( seq );
		m_retxSeqs.erase( seq );
		m_rtt->AckSeq( SequenceNumber32( seq ) );

	//data = nullptr;
	//data.reset();
        //std::cout << "data received at APP in subscriber " << data->getName() << " count " << data.use_count() << std::endl;
}

void
BaseStation::WillSendOutInterest(uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
                                << m_seqTimeouts.size() << " items");

  m_seqTimeouts.insert(SeqTimeout(sequenceNumber, Simulator::Now()));
  m_seqFullDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqLastDelay.erase(sequenceNumber);
  m_seqLastDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqRetxCounts[sequenceNumber]++;

  m_rtt->SentSeq(SequenceNumber32(sequenceNumber), 1);
}


void
BaseStation::OnTimeout( uint32_t sequenceNumber )
{
	//NS_LOG_FUNCTION( sequenceNumber );
	//std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
	//m_rtt->RetransmitTimeout ().ToDouble ( Time::S ) << "s\n";

	m_rtt->IncreaseMultiplier(); // Double the next RTO
	m_rtt->SentSeq( SequenceNumber32( sequenceNumber ), 1 ); // make sure to disable RTT calculation for this sample
	m_retxSeqs.insert( sequenceNumber );
	ScheduleNextPacket();
}

void
BaseStation::SendGathered()
{
   //std::cout<<"Sending out data "<<Simulator::Now().GetSeconds()<<std::endl;
   //std::cout<<newServers.size()<<" "<<inServers.size()<<std::endl;
   if(newServers.size()>=inServers.size()){
      servers.clear();
      for(auto iter = newServers.begin(); iter != newServers.end(); ++iter){
          servers[iter->first]=iter->second;
      }
      Simulator::Schedule( Seconds( double( 0.005 ) ), &BaseStation::SendGathered, this );
   }
   else Simulator::Schedule( Seconds( double( 0.001 ) ), &BaseStation::SendGathered, this );
   while (!pending.empty()){
      Name temp = pending.back();
      pending.pop_back();
      SendData(temp, true);
      isFresh = true;
      Simulator::Schedule( m_qFresh, &BaseStation::updateFreshness, this );
   }
}



void
BaseStation::OnInterest(shared_ptr<const Interest> interest)
{
    NS_LOG_INFO("Interest name = " << interest->getName() << " & PAYLOAD = " << interest->getPayloadLength() << " TIME: " << Simulator::Now());
    App::OnInterest(interest); // tracing inside

    NS_LOG_FUNCTION(this << interest);

    // Callback for received interests
    m_receivedInterest(GetNode()->GetId(), interest);

    if (!m_active)
        return;

    //Send data if there's a subscription
    m_subscription = interest->getSubscription();
    m_receivedpayload = interest->getPayloadLength();

    std::vector<uint8_t> payloadVector( &interest->getPayload()[0], &interest->getPayload()[interest->getPayloadLength()] );
    std::string payload( payloadVector.begin(), payloadVector.end() );
    //std::cout<<"At the base " << payload <<" "<<server<<std::endl;
    //std::cout<<interest->getName()<<Simulator::Now().GetSeconds()<<std::endl;
    bool sendPayload = false;
    if(interest->getName().getSubName(1,1).toUri() == "/service"){
	if(!m_proactive and !isFresh) {
            if(pending.empty()){
	     	Simulator::Schedule( Seconds( double( 0.035 ) ), &BaseStation::SendGathered, this );
                SendPacket();
		SendToInServers();
            }
            pending.push_back(interest->getName());
            return;
        }
        else 
		sendPayload = true;
    }
    //else if(interest->getName().getSubName(1,1).toUri() == "/consumerQuery") 
    
    //else if(payload == "remove") 
	    //servers.erase(server);
    else if (interest->getName().getSubName(1,1).toUri() == "/update"){
     if ( newServers.empty())
           Simulator::Schedule( Seconds( double( 0.005 ) ), &BaseStation::SendGathered, this );    
 
    for(auto iter = servers.begin(); iter != servers.end(); ++iter){
      //  std::cout << (*iter) << ", " ;
    }
    std::string server = interest->getName().getSubName(2,1).toUri();
    std::string temp = interest->getName().getSubName(3,1).toUri();
    server+= temp.substr(1);
    //std::cout<<server<<" "<<interest->getName()<<std::endl;
    newServers[server] = payload;
    }
    else return;
    //Normal interest, without a subscription
    if (m_subscription == 0) {
        SendData(interest->getName(), sendPayload);
    }
    if(interest->getName().getSubName(2,1).toUri() == "/server"){
      inServers.push_back(interest->getName().getSubName(3,1)); 
    }
}

void
BaseStation::SendTimeout(){
	
    double send_delay = 0.0; //(double)((Simulator::Now().GetNanoSeconds())/1000000000);

    //Do not send initial data before scheduling with the input frequency
    if(m_firstTime) {
        m_firstTime = false;
    } else {
        //Only send data when there is a subscription (1-soft or 2-hard)
        if (m_subscription == 1 || m_subscription == 2) {
            //Send multiple chunks of 1Kbyte (1024bytes) data to physical node
            for (int i=0; i<(int)m_subDataSize; i++) {
                //SendData(m_prefix);
                Simulator::Schedule(Seconds(send_delay), &BaseStation::SendData, this, m_prefix, false);
                send_delay = send_delay + 0.03;
            }
        }
    }

    if(m_frequency != 0) {
        m_txEvent = Simulator::Schedule(m_frequency, &BaseStation::SendTimeout, this);
    }
}

void
BaseStation::SendData(const Name &dataName, bool payload)
{
    if (!m_active)
        return;

    //send only ACK (payload size = 1) if payloaded interest was received
    if (m_subscription == 0 && m_receivedpayload > 0) {
        m_virtualPayloadSize = 1;
    }

    //std::cout << " ack payload= " << m_virtualPayloadSize << std::endl;

    auto data = make_shared<Data>();
    data->setName(dataName);
    data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

    std::string serverList = "";
    for(auto iter  : servers){
       if(iter.second != "")
       serverList += iter.second + " ";
    }

    //std::cout << "printing server list\n"<<serverList<<" "<< Simulator::Now().GetSeconds()<<" "<< GetNode()->GetId()<<std::endl;
    if(payload){
       std::vector<uint8_t> myVector( serverList.begin(), serverList.end() );
       uint8_t *p = &myVector[0];
       data->setContent( p, myVector.size()); // Add payload to interest
    }

    else {
       data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
    }

    Signature signature;
    SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

    if (m_keyLocator.size() > 0) {
        signatureInfo.setKeyLocator(m_keyLocator);
    }

    signature.setInfo(signatureInfo);
    signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

    data->setSignature(signature);

    if (m_subscription == 0 && m_receivedpayload > 0) {
        NS_LOG_INFO("node(" << GetNode()->GetId() << ") sending ACK: " << data->getName() << " TIME: " << Simulator::Now());
    } else {
        NS_LOG_INFO("node(" << GetNode()->GetId() << ") sending DATA for " << data->getName() << " TIME: " << Simulator::Now());
    }

    //std::cout << "use count " << data.use_count() << " data content/payload size = " << data->getContent().value_size() << std::endl;

    // to create real wire encoding
    data->wireEncode();
    //std::cout<<data->getName()<<std::endl;
    m_transmittedDatas(data, this, m_face);
    m_appLink->onReceiveData(*data);

    // Callback for tranmitted subscription data
    m_sentData(GetNode()->GetId(), data);

    //std::cout << "APP use count " << data.use_count () << std::endl;
    //m_appLink->DanFree();
}

std::vector<std::string>
BaseStation::SplitString( std::string strLine, char delimiter ) {

        std::string str = strLine;
        std::vector<std::string> result;
        int i =0;
        std::string buildStr = "";

        for ( i = 0; i<str.size(); i++) {

           if ( str[i]== delimiter ) {
              result.push_back( buildStr );
	      buildStr = "";
	   } 
	   else {
              buildStr += str[i];    
	   }
        }
        
	if(buildStr!="")
           result.push_back( buildStr );

        return result;
}

} // namespace ndn
} // namespace ns3

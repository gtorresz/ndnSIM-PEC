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

#include "ndn-intel-consumer.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "utils/ndn-ns3-packet-tag.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <ndn-cxx/lp/tags.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdlib.h>

#include <fstream>

NS_LOG_COMPONENT_DEFINE( "ndn.IntelConsumer" );

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED( intelConsumer );

TypeId
intelConsumer::GetTypeId( void )
{
  static TypeId tid =
    TypeId( "ns3::ndn::IntelConsumer" )
      .SetGroupName( "Ndn" )
      .SetParent<App>()
      .AddConstructor<intelConsumer>()

      .AddAttribute( "StartSeq", "Initial sequence number", IntegerValue( 0 ),
                    MakeIntegerAccessor( &intelConsumer::m_seq ), MakeIntegerChecker<int32_t>() )
      .AddAttribute( "Prefix", "Name of the Interest", StringValue( "/" ),
                    MakeNameAccessor( &intelConsumer::m_queryName ), MakeNameChecker() )
      .AddAttribute( "LifeTime", "LifeTime for subscription packet", StringValue( "5400s" ),
                    MakeTimeAccessor( &intelConsumer::m_interestLifeTime ), MakeTimeChecker() )
      .AddAttribute( "Frequency",
                    "Timeout defining how frequently subscription should be reinforced",
		    TimeValue( Seconds( 1 ) ),
                    MakeTimeAccessor( &intelConsumer::m_longInterval ), MakeTimeChecker() )

      .AddAttribute( "DataSendFrequency",
                    "Timeout defining how frequently subscription should be reinforced",
                    TimeValue( Seconds( 0.1 ) ),
                    MakeTimeAccessor( &intelConsumer::m_dataInterval ), MakeTimeChecker() )

      .AddAttribute( "RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue( "50s" ),
                    MakeTimeAccessor( &intelConsumer::GetRetxTimer, &intelConsumer::SetRetxTimer ),
                    MakeTimeChecker() )

      .AddAttribute( "RetransmitPackets", "Retransmit lost packets if set to 1, otherwise do not perform retransmission", IntegerValue( 1 ),
                    MakeIntegerAccessor( &intelConsumer::m_doRetransmission ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "Offset", "Random offset to randomize sending of interests", IntegerValue( 0 ),
                    MakeIntegerAccessor( &intelConsumer::m_offset ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "PayloadSize", "Virtual payload size for interest packets", UintegerValue( 0 ),
                    MakeUintegerAccessor( &intelConsumer::m_virtualPayloadSize ),
                    MakeUintegerChecker<uint32_t>() )

      .AddAttribute( "Service", "Service to request", StringValue( "1" ),
                    MakeStringAccessor( &intelConsumer::m_service ),
                    MakeStringChecker() )

      .AddAttribute( "NodeID", "Identifier for node", StringValue( "1" ),
                    MakeStringAccessor( &intelConsumer::m_nodeId ),
                    MakeStringChecker() )

      .AddTraceSource( "LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor( &intelConsumer::m_lastRetransmittedInterestDataDelay ),
                      "ns3::ndn::intelConsumer::LastRetransmittedInterestDataDelayCallback" )

      .AddTraceSource( "FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor( &intelConsumer::m_firstInterestDataDelay ),
                      "ns3::ndn::intelConsumer::FirstInterestDataDelayCallback" )

      .AddTraceSource( "ReceivedData", "ReceivedData",
                      MakeTraceSourceAccessor( &intelConsumer::m_receivedData ),
                      "ns3::ndn::intelConsumer::ReceivedDataTraceCallback" )

      .AddTraceSource( "SentInterest", "SentInterest",
                      MakeTraceSourceAccessor( &intelConsumer::m_sentInterest ),
                      "ns3::ndn::intelConsumer::SentInterestTraceCallback" )
      
     .AddTraceSource("ServerChoice", "ServerChoice",
                      MakeTraceSourceAccessor( &intelConsumer::m_serverChoice),
                      "ns3::ndn::PECServer::ServerChoiceTraceCallback");
      ;

	  return tid;
}

intelConsumer::intelConsumer()
    : m_rand( CreateObject<UniformRandomVariable>() )
    , m_seq( 0 )
    , m_seqMax( std::numeric_limits<uint32_t>::max() ) // set to max value on uint32
    , m_firstTime ( true )
    , m_doRetransmission( 1 )
{
	NS_LOG_FUNCTION_NOARGS();
	m_rtt = CreateObject<RttMeanDeviation>();
}


intelConsumer::~intelConsumer()
{
m_subscription =1;
}


void
intelConsumer::ScheduleNextPacket()
{
	if ( m_firstTime ) {

		m_sendEvent = Simulator::Schedule( Seconds( double( m_offset ) ), &intelConsumer::SendPacket, this );
		m_firstTime = false;

	} else if ( !m_sendEvent.IsRunning() ) {
		//std::cout<<"should be scheduling with time " <<m_txInterval<<std::endl;
		m_sendEvent = Simulator::Schedule( m_txInterval, &intelConsumer::SendPacket, this );

	}
}


void
intelConsumer::SetRetxTimer( Time retxTimer )
{

	// Do not restranmit lost packets, if set to 0
	if ( m_doRetransmission == 1 ) {

		m_retxTimer = retxTimer;

		if ( m_retxEvent.IsRunning() ) {

			//m_retxEvent.Cancel (); // cancel any scheduled cleanup events
			Simulator::Remove( m_retxEvent ); // slower, but better for memory
		}

		// Schedule even with new timeout
		m_retxEvent = Simulator::Schedule( m_retxTimer, &intelConsumer::CheckRetxTimeout, this );
	}
}


Time
intelConsumer::GetRetxTimer() const
{
	return m_retxTimer;
}

void
intelConsumer::CheckRetxTimeout()
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

	m_retxEvent = Simulator::Schedule( m_retxTimer, &intelConsumer::CheckRetxTimeout, this );
}



// Application methods
void
intelConsumer::StartApplication()
{
	NS_LOG_FUNCTION_NOARGS();
	App::StartApplication();
        m_interestName = m_queryName;
	m_interestName.append("service");
	m_interestName.append(m_nodeId);
	ScheduleNextPacket();
	m_subscription = 1;
	m_txInterval = m_longInterval;
	//std::cout<<m_service<<std::endl;
}

void
intelConsumer::StopApplication() // Called at time specified by Stop
{
	NS_LOG_FUNCTION_NOARGS();
	Simulator::Cancel( m_sendEvent );
	App::StopApplication();
}

void
intelConsumer::SendPacket()
{
	// Set default size for payload interets
	if ( m_subscription == 0 && m_virtualPayloadSize == 0 ) {
		m_virtualPayloadSize = 4;
	}

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

	if ( m_seqMax != std::numeric_limits<uint32_t>::max() ) {

		if ( m_seq >= m_seqMax ) {
			return;
		}
	}

	seq = m_seq++;
	uint8_t payload[1] = {1};

	shared_ptr<Name> nameWithSequence = make_shared<Name>( m_interestName );

	shared_ptr<Interest> interest = make_shared<Interest>();
	interest->setNonce( m_rand->GetValue( 0, std::numeric_limits<uint32_t>::max() ) );
	interest->setSubscription( m_subscription );
	nameWithSequence->appendSequenceNumber( seq );

	//if ( m_subscription == 0 ) {
//		interest->setPayload( payload, m_virtualPayloadSize ); //add payload to interest
//	}

	interest->setName( *nameWithSequence );
	time::milliseconds interestLifeTime( m_interestLifeTime.GetMilliSeconds() );
	interest->setInterestLifetime( interestLifeTime );

	NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) > sending Interest: " << interest->getName() /*m_interestName*/ << " with Payload = " << interest->getPayloadLength() << "bytes" );

        if(interest->getName().getSubName(1,1).toUri()=="/service")
		interest->setHopLimit(1);
	else {
	   time::milliseconds lifeTime(Seconds( 5 ).GetMilliSeconds());
           interest->setInterestLifetime( lifeTime );		
	}

	WillSendOutInterest( seq );

	m_transmittedInterests( interest, this, m_face );
	//std::cout<<interest->getName() <<std::endl;
	m_appLink->onReceiveInterest( *interest );

	/*if(interest->getName().getSubName(3,1).toUri()=="/data"){
       	   m_dataReq--;
           if(m_dataReq==0){
              m_interestName = m_queryName;
	      m_interestName.append("compute");
	      m_interestName.append(bestServer);
              m_interestName.append("execute");
              m_interestName.append(std::to_string(GetNode()->GetId()));
	   }
	   ScheduleNextPacket();	      
	}*/

	// Callback for sent payload interests
	if(m_subscription==1){
   	   m_sentInterest( GetNode()->GetId(), interest );
	}
}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
intelConsumer::OnData( shared_ptr<const Data> data )
{

	if ( !m_active ) {
		return;
	}
//std::cout<<data->getName()<<std::endl;

	App::OnData( data ); // tracing inside

	NS_LOG_FUNCTION( this << data );

	// This could be a problem......
	//uint32_t seq = data->getName().at( -1 ).toSequenceNumber();
        
	if(!chosen && data->getName().getSubName(1,1)=="service"){
          std::vector<uint8_t> payloadVector( &data->getContent().value()[0], &data->getContent().value()[data->getContent().value_size()] );
          std::string payload( payloadVector.begin(), payloadVector.end() );

          std::vector<std::string> servers = SplitString(payload, ' ');
          if(servers.size() > 1)
	  {
	     for(int i=0; i < servers.size(); i++)
	     {
	        std::vector<std::string> server = SplitString(servers[i], ',');	 
		bool hasService = false;
	        for(int k= 2; k < server.size(); k++ ){
		   if(server[k] == m_service)
		      hasService = true;
		}	
                //std::cout<<servers[i]<<std::endl;
                if(hasService && PECservers[server[0]] == 0){
	           PECservers[server[0]] = std::stoi(server[1]);
                   conMap[server[0]] = false;
                }
	     }
	  }
	  else 
	  {
	     //std::cout<<data->getName()<<std::endl;	  
	     std::vector<std::string> server = SplitString(servers[0], ',');
	     bool hasService = false;
             for(int k= 2; k < server.size(); k++ ){
                if(server[k] == m_service)
                   hasService = true;
             }
	     if(hasService){
                PECservers[server[0]] = std::stoi(server[1]);	   
                conMap[server[0]] = true;
	     }
  	  }

          //for (auto i : PECservers) 
	  //{
             //if(lowestUtil>i.second){
             //   lowestUtil=i.second;
	     //   bestServer = i.first;
	     //} 
	  //}

          //Name payloadName = m_interestName.toUri()+payload;
 
	  NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) < Received DATA for " << data->getName() << " Content: " << payload << " Current Best:"<< bestServer << " " << lowestUtil << " TIME: " << Simulator::Now() );

	  if (firstResponse){
	     firstResponse = false;
             Simulator::Schedule( Seconds(0.05), &intelConsumer::ChooseServer, this );           
	  }

	}
        /*else if(data->getName().getSubName(3,1)=="request"){
	   std::vector<uint8_t> payloadVector( &data->getContent().value()[0], 
			   &data->getContent().value()[data->getContent().value_size()] );
           std::string payload( payloadVector.begin(), payloadVector.end() );

	   m_dataReq = std::stoi(payload);
           m_interestName = m_queryName;
	   m_interestName.append("compute");
	   m_interestName.append(bestServer);
	   m_interestName.append("data");
	   m_interestName.append(std::to_string(GetNode()->GetId()));
           m_txInterval = m_dataInterval;
	   ScheduleNextPacket();
	}*/
	else if(data->getName().getSubName(1,1)=="compute" && data->getName().getSubName(-2,1)!="obtain")
	{
	//std::cout<<"new request\n";
           firstResponse = true;
           m_subscription = 1;
           chosen = false;

	   //clear query related attributes
           PECservers.clear();
	   bestServer = "";
	   lowestUtil = 1000;
           std::vector<uint8_t> payloadVector( &data->getContent().value()[0], &data->getContent().value()[data->getContent().value_size()] );
           std::string payload(payloadVector.begin(), payloadVector.end());

	   Simulator::Schedule( Seconds(std::stod(payload)), &intelConsumer::SendObtainPacket, this, data->getName() );
           m_interestName = m_queryName;
           m_interestName.append("service");
	   m_interestName.append(m_nodeId);
	   m_txInterval = m_longInterval;
           ScheduleNextPacket();

	   //m_receivedData( GetNode()->GetId(), data, m_intSent );
	}
        else if(data->getName().getSubName(-2,1)=="obtain"){
           m_receivedData( GetNode()->GetId(), data, m_intSent );
        }
	// Callback for received subscription data
	//m_receivedData( GetNode()->GetId(), data );

	int hopCount = 0;
	auto hopCountTag = data->getTag<lp::HopCountTag>();

	if ( hopCountTag != nullptr ) { // e.g., packet came from local node's cache
		hopCount = *hopCountTag;
	}

	NS_LOG_DEBUG( "Hop count: " << hopCount );

	// Enable trace file for Interests with sequence number ( subscription = 0 )
	if ( m_subscription == 0 ) {

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
	}

	//data = nullptr;
	//data.reset();
	//std::cout << "data received at APP in subscriber " << data->getName() << " count " << data.use_count() << std::endl;
	
	/*if(!chosen){
    	   m_subscription = 0;
           chosen = true;
           //m_interestName = payloadName;
	   //SendPacket();
        }
        else{ 
     	   m_subscription = 1;
	   //chosen = false;
           //m_interestName = m_queryName;
           //ScheduleNextPacket();
	}*/


}

void
intelConsumer::ChooseServer()
{

   std::string ser="";
   for (auto i : PECservers)
   {
      ser += i.first+" ";
      //std::cout<<i.first<<","<<i.second<<" ";  
      if(lowestUtil>i.second){
         lowestUtil=i.second;
         bestServer = i.first;
      }
   }
   //std::cout<<"\nBest server: "<<bestServer<<", "<<lowestUtil<<"\n\n";

   m_subscription = 0;
   chosen = true;
   m_interestName = m_queryName;
   m_interestName.append("compute");
   m_interestName.append(bestServer);
   //m_interestName.append("request");
   m_interestName.append(m_nodeId);

   //m_intSent = 0;
   SendPacket();

   m_serverChoice(GetNode()->GetId(), bestServer, lowestUtil, ser, conMap[bestServer]);
}

void
intelConsumer::OnTimeout( uint32_t sequenceNumber )
{
	//NS_LOG_FUNCTION( sequenceNumber );
	std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
	m_rtt->RetransmitTimeout ().ToDouble ( Time::S ) << "s\n";

	m_rtt->IncreaseMultiplier(); // Double the next RTO
	m_rtt->SentSeq( SequenceNumber32( sequenceNumber ), 1 ); // make sure to disable RTT calculation for this sample
	m_retxSeqs.insert( sequenceNumber );
	//ScheduleNextPacket();
}

void
intelConsumer::WillSendOutInterest( uint32_t sequenceNumber )
{
	NS_LOG_DEBUG( "Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
			<< m_seqTimeouts.size() << " items" );
	//m_seqTimeouts.insert( SeqTimeout( sequenceNumber, Simulator::Now() ) );
	m_seqFullDelay.insert( SeqTimeout( sequenceNumber, Simulator::Now() ) );
	m_seqLastDelay.erase( sequenceNumber );
	m_seqLastDelay.insert( SeqTimeout( sequenceNumber, Simulator::Now() ) );
	m_seqRetxCounts[sequenceNumber]++;
	m_rtt->SentSeq( SequenceNumber32( sequenceNumber ), 1 );
}


std::vector<std::string>
intelConsumer::SplitString( std::string strLine, char delimiter ) {

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
void
intelConsumer::SendObtainPacket(Name interestName)
{
        // Set default size for payload interets
        if ( m_subscription == 0 && m_virtualPayloadSize == 0 ) {
                m_virtualPayloadSize = 4;
        }

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

        if ( m_seqMax != std::numeric_limits<uint32_t>::max() ) {

                if ( m_seq >= m_seqMax ) {
                        return;
                }
        }

        shared_ptr<Interest> interest = make_shared<Interest>();
        interest->setNonce( m_rand->GetValue( 0, std::numeric_limits<uint32_t>::max()));
        interest->setSubscription( m_subscription );
        Name se = interestName.getSubName(-1, 1);
        interestName = interestName.getSubName(0,  interestName.size()-1);
        interestName.append("obtain");
        interestName.append( se );


        //if ( m_subscription == 0 ) {
//              interest->setPayload( payload, m_virtualPayloadSize ); //add payload to interest
//      }

        interest->setName( interestName );
        time::milliseconds interestLifeTime( m_interestLifeTime.GetMilliSeconds() );
        interest->setInterestLifetime( interestLifeTime );

        NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) > sending Interest: " << interest->getName() /*m_interestName*/ << " with Payload = " << interest->getPayloadLength() << "bytes" );

        if(interest->getName().getSubName(1,1).toUri()=="/service")
                interest->setHopLimit(1);
        else {
           time::milliseconds lifeTime(Seconds( 5 ).GetMilliSeconds());
           interest->setInterestLifetime( lifeTime );
        }

        //WillSendOutInterest( seq );

        m_transmittedInterests( interest, this, m_face );
        //std::cout<<interest->getName() <<std::endl;
        m_appLink->onReceiveInterest( *interest );

        // Callback for sent payload interests
        //if(m_subscription==1){
        //   m_sentInterest( GetNode()->GetId(), interest );
        //}
}

} // namespace ndn
} // namespace ns3

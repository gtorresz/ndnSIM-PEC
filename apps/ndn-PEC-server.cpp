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

#include "ndn-PEC-server.hpp"
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
#include "helper/ndn-fib-helper.hpp"

#include <ndn-cxx/lp/tags.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>

#include <fstream>

NS_LOG_COMPONENT_DEFINE( "ndn.PEC-Server" );

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED( PECServer );

TypeId
PECServer::GetTypeId( void )
{
  static TypeId tid =
    TypeId( "ns3::ndn::PECServer" )
      .SetGroupName( "Ndn" )
      .SetParent<App>()
      .AddConstructor<PECServer>()

      .AddAttribute( "StartSeq", "Initial sequence number", IntegerValue( 0 ),
                    MakeIntegerAccessor( &PECServer::m_seq ), MakeIntegerChecker<int32_t>() )
      .AddAttribute( "Prefix", "Name of accepted interests", StringValue( "/" ),
                    MakeNameAccessor( &PECServer::m_prefix ), MakeNameChecker() )
      .AddAttribute( "UpdatePrefix", "Name to be used for Interest based updates to base station", StringValue( "/" ),
                    MakeNameAccessor( &PECServer::m_interestName ), MakeNameChecker() )
      .AddAttribute( "LifeTime", "LifeTime for subscription packet", StringValue( "5400s" ),
                    MakeTimeAccessor( &PECServer::m_interestLifeTime ), MakeTimeChecker() )
      .AddAttribute( "Frequency",
                    "Timeout defining how frequently subscription should be reinforced",
		    TimeValue( Seconds( 60 ) ),
                    MakeTimeAccessor( &PECServer::m_txInterval ), MakeTimeChecker() )

      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&PECServer::m_freshness),
                    MakeTimeChecker())


      .AddAttribute( "RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue( "50s" ),
                    MakeTimeAccessor( &PECServer::GetRetxTimer, &PECServer::SetRetxTimer ),
                    MakeTimeChecker() )

      .AddAttribute( "RetransmitPackets", "Retransmit lost packets if set to 1, otherwise do not perform retransmission", IntegerValue( 1 ),
                    MakeIntegerAccessor( &PECServer::m_doRetransmission ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "Offset", "Random offset to randomize sending of interests", IntegerValue( 0 ),
                    MakeIntegerAccessor( &PECServer::m_offset ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "PayloadSize", "Virtual payload size for interest packets", UintegerValue( 0 ),
                    MakeUintegerAccessor( &PECServer::m_virtualPayloadSize ),
                    MakeUintegerChecker<uint32_t>() )

      .AddAttribute( "Signature", "Fake signature, 0 valid signature (default), other values application-specific",
	           UintegerValue(0), MakeUintegerAccessor(&PECServer::m_signature),
	  	   MakeUintegerChecker<uint32_t>())

      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&PECServer::m_keyLocator), MakeNameChecker())

      .AddAttribute( "UtilMin", "Minimum utilization value", IntegerValue( 20 ),
                    MakeIntegerAccessor( &PECServer::m_uMin ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "UtilRange", "Range of variance for base utilization value", IntegerValue( 20 ),
                    MakeIntegerAccessor( &PECServer::m_uRange ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "UtilRise", "The base amount by which utilzation is raised by a request.", IntegerValue( 25 ),
                    MakeIntegerAccessor( &PECServer::m_uRaise ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "UtilRiseRange", "The range by which amount by which utilzation is raised by a request will vary.", IntegerValue( 0 ),
                    MakeIntegerAccessor( &PECServer::m_uRaiseRange ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "Services", "The services offered seperated by a space.", StringValue( "1" ),
                    MakeStringAccessor( &PECServer::m_services ), MakeStringChecker() )

      .AddAttribute( "InServer", "Is this server infratsructure.", IntegerValue( 1 ),
                    MakeIntegerAccessor( &PECServer::m_inServer ), MakeIntegerChecker<int32_t>() )

      .AddAttribute( "StatChangeFreq", "If PEC how often status may change.", TimeValue( Seconds( 5 ) ),
                    MakeTimeAccessor( &PECServer::m_changeInterval ), MakeTimeChecker() )

      .AddAttribute( "ComRate", "Rate used to multiply against computed com time", DoubleValue( 1 ),
                    MakeIntegerAccessor( &PECServer::m_cr ), MakeDoubleChecker<double>() )


      .AddTraceSource( "LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor( &PECServer::m_lastRetransmittedInterestDataDelay ),
                      "ns3::ndn::PECServer::LastRetransmittedInterestDataDelayCallback" )

      .AddTraceSource( "FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor( &PECServer::m_firstInterestDataDelay ),
                      "ns3::ndn::PECServer::FirstInterestDataDelayCallback" )

      .AddTraceSource( "ReceivedData", "ReceivedData",
                      MakeTraceSourceAccessor( &PECServer::m_receivedData ),
                      "ns3::ndn::PECServer::ReceivedDataTraceCallback" )

      .AddTraceSource( "SentInterest", "SentInterest",
                      MakeTraceSourceAccessor( &PECServer::m_sentInterest ),
                      "ns3::ndn::PECServer::SentInterestTraceCallback" )
      
      .AddTraceSource("SentData", "SentData",
                      MakeTraceSourceAccessor(&PECServer::m_sentData),
                      "ns3::ndn::PECServer::SentDataTraceCallback")

      .AddTraceSource("ReceivedInterest", "ReceivedInterest",
                      MakeTraceSourceAccessor(&PECServer::m_receivedInterest),
                      "ns3::ndn::PECServer::ReceivedInterestTraceCallback")

      .AddTraceSource("ServerUpdate", "ServerUpdate",
                      MakeTraceSourceAccessor(&PECServer::m_serverUpdate),
                      "ns3::ndn::PECServer::ServerUpdateTraceCallback");
      ;

	  return tid;
}

PECServer::PECServer()
    : m_rand( CreateObject<UniformRandomVariable>() )
    , m_comTime( CreateObject<NormalRandomVariable> ())
    , m_seq( 0 )
    , m_seqMax( std::numeric_limits<uint32_t>::max() ) // set to max value on uint32
    , m_firstTime ( true )
    , m_doRetransmission( 1 )
{
   
   NS_LOG_FUNCTION_NOARGS();
   m_rtt = CreateObject<RttMeanDeviation>();
   m_comTime->SetAttribute ("Mean", DoubleValue (1.0));
   m_comTime->SetAttribute ("Variance", DoubleValue (0.03));

}


PECServer::~PECServer()
{
}


void
PECServer::ScheduleNextPacket()
{
	if ( m_firstTime && m_inServer ) {

		m_sendEvent = Simulator::Schedule( Seconds( double( 0.001 ) ), &PECServer::SendPacket, this );
		m_firstTime = false;

	}/* else if ( !m_sendEvent.IsRunning() ) {

		m_sendEvent = Simulator::Schedule( m_txInterval, &PECServer::SendPacket, this );

	}*/
}


void
PECServer::SetRetxTimer( Time retxTimer )
{

	// Do not restranmit lost packets, if set to 0
	if ( m_doRetransmission == 1 ) {

		m_retxTimer = retxTimer;

		if ( m_retxEvent.IsRunning() ) {

			//m_retxEvent.Cancel (); // cancel any scheduled cleanup events
			Simulator::Remove( m_retxEvent ); // slower, but better for memory
		}

		// Schedule even with new timeout
		m_retxEvent = Simulator::Schedule( m_retxTimer, &PECServer::CheckRetxTimeout, this );
	}
}


Time
PECServer::GetRetxTimer() const
{
	return m_retxTimer;
}

void
PECServer::CheckRetxTimeout()
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

	m_retxEvent = Simulator::Schedule( m_retxTimer, &PECServer::CheckRetxTimeout, this );
}



// Application methods
void
PECServer::StartApplication()
{
	NS_LOG_FUNCTION_NOARGS();
	App::StartApplication();
        //std::cout<<m_prefix<<std::endl;
        m_prefixWithoutSequence = m_prefix;
	Name servicePrefix = m_prefix.getSubName(0,1);
	servicePrefix.append("service");
        Name basePrefix = m_prefix.getSubName(0,1);
	basePrefix.append("baseQuery");
	Name computePrefix = m_prefix.getSubName(0,1);
	computePrefix.append("compute");
	computePrefix.append(m_prefix.getSubName(1,1).toUri());
        FibHelper::AddRoute(GetNode(), servicePrefix, m_face, 0);
        FibHelper::AddRoute(GetNode(), basePrefix , m_face, 0);
	FibHelper::AddRoute(GetNode(), computePrefix, m_face, 0);
        m_utilization = rand()%m_uRange+m_uMin;

        std::string server = m_interestName.getSubName(2,1).toUri();
        m_serverUpdate(GetNode()->GetId(), server, m_utilization);

        if(!m_inServer)Simulator::Schedule( m_changeInterval, &PECServer::SwitchStatus, this );
	ScheduleNextPacket();
}

void
PECServer::StopApplication() // Called at time specified by Stop
{
	NS_LOG_FUNCTION_NOARGS();
	Simulator::Cancel( m_sendEvent );
	App::StopApplication();
}

void
PECServer::SwitchStatus() {
   int change = rand()%10000;
   if(change >= 6500)
      accepting = !accepting;
   if(!m_inServer)Simulator::Schedule( m_changeInterval, &PECServer::SwitchStatus, this );
}

void
PECServer::SendPacket()
{

          std::string server = m_interestName.getSubName(2,1).toUri() + m_interestName.getSubName(3,1).toUri().substr(1);

	// Set default size for payload interets
	if (  m_virtualPayloadSize == 0 ) {
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
       	//uint8_t payload[1] = {1};
        std::string payload = m_interestName.getSubName(2,1).toUri() + m_interestName.getSubName(3,1).toUri().substr(1);
        double promUtil = m_utilization;
        for (auto i : pendingUtil) {
           promUtil += i.second;
        }

        payload +=  ","+std::to_string(int(promUtil));
        m_serverUpdate(GetNode()->GetId(), server, promUtil);

        std::vector<std::string> services = SplitString(m_services, ' ');
	for (uint32_t i = 0; i < services.size(); i++ ){
		payload +=  ","+services[i];	
	}
	
	shared_ptr<Name> nameWithSequence = make_shared<Name>( m_interestName );

	shared_ptr<Interest> interest = make_shared<Interest>();
	interest->setNonce( m_rand->GetValue( 0, std::numeric_limits<uint32_t>::max() ) );
	interest->setSubscription( 0 );
	nameWithSequence->appendSequenceNumber( seq );

	if ( m_available == 0 ) {
		m_available = 1;
		//Add back
                //payload = "remove";
	}
        else 
		m_available = 0;

        std::vector<uint8_t> myVector( payload.begin(), payload.end() );
        uint8_t *p = &myVector[0];
        interest->setPayload( p, myVector.size()); // Add payload to interest


	interest->setName( *nameWithSequence );
	time::milliseconds interestLifeTime( m_interestLifeTime.GetMilliSeconds() );
	interest->setInterestLifetime( interestLifeTime );
	if(m_inServer == 0){
		interest->setHopLimit(1);
	}
	else interest->setHopLimit(2);

	NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) > sending Interest: " << interest->getName() /*m_interestName*/ << " with Payload = " << interest->getPayloadLength() << "bytes" );

	WillSendOutInterest( seq );

	m_transmittedInterests( interest, this, m_face );
	m_appLink->onReceiveInterest( *interest );

	// Callback for sent payload interests
	m_sentInterest( GetNode()->GetId(), interest );
        //std::cout<<interest->getName()<<" "<<Simulator::Now().GetSeconds()<<std::endl;
	//ScheduleNextPacket();
}


void
PECServer::SendInputRequest(Name clientName, int packetsLeft)
{
  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (seq == std::numeric_limits<uint32_t>::max()) {
    if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return; // we are totally done
      }
    }

    seq = m_seq++;
  }

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(clientName);
  nameWithSequence->appendSequenceNumber(seq);
  //

  // shared_ptr<Interest> interest = make_shared<Interest> ();
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setCanBePrefix(false);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest for " << seq);

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  packetsLeft--;
  pendingInput[clientName]++;
  if(packetsLeft != 0){
     Simulator::Schedule(Seconds(double(0.001)), &PECServer::SendInputRequest, this, clientName, packetsLeft);
  }

}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
PECServer::OnData( shared_ptr<const Data> data )
{

	if ( !m_active ) {
		return;
	}

	App::OnData( data ); // tracing inside

	NS_LOG_FUNCTION( this << data );

	// This could be a problem......
	//uint32_t seq = data->getName().at( -1 ).toSequenceNumber();

	NS_LOG_INFO( "node( " << GetNode()->GetId() << " ) < Received DATA for " << data->getName() << " TIME: " << Simulator::Now() );

	// Callback for received subscription data
	m_receivedData( GetNode()->GetId(), data );
        if(data->getName().getSubName(1,1).toUri()=="/input"){
            Name clientName = data->getName().getSubName(0,3);
	    pendingInput[clientName]--;
	    if(pendingInput[clientName]<=0){
               pendingInput.erase(clientName);
               Name dName = inputMap[clientName].getSubName(0,  inputMap[clientName].size()-1) ;
               dName.append("obtain");
               dName.append(inputMap[clientName].getSubName(-1, 1));
               inputMap.erase(clientName);
               pendingData[dName] = 0;  
               ScheduleComputeTime(dName);
	    }
	}

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
PECServer::OnTimeout( uint32_t sequenceNumber )
{
	//NS_LOG_FUNCTION( sequenceNumber );
	//std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
	//m_rtt->RetransmitTimeout ().ToDouble ( Time::S ) << "s\n";

	m_rtt->IncreaseMultiplier(); // Double the next RTO
	m_rtt->SentSeq( SequenceNumber32( sequenceNumber ), 1 ); // make sure to disable RTT calculation for this sample
	m_retxSeqs.insert( sequenceNumber );
	//ScheduleNextPacket();
}

void
PECServer::OnInterest(shared_ptr<const Interest> interest)
{
    App::OnInterest(interest); // tracing inside

    NS_LOG_FUNCTION(this << interest);
    if(!accepting)
      return;



    // Callback for received interests
    m_receivedInterest(GetNode()->GetId(), interest);
    std::string payload = "";
    bool sendManifest = false;
    if (interest->getName().getSubName(1,1) == "/compute" && interest->getName().getSubName(-1,1)!="obtain"){
        
       /*if(interest->getName().getSubName(3,1) == "/execute"){
	  pendingUtil.erase(interest->getName().getSubName(2,1).toUri());
          Name dName = interest->getName().toUri();
          ScheduleComputeTime(dName);
	  return;
       }
       else{*/ 
          double util = rand()%m_uRaiseRange+(m_uRaise-m_uRaiseRange/2);
	  pendingUtil[interest->getName().getSubName(2,1).toUri()] = util;

          std::string server = m_interestName.getSubName(2,1).toUri()  + m_interestName.getSubName(3,1).toUri().substr(1);
	  double promUtil = m_utilization;
          for (auto i : pendingUtil) {
             promUtil += i.second;
          }
          m_serverUpdate(GetNode()->GetId(), server, promUtil);
          Name cname = "prefix/input/";
	  cname.append(interest->getName().getSubName(-2,1));
	  pendingInput[cname] = 0;
	  inputMap[cname] =  interest->getName().toUri();
	  Simulator::Schedule(Seconds(double(0.001)), &PECServer::SendInputRequest, this, cname, 8);

          //sendManifest = true;
          //return; 
	  double BCT = std::max((double)0, m_comTime->GetValue());
	  double computeTime = BCT * (m_cr+m_utilization/100);
          payload = std::to_string(computeTime);
       //}
    }
    else if(interest->getName().getSubName(1,1) == "/baseQuery"){
	SendPacket();
    }
    if(interest->getName().getSubName(-11,1) == "/obtain"){
       if(pendingData.find(interest->getName()) == pendingData.end()) return;
       if(pendingData[interest->getName()] == 0 || pendingData[interest->getName()] == 1 ){
          pendingData[interest->getName()] = 1;
          return;
       }
       if(pendingData[interest->getName()] == 2){
          pendingData.erase(interest->getName());
       }
    }
    if (!m_active)
        return;

    auto data = make_shared<Data>();
    data->setName(interest->getName());
    data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

    std::string serverInfo = "";
    if(sendManifest){
       serverInfo += "5";
    }
    else{
       serverInfo += m_interestName.getSubName(2,1).toUri() + m_interestName.getSubName(3,1).toUri().substr(1);
       double promUtil = m_utilization;
       for (auto i : pendingUtil) {
         promUtil += i.second; 
       }

       serverInfo += ","+std::to_string(int(promUtil));
       std::vector<std::string> services = SplitString(m_services, ' ');
       for (uint32_t i= 0; i < services.size(); i++ ){
          serverInfo +=  ","+services[i];
       }
    }
    if(interest->getName().getSubName(1,1) == "/baseQuery")
    data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
 
    else if (interest->getName().getSubName(1,1) == "/compute"){
       std::vector<uint8_t> myVector( payload.begin(), payload.end() );
       uint8_t *p = &myVector[0];
       data->setContent( p, myVector.size()); // Add payload to interest
    }
    else{
       std::vector<uint8_t> myVector( serverInfo.begin(), serverInfo.end() );  
       uint8_t *p = &myVector[0];
       data->setContent( p, myVector.size()); // Add payload to interest
    }
    Signature signature;
    SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

    if (m_keyLocator.size() > 0) {
        signatureInfo.setKeyLocator(m_keyLocator);
    }

    signature.setInfo(signatureInfo);
    signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

    data->setSignature(signature);

    NS_LOG_INFO("node(" << GetNode()->GetId() << ") sending DATA for " << data->getName() << " TIME: " << Simulator::Now());

    // to create real wire encoding
    data->wireEncode();

    m_transmittedDatas(data, this, m_face);
    m_appLink->onReceiveData(*data);

    // Callback for tranmitted subscription data
    m_sentData(GetNode()->GetId(), data);

}

void
PECServer::ScheduleComputeTime(const Name &dataName){
  //get sompute time
  double BCT = std::max((double)0, m_comTime->GetValue());
  double computeTime = BCT * (m_cr+m_utilization/100);
  //std::cout<<"Base time: "<<BCT<<" Real time: "<<computeTime<<std::endl;
  double util = rand()%m_uRaiseRange+(m_uRaise-m_uRaiseRange/2);
  //check if there is enough utilization for request
  if( (m_utilization + util) > 100.0){
     pendingRequests.push_back(dataName);
     return;
  }
  m_utilization += util;
  double promUtil = m_utilization;
  for (auto i : pendingUtil) {
     promUtil += i.second;
  }

  std::string server = m_interestName.getSubName(2,1).toUri()  + m_interestName.getSubName(3,1).toUri().substr(1);
  if(!accepting){
     m_serverUpdate(GetNode()->GetId(), server, 1000);
  }
  else m_serverUpdate(GetNode()->GetId(), server, promUtil);

  Simulator::Schedule(Seconds(computeTime), &PECServer::SendData, this, dataName, util);
}


void
PECServer::SendData(const Name &dataName, double util)
{
    if (!m_active)
        return;

    if(pendingData[dataName] == 0){
       pendingData[dataName] = 2;
       return;
    }
    else if(pendingData[dataName] == 2) return;

    m_utilization -= util;
    if(!pendingRequests.empty()){
       ScheduleComputeTime( pendingRequests.front());
       pendingRequests.erase(pendingRequests.begin());     
    }

    auto data = make_shared<Data>();
    data->setName(dataName);
    data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

    data->setContent( make_shared< ::ndn::Buffer>(1024)); // Add payload to interest

    Signature signature;
    SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

    if (m_keyLocator.size() > 0) {
        signatureInfo.setKeyLocator(m_keyLocator);
    }

    signature.setInfo(signatureInfo);
    signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

    data->setSignature(signature);

    NS_LOG_INFO("node(" << GetNode()->GetId() << ") sending DATA for " << data->getName() << " TIME: " << Simulator::Now());
    // to create real wire encoding
    data->wireEncode();

    m_transmittedDatas(data, this, m_face);
    m_appLink->onReceiveData(*data);

    // Callback for tranmitted subscription data
    m_sentData(GetNode()->GetId(), data);

    std::string server = m_interestName.getSubName(2,1).toUri() + m_interestName.getSubName(3,1).toUri().substr(1);
  if(!accepting){
     m_serverUpdate(GetNode()->GetId(), server, 1000);
  }
  else m_serverUpdate(GetNode()->GetId(), server, m_utilization);

}


void
PECServer::WillSendOutInterest( uint32_t sequenceNumber )
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


} // namespace ndn
} // namespace ns3

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

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

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
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&BaseStation::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&BaseStation::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&BaseStation::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&BaseStation::m_freshness),
                    MakeTimeChecker())
      .AddAttribute("Frequency", "Frequency of data packets, if 0, then no spontaneous publish",
                    TimeValue(Seconds(5)), MakeTimeAccessor(&BaseStation::m_frequency),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&BaseStation::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&BaseStation::m_keyLocator), MakeNameChecker())

      .AddTraceSource("SentData", "SentData",
                      MakeTraceSourceAccessor(&BaseStation::m_sentData),
                      "ns3::ndn::BaseStation::SentDataTraceCallback")

      .AddTraceSource("ReceivedInterest", "ReceivedInterest",
                      MakeTraceSourceAccessor(&BaseStation::m_receivedInterest),
                      "ns3::ndn::BaseStation::ReceivedInterestTraceCallback");

  return tid;
}

BaseStation::BaseStation()
  :m_firstTime(true)
  , m_subscription(0)
  , m_receivedpayload(0)
  , m_subDataSize (1)
{
    NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
BaseStation::StartApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    App::StartApplication();

    m_prefixWithoutSequence = m_prefix; //store prefix without sequence using to be used in log output

    FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);

    SendTimeout();
}

void
BaseStation::StopApplication()
{
    NS_LOG_FUNCTION_NOARGS();
    App::StopApplication();
}

void
BaseStation::OnInterest(shared_ptr<const Interest> interest)
{
    NS_LOG_INFO("SUBSCRIPTION value = " << interest->getSubscription() << " & PAYLOAD = " << interest->getPayloadLength() << " TIME: " << Simulator::Now());
    App::OnInterest(interest); // tracing inside

    NS_LOG_FUNCTION(this << interest);

    // Callback for received interests
    m_receivedInterest(GetNode()->GetId(), interest);

    if (!m_active)
        return;

    std::string server = interest->getName().getSubName(1,1).toUri();
    //Send data if there's a subscription
    m_subscription = interest->getSubscription();
    m_receivedpayload = interest->getPayloadLength();

    std::vector<uint8_t> payloadVector( &interest->getPayload()[0], &interest->getPayload()[interest->getPayloadLength()] );
    std::string payload( payloadVector.begin(), payloadVector.end() );
    //std::cout<<"At the base " << payload <<" "<<server<<std::endl;
    if(payload == "add") 
	    servers.insert(server);
    else 
	    servers.erase(server);
    
    for(auto iter = servers.begin(); iter != servers.end(); ++iter){
        std::cout << (*iter) << ", " ;
    }
    std::cout<<std::endl;

    m_prefix = interest->getName();

    //Normal interest, without a subscription
    if (m_subscription == 0) {
        SendData(m_prefix);
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
                Simulator::Schedule(Seconds(send_delay), &BaseStation::SendData, this, m_prefix);
                send_delay = send_delay + 0.03;
            }
        }
    }

    if(m_frequency != 0) {
        m_txEvent = Simulator::Schedule(m_frequency, &BaseStation::SendTimeout, this);
    }
}

void
BaseStation::SendData(const Name &dataName)
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
    data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

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

    m_transmittedDatas(data, this, m_face);
    m_appLink->onReceiveData(*data);

    // Callback for tranmitted subscription data
    m_sentData(GetNode()->GetId(), data);

    //std::cout << "APP use count " << data.use_count () << std::endl;
    //m_appLink->DanFree();
}

} // namespace ndn
} // namespace ns3

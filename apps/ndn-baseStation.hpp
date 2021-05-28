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

#ifndef NDN_QOS_PRODUCER_H
#define NDN_QOS_PRODUCER_H

#include "ndn-app.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include <unordered_set>
#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include "ns3/ndnSIM/utils/ndn-rtt-estimator.hpp"
#include "ns3/random-variable-stream.h"

#include <set>
#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>


namespace ns3 {
namespace ndn {

/**
 * @ingroup ndnQoS
 * @brief A producer which can receive and process payloaded interests.
 *
 * When the producer receives a payloaded interest, it will only respond
 * with an ack. 
 *
 * It also has the ability to publish content at a given frequency to 
 * consmers that have subscribed to it. This published content will be 
 * sent out to consumers without needing a corresponding interest.
 */
class BaseStation : public App {
public:
  static TypeId
  GetTypeId(void);

  /// Default constructor for the class
  BaseStation();

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest);

  /**
   * @brief Send data to subscribed nodes or send out ack.
   */
  void
  SendData(const Name &dataName, bool payload);

  void
  SendTimeout();

  void
  SendGathered();

  void
  updateFreshness(){
     isFresh = false;
  };

  void
  ScheduleNextPacket();

  void
  OnTimeout(uint32_t sequenceNumber);

  void
  OnData(shared_ptr<const Data> data );

  void
  WillSendOutInterest(uint32_t sequenceNumber);

  void
  SendPacket();

  void
  SendToInServers();

  void
  CheckRetxTimeout();

  /**
   * \brief Modifies the frequency of checking the retransmission timeouts
   * \param retxTimer Timeout defining how frequent retransmission timeouts should be checked
   */
  void
  SetRetxTimer(Time retxTimer);

  /**
   * \brief Returns the frequency of checking the retransmission timeouts
   * \return Timeout defining how frequent retransmission timeouts should be checked
   */
  Time
  GetRetxTimer() const;


  std::vector<std::string>
  SplitString( std::string strLine, char delimiter );

public:
  typedef void (*ReceivedInterestTraceCallback)( uint32_t, shared_ptr<const Interest> );
  typedef void (*SentDataTraceCallback)( uint32_t, shared_ptr<const Data> );
  typedef void (*SentInterestTraceCallback)( uint32_t, shared_ptr<const Interest> );
  typedef void (*ReceivedDataTraceCallback)( uint32_t, shared_ptr<const Data> );


protected:
  // inherited from Application base class.
  virtual void
  StartApplication(); // Called at time specified by Start

  virtual void
  StopApplication(); // Called at time specified by Stop

private:
  Ptr<UniformRandomVariable> m_rand; ///< @brief nonce generator
  uint32_t m_seq;      ///< @brief currently requested sequence number
  uint32_t m_seqMax;
  EventId m_sendEvent; ///< @brief EventId of pending "send packet" event
  Time m_retxTimer;    ///< @brief Currently estimated retransmission timer
  EventId m_retxEvent; ///< @brief Event to check whether or not retransmission should be performed

  Time m_txInterval;
  Name m_interestName;  
  Name m_prefix;
  Name m_postfix;
  uint32_t m_virtualPayloadSize;
  Time m_freshness;
  Time m_qFresh;
  Time m_frequency;
  EventId m_txEvent;
  bool m_firstTime;
  uint32_t m_subscription;
  Name m_prefixWithoutSequence;
  size_t m_receivedpayload;
  size_t m_subDataSize; //Size of subscription data, in Kbytes
  std::unordered_map<std::string, std::string> servers;
  std::unordered_map<std::string, std::string> newServers; 
  std::vector<Name> inServers; 
  uint32_t m_proactive;
  Name m_keyLocator;
  uint32_t m_signature;
  uint32_t m_hoplimit;
  uint32_t m_offset; //random offset
  uint32_t m_doRetransmission; //retransmit lost interest packets if set to 1
  Time m_interestLifeTime;
  Ptr<RttEstimator> m_rtt; ///< @brief RTT estimator
  std::vector<Name> pending;

  bool isFresh = false;

protected:
  TracedCallback < uint32_t, shared_ptr<const Interest> > m_sentInterest;
  TracedCallback < uint32_t, shared_ptr<const Data> > m_receivedData;
  TracedCallback <  uint32_t, shared_ptr<const Interest> > m_receivedInterest;
  TracedCallback <  uint32_t, shared_ptr<const Data> > m_sentData;

 /// @cond include_hidden
  /**
   * \struct This struct contains sequence numbers of packets to be retransmitted
   */
  struct RetxSeqsContainer : public std::set<uint32_t> {
  };

  RetxSeqsContainer m_retxSeqs; ///< \brief ordered set of sequence numbers to be retransmitted

  /**
   * \struct This struct contains a pair of packet sequence number and its timeout
   */
  struct SeqTimeout {
    SeqTimeout(uint32_t _seq, Time _time)
      : seq(_seq)
      , time(_time)
    {
    }

    uint32_t seq;
    Time time;
  };
  /// @endcond

  /// @cond include_hidden
  class i_seq {
  };
  class i_timestamp {
  };
  /// @endcond

  /// @cond include_hidden
  /**
   * \struct This struct contains a multi-index for the set of SeqTimeout structs
   */
  struct SeqTimeoutsContainer
    : public boost::multi_index::
        multi_index_container<SeqTimeout,
                              boost::multi_index::
                                indexed_by<boost::multi_index::
                                             ordered_unique<boost::multi_index::tag<i_seq>,
                                                            boost::multi_index::
                                                              member<SeqTimeout, uint32_t,
                                                                     &SeqTimeout::seq>>,
                                           boost::multi_index::
                                             ordered_non_unique<boost::multi_index::
                                                                  tag<i_timestamp>,
                                                                boost::multi_index::
                                                                  member<SeqTimeout, Time,
                                                                         &SeqTimeout::time>>>> {
  };

  SeqTimeoutsContainer m_seqTimeouts; ///< \brief multi-index for the set of SeqTimeout structs

  SeqTimeoutsContainer m_seqLastDelay;
  SeqTimeoutsContainer m_seqFullDelay;
  std::map<uint32_t, uint32_t> m_seqRetxCounts;

  TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */, Time /* delay */, int32_t /*hop count*/>
    m_lastRetransmittedInterestDataDelay;
  TracedCallback<Ptr<App> /* app */, uint32_t /* seqno */, Time /* delay */,
                 uint32_t /*retx count*/, int32_t /*hop count*/> m_firstInterestDataDelay;

  /// @endcond

};

} // namespace ndn
} // namespace ns3

#endif // NDN_PRODUCER_H

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

// ndn-simple.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +--------+     1Mbps      +----------+
 *      | consumer | <------------> | router | <------------> | producer |
 *      +----------+         10ms   +--------+          10ms  +----------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
 */

void SentInterestCallback( uint32_t, shared_ptr<const ndn::Interest> );

void ReceivedDataCallback( uint32_t, shared_ptr<const ndn::Data> );

void ReceivedInterestCallback( uint32_t, shared_ptr<const ndn::Interest> );

void ServerChoiceCallback( uint32_t nodeid, std::string serverChoice, int serverUtil);

void ServerUpdateCallback( uint32_t nodeid, std::string server, int serverUtil);


std::ofstream tracefile;
std::ofstream tracefile1;


int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  //Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  //Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("20p"));

  int run = 0;
  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.AddValue("Run", "Run", run);
  cmd.Parse(argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(15);

  srand( run );
  std::cout<<run<<std::endl;
  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute( "DataRate", StringValue( "1Mbps" ) );
  char temp[10];
  sprintf( temp, "%dms", (rand()%2+9 ));
  p2p.SetChannelAttribute( "Delay", StringValue( temp ) );
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(6), nodes.Get(1));
  p2p.Install(nodes.Get(7), nodes.Get(1));
  p2p.Install(nodes.Get(8), nodes.Get(1));
  p2p.Install(nodes.Get(9), nodes.Get(1));
  p2p.Install(nodes.Get(10), nodes.Get(1));
  p2p.Install(nodes.Get(11), nodes.Get(1));
  p2p.Install(nodes.Get(12), nodes.Get(1));
  p2p.Install(nodes.Get(13), nodes.Get(1));
  p2p.Install(nodes.Get(14), nodes.Get(1));
//  p2p.Install(nodes.Get(15), nodes.Get(20));
//  p2p.Install(nodes.Get(16), nodes.Get(20));

  sprintf( temp, "%dms", ( ( rand()%2+24 ) ) ); 
  p2p.SetChannelAttribute( "Delay", StringValue( temp ) );
  p2p.Install(nodes.Get(0), nodes.Get(2));
  p2p.Install(nodes.Get(6), nodes.Get(2));
  p2p.Install(nodes.Get(7), nodes.Get(2));
  p2p.Install(nodes.Get(8), nodes.Get(2));
  p2p.Install(nodes.Get(9), nodes.Get(2));
  p2p.Install(nodes.Get(10), nodes.Get(2));
  p2p.Install(nodes.Get(11), nodes.Get(3));
  p2p.Install(nodes.Get(12), nodes.Get(3));
  p2p.Install(nodes.Get(13), nodes.Get(3));
  p2p.Install(nodes.Get(14), nodes.Get(3));
//  p2p.Install(nodes.Get(15), nodes.Get(3));
//  p2p.Install(nodes.Get(16), nodes.Get(3));

  sprintf( temp, "%dms", ( ( rand()%2+1 ) ) );
  p2p.SetChannelAttribute( "Delay", StringValue( temp ) );
  p2p.Install(nodes.Get(0), nodes.Get(3));
  p2p.Install(nodes.Get(6), nodes.Get(3));
  p2p.Install(nodes.Get(7), nodes.Get(3));
  p2p.Install(nodes.Get(8), nodes.Get(3));
  p2p.Install(nodes.Get(9), nodes.Get(3));
  p2p.Install(nodes.Get(10), nodes.Get(3));
  p2p.Install(nodes.Get(11), nodes.Get(4));
  p2p.Install(nodes.Get(12), nodes.Get(4));
  p2p.Install(nodes.Get(13), nodes.Get(4));
  p2p.Install(nodes.Get(14), nodes.Get(4));
//  p2p.Install(nodes.Get(15), nodes.Get(4));
//  p2p.Install(nodes.Get(16), nodes.Get(4));

  /*sprintf( temp, "%dms", ( ( rand()%2+14 ) ) );
  p2p.SetChannelAttribute( "Delay", StringValue( temp ) );
  p2p.Install(nodes.Get(0), nodes.Get(4));
  p2p.Install(nodes.Get(6), nodes.Get(4));
  p2p.Install(nodes.Get(7), nodes.Get(4));
  p2p.Install(nodes.Get(8), nodes.Get(4));
  p2p.Install(nodes.Get(9), nodes.Get(4));
  p2p.Install(nodes.Get(10), nodes.Get(4));
  p2p.Install(nodes.Get(11), nodes.Get(5));
  p2p.Install(nodes.Get(12), nodes.Get(5));
  p2p.Install(nodes.Get(13), nodes.Get(5));
  p2p.Install(nodes.Get(14), nodes.Get(5));
  //p2p.Install(nodes.Get(15), nodes.Get(5));
  //p2p.Install(nodes.Get(16), nodes.Get(5));
  */

  p2p.SetChannelAttribute( "Delay", StringValue( "10ms" ) );
  p2p.Install(nodes.Get(1), nodes.Get(2));
  p2p.Install(nodes.Get(1), nodes.Get(3));
  p2p.Install(nodes.Get(1), nodes.Get(4));
  p2p.Install(nodes.Get(1), nodes.Get(5));
  //p2p.Install(nodes.Get(1), nodes.Get(19));
  //p2p.Install(nodes.Get(20), nodes.Get(19));
  //p2p.Install(nodes.Get(20), nodes.Get(17));
  //p2p.Install(nodes.Get(20), nodes.Get(18));
  //p2p.Install(nodes.Get(20), nodes.Get(5));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::Install(nodes.Get( 0 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 6 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 7 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 8 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 9 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 10 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 11 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 12 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 13 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  ndn::StrategyChoiceHelper::Install(nodes.Get( 14 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  //ndn::StrategyChoiceHelper::Install(nodes.Get( 15 ),"/prefix/service", "/localhost/nfd/strategy/intel");
  //ndn::StrategyChoiceHelper::Install(nodes.Get( 16 ),"/prefix/service", "/localhost/nfd/strategy/intel");

  ndn::StrategyChoiceHelper::InstallAll( "prefix/update", "/localhost/nfd/strategy/multicast" );

  // Installing applications

   ndn::AppHelper consumerHelper("ns3::ndn::IntelConsumer");
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue(".1")); // 10 interests a second
  consumerHelper.SetAttribute( "PayloadSize", StringValue( "200" ) );
  consumerHelper.SetAttribute( "RetransmitPackets", IntegerValue( 0 ) );
  consumerHelper.SetAttribute( "Offset", IntegerValue( 0 ) );
  consumerHelper.SetAttribute( "LifeTime", StringValue( "10s" ) );
  auto app = consumerHelper.Install(nodes.Get(0));      // first node
  app.Start(Seconds(0.2)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(6));      // first node
  app.Start(Seconds(0.3)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(7));      // first node
  app.Start(Seconds(0.4)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(8));      // first node
  app.Start(Seconds(0.5)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(9));      // first node
  app.Start(Seconds(0.6)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(10));      // first node
  app.Start(Seconds(0.7)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(11));      // first node
  app.Start(Seconds(0.8)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(12));      // first node
  app.Start(Seconds(0.9)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(13));      // first node
  app.Start(Seconds(1.0)); // stop the consumer app at 10 seconds mark
  app = consumerHelper.Install(nodes.Get(14));      // first node
  app.Start(Seconds(1.1)); // stop the consumer app at 10 seconds mark
  //app = consumerHelper.Install(nodes.Get(15));      // first node
  //app.Start(Seconds(1.2)); // stop the consumer app at 10 seconds mark
  //app = consumerHelper.Install(nodes.Get(16));      // first node
  //app.Start(Seconds(1.3)); // stop the consumer app at 10 seconds mark


  std::string strcallback;
  strcallback = "/NodeList/0/ApplicationList/*/SentInterest";
  Config::ConnectWithoutContext( strcallback, MakeCallback( &SentInterestCallback ) );
  strcallback = "/NodeList/0/ApplicationList/*/ReceivedData";
  Config::ConnectWithoutContext( strcallback, MakeCallback( & ReceivedDataCallback ) );
  strcallback = "/NodeList/0/ApplicationList/*/ServerChoice";
  Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerChoiceCallback ) );
  
  int i;
  for(i = 6; i<15; i++){
     strcallback =  "/NodeList/"+std::to_string(i)+"/ApplicationList/*/SentInterest";
     Config::ConnectWithoutContext( strcallback, MakeCallback( &SentInterestCallback ) );
     strcallback =  "/NodeList/"+std::to_string(i)+"/ApplicationList/*/ReceivedData";
     Config::ConnectWithoutContext( strcallback, MakeCallback( & ReceivedDataCallback ) );
     strcallback =  "/NodeList/"+std::to_string(i)+"/ApplicationList/*/ServerChoice";
     Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerChoiceCallback ) );
  }

  
  // Consumer
  ndn::AppHelper serverHelper("ns3::ndn::PECServer");
  // Consumer will request /prefix/0, /prefix/1, ...
  serverHelper.SetPrefix("/prefix/server1");
  serverHelper.SetAttribute("UpdatePrefix",StringValue("/prefix/update/server1"));
  serverHelper.SetAttribute("Frequency", StringValue("1")); // update base station every second
  serverHelper.SetAttribute( "PayloadSize", StringValue( "200" ) );
  serverHelper.SetAttribute( "RetransmitPackets", IntegerValue( 0 ) );
  serverHelper.SetAttribute( "Offset", IntegerValue( 0 ) );
  serverHelper.SetAttribute( "LifeTime", StringValue( "10s" ) );
  serverHelper.Install(nodes.Get(2));      // first node
  serverHelper.SetPrefix("/prefix/server2");
  serverHelper.SetAttribute("UpdatePrefix",StringValue("/prefix/update/server2"));
  serverHelper.Install(nodes.Get(3));
  serverHelper.SetPrefix("/prefix/server3");
  serverHelper.SetAttribute("UpdatePrefix",StringValue("/prefix/update/server3"));
  serverHelper.Install(nodes.Get(4));
  serverHelper.SetPrefix("/prefix/server4");
  serverHelper.SetAttribute("UpdatePrefix",StringValue("/prefix/update/server4"));
  serverHelper.SetAttribute("UtilMin", IntegerValue(10));
  serverHelper.SetAttribute("UtilRange", IntegerValue(10));
  serverHelper.SetAttribute("UtilRise", IntegerValue(5));
  serverHelper.SetAttribute("UtilRiseRange", IntegerValue(5));
  serverHelper.Install(nodes.Get(5));
  //serverHelper.SetPrefix("/prefix/server5");
  //serverHelper.SetAttribute("UpdatePrefix",StringValue("/prefix/update/server5"));
  //serverHelper.Install(nodes.Get(17));
  //serverHelper.SetPrefix("/prefix/server6");
  //serverHelper.SetAttribute("UpdatePrefix",StringValue("/prefix/update/server6"));
  //serverHelper.Install(nodes.Get(18));


  strcallback = "/NodeList/2/ApplicationList/*/ServerUpdate";
  Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerUpdateCallback ) );
  strcallback = "/NodeList/3/ApplicationList/*/ServerUpdate";
  Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerUpdateCallback ) );
  strcallback = "/NodeList/4/ApplicationList/*/ServerUpdate";
  Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerUpdateCallback ) );
  strcallback = "/NodeList/5/ApplicationList/*/ServerUpdate";
  Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerUpdateCallback ) );
  //strcallback = "/NodeList/17/ApplicationList/*/ServerUpdate";
  //Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerUpdateCallback ) );
  //strcallback = "/NodeList/18/ApplicationList/*/ServerUpdate";
  //Config::ConnectWithoutContext( strcallback, MakeCallback( & ServerUpdateCallback ) );

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install( nodes );
  ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(2));
  ndnGlobalRoutingHelper.AddOrigin("prefix/compute/server1", nodes.Get(2));
  ndnGlobalRoutingHelper.AddOrigin("prefix/", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigin("prefix/compute/server2", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(4));
  ndnGlobalRoutingHelper.AddOrigin("prefix/compute/server3", nodes.Get(4));
  ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(5));	  
  ndnGlobalRoutingHelper.AddOrigin("prefix/compute/server4", nodes.Get(5));
  //ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(17));
  //ndnGlobalRoutingHelper.AddOrigin("prefix/compute/server5", nodes.Get(17));
  //ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(18));
  //ndnGlobalRoutingHelper.AddOrigin("prefix/compute/server6", nodes.Get(18));

  // Producer
  ndn::AppHelper baseStationHelper("ns3::ndn::BaseStation");
  // Producer will reply to all requests starting with /prefix
  baseStationHelper.SetPrefix("/prefix");
  baseStationHelper.SetAttribute("PayloadSize", StringValue("1024"));
  baseStationHelper.SetAttribute( "Frequency", StringValue( "1" ) );
  baseStationHelper.Install(nodes.Get(1)); // last node
  //baseStationHelper.Install(nodes.Get(20));

  ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(1));
  //ndnGlobalRoutingHelper.AddOrigin("prefix", nodes.Get(20));

  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();

  //Open trace file for writing
  char trace[100];
  sprintf( trace, "ndn-test-run%d.csv", run );

  tracefile.open( trace, std::ios::out );
  tracefile << "nodeid,event,name,time" << std::endl;

  sprintf( trace, "choice-test-run%d.csv", run );

  tracefile1.open( trace, std::ios::out );
  tracefile1 << "nodeid,event,server,util,time" << std::endl;

  Simulator::Stop(Seconds(1000));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}


void SentInterestCallback( uint32_t nodeid, shared_ptr<const ndn::Interest> interest){
  tracefile << nodeid << ",sent," << interest->getName() << "," << std::fixed << setprecision( 9 ) << 
	  ( Simulator::Now().GetNanoSeconds() )/1000000000.0 << std::endl;
}

void ReceivedDataCallback( uint32_t nodeid, shared_ptr<const ndn::Data> data){
  
	ndn::Name traceName = data->getName().getSubName(0,1);
  traceName.append("service");
  traceName.append(std::to_string(nodeid));
  uint32_t seq = data->getName().at( -1 ).toSequenceNumber();
  traceName.appendSequenceNumber(seq-1);

  tracefile << nodeid << ",received," << traceName << "," << std::fixed << setprecision( 9 ) << 
	  ( Simulator::Now().GetNanoSeconds() )/1000000000.0 << std::endl;
}


void ReceivedInterestCallback( uint32_t nodeid, shared_ptr<const ndn::Interest> interest ){
  tracefile << nodeid << ",compute," << interest->getName() << "," << std::fixed << setprecision( 9 ) <<
          ( Simulator::Now().GetNanoSeconds() )/1000000000.0 << std::endl;
}

void ServerChoiceCallback( uint32_t nodeid, std::string serverChoice, int serverUtil){
  tracefile1 << nodeid << ",choice," << serverChoice << "," << serverUtil << "," << std::fixed << setprecision( 9 ) <<
          ( Simulator::Now().GetNanoSeconds() )/1000000000.0 << std::endl;
}

void ServerUpdateCallback( uint32_t nodeid, std::string server, int serverUtil){
  tracefile1 << nodeid << ",update," << server << "," << serverUtil << "," << std::fixed << setprecision( 9 ) <<
          ( Simulator::Now().GetNanoSeconds() )/1000000000.0 << std::endl;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}

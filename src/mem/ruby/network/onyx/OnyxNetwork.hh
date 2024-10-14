/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef __MEM_RUBY_NETWORK_ONYX_0_ONYXNETWORK_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_ONYXNETWORK_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/network/Chain.hh"
#include "mem/ruby/network/fault_model/FaultModel.hh"
#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "params/OnyxNetwork.hh"

namespace gem5
{

namespace ruby
{
//for fault modeling simulation
class FaultModel;
//NetDest specifies the network destination of a Message
class NetDest;

namespace onyx
{
//We need these classes to create the network
class InterfaceModule; //Network Interface (NI)
class Switcher; //Router
//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
class Bus;
//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
class NetLink; //Network Link for forwarding flits
class NetBridge; //Network Bridge for heterogenous architectures
class AckLink; //Credit Link for flow-control information

//OnyxNetwork inherites from Network (in Network.hh)
class OnyxNetwork : public Chain
{
  public:
    typedef OnyxNetworkParams Params;
    //OnyxNetwork constructor and destructor
    OnyxNetwork(const Params &p); //constructor declaration
    ~OnyxNetwork() = default; //destructor declaration

    void init(); //initializing the network

    const char *onyxVersion = "3.0"; //use Heteroonyx

    // Configuration (set externally)

    // for 2D topology (getter functions for rows and cols of NoC)
    int getNumRows() const { return m_num_rows; }
    int getNumCols() { return m_num_cols; }
    int getNumLayers() { return m_num_layers; }

    // for network
    //get the flit size for NetworkInterface
    uint32_t getNiFlitSize() const { return m_ni_flit_size; }
    //how many buffers per data virtual channel (VC)
    uint32_t getBuffersPerDataVC() { return m_buffers_per_data_vc; }
    //how many buffers per control virtual channel (VC)
    uint32_t getBuffersPerCtrlVC() { return m_buffers_per_ctrl_vc; }
    //get the routing algorithm (XY, custom, or weight-table)
    int getRoutingAlgorithm() const { return m_routing_algorithm; }
    //check whether fault model is enabled
    bool isFaultModelEnabled() const { return m_enable_fault_model; }
    FaultModel* fault_model; //create an object of the class FaultModel


    // Internal configuration
    //check whether a Vnet is ordered
    //In an ordered vnet, the flit that was enqueued first, should be
    //sent first (FIFO).
    bool isVNetOrdered(int vnet) const { return m_ordered[vnet]; }
    //get the type of a vnet (control, data, etc.)
    VNET_type
    get_vnet_type(int vnet)
    {
        return m_vnet_type[vnet];
    }
    //get the number of routers
    int getNumRouters();
    //get the id of a router by NetworkInterface and vnet
    int get_router_id(int ni, int vnet);

    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    //get the number of busses
    int getNumBusses();
    //get the id of a bus by Router and vnet
    int get_bus_id(int ni, int vnet);
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&


    // Methods used by Topology to setup the network
    //create an external link from router to NI
    void makeExtOutLink(SwitchID src, NodeID dest, BasicLink* link,
                     std::vector<NetDest>& routing_table_entry);
    //create an external link from NI to router
    void makeExtInLink(NodeID src, SwitchID dest, BasicLink* link,
                    std::vector<NetDest>& routing_table_entry);
    //===================================================================
    //===================================================================
    //create a bus link from bus to NI
    void makeBusOutLink(SwitchID src, NodeID dest, BasicLink* link,
                     std::vector<NetDest>& routing_table_entry);
    //create a bus link from NI to bus
    void makeBusInLink(NodeID src, SwitchID dest, BasicLink* link,
                    std::vector<NetDest>& routing_table_entry);
    //===================================================================
    //===================================================================
    //create an internal link between two routers (from src_outport
    //to dst_inport)
    void makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                          std::vector<NetDest>& routing_table_entry,
                          PortDirection src_outport_dirn,
                          PortDirection dest_inport_dirn);

    bool functionalRead(Packet *pkt, WriteMask &mask);
    //! Function for performing a functional write. The return value
    //! indicates the number of messages that were written.
    uint32_t functionalWrite(Packet *pkt);

    // Stats
    void collateStats(); //for collating the OnyxNetwork stats
    void regStats(); //for registering the OnyxNetwork stats
    void resetStats(); //reset the OnyxNetwork stats
    //for printing a OnyxNetwork object
    void print(std::ostream& out) const;

    // increment counters
    //update the number of injected packets for a vnet
    void increment_injected_packets(int vnet) { m_packets_injected[vnet]++; }
    //update the number of received packets for a vnet
    void increment_received_packets(int vnet) { m_packets_received[vnet]++; }
    //increment the packet_network_latency for a vnet
    void
    increment_packet_network_latency(Tick latency, int vnet)
    {
        m_packet_network_latency[vnet] += latency;
    }
    //increment the packet_queuing_latency for a vnet
    void
    increment_packet_queueing_latency(Tick latency, int vnet)
    {
        m_packet_queueing_latency[vnet] += latency;
    }
    //update the number of injected flits for a vnet
    void increment_injected_flits(int vnet) { m_flits_injected[vnet]++; }
    //update the number of received flits for a vnet
    void increment_received_flits(int vnet) { m_flits_received[vnet]++; }
    //increment the flit_network_latency for a vnet
    void
    increment_flit_network_latency(Tick latency, int vnet)
    {
        m_flit_network_latency[vnet] += latency;
    }
    //increment the flit_queuing_latency for a vnet
    void
    increment_flit_queueing_latency(Tick latency, int vnet)
    {
        m_flit_queueing_latency[vnet] += latency;
    }

    //update the total number of hops in the network
    void
    increment_total_hops(int hops)
    {
        m_total_hops += hops;
    }

    //update traffic distribution based on a RouteInfo
    void update_traffic_distribution(RouteInfo route);
    //getting the id of the next packet
    int getNextPacketID() { return m_next_packet_id++; }

  protected:
    // Configuration
    int m_num_rows; //number of rows in the network
    int m_num_cols; //number of columns in the network
    int m_num_layers; //number of layers in the network
    uint32_t m_ni_flit_size; //flit size for NetworkInterface
    uint32_t m_max_vcs_per_vnet; //maximum number of vcs per vnet
    uint32_t m_buffers_per_ctrl_vc; //number of buffers per control vc
    uint32_t m_buffers_per_data_vc; //number of buffers per data vc
    //network routing algorithm (0:weight-based, 1:XY, 2:custom)
    int m_routing_algorithm;
    //for enabling fault model
    bool m_enable_fault_model;

    // Statistical variables
    //number of packets received
    statistics::Vector m_packets_received;
    //number of packets injected
    statistics::Vector m_packets_injected;
    //packet network latency
    statistics::Vector m_packet_network_latency;
    //packet queuing latency
    statistics::Vector m_packet_queueing_latency;

    //average packet vnet latency
    statistics::Formula m_avg_packet_vnet_latency;
    //average packet queuing latency for a vnet
    statistics::Formula m_avg_packet_vqueue_latency;
    //average packet network latency
    statistics::Formula m_avg_packet_network_latency;
    //average packet queuing latency
    statistics::Formula m_avg_packet_queueing_latency;
    //average packet latency
    statistics::Formula m_avg_packet_latency;

    //number of flits received
    statistics::Vector m_flits_received;
    //number of flits injected
    statistics::Vector m_flits_injected;
    //flit network latency
    statistics::Vector m_flit_network_latency;
    //flit queuing latency
    statistics::Vector m_flit_queueing_latency;

    //average flit vnet latency
    statistics::Formula m_avg_flit_vnet_latency;
    //average flit queuing latency for a vnet
    statistics::Formula m_avg_flit_vqueue_latency;
    //average flit network latency
    statistics::Formula m_avg_flit_network_latency;
    //average flit queuing latency
    statistics::Formula m_avg_flit_queueing_latency;
    //average flit latency
    statistics::Formula m_avg_flit_latency;

    //total utilization of ext_in (from NI to router) links
    statistics::Scalar m_total_ext_in_link_utilization;
    //total utilization of ext_out (from router to NI) links
    statistics::Scalar m_total_ext_out_link_utilization;
    //============================================================
    //============================================================
    //total utilization of bus_in (from NI to bus) links
    statistics::Scalar m_total_bus_in_link_utilization;
    //total utilization of bus_out (from bus to NI) links
    statistics::Scalar m_total_bus_out_link_utilization;
    //============================================================
    //============================================================
    //total utilization of internal (from router to router) links
    statistics::Scalar m_total_int_link_utilization;
    //average link utilization
    statistics::Scalar m_average_link_utilization;
    //average load of virtual channels (VC)
    statistics::Vector m_average_vc_load;
    //average external link utilization
    statistics::Scalar m_average_ext_link_utilization;
    //average internal link utilization
    statistics::Scalar m_average_int_link_utilization;
    //=====================================================
    //=====================================================
    //average bus link utilization
    statistics::Scalar m_average_bus_link_utilization;
    //=====================================================
    //=====================================================
    //total number of hops
    statistics::Scalar  m_total_hops;
    //average number of hops
    statistics::Formula m_avg_hops;

    //traffic distribution of data messages
    std::vector<std::vector<statistics::Scalar *>> m_data_traffic_distribution;
    //traffic distribution of control messages
    std::vector<std::vector<statistics::Scalar *>> m_ctrl_traffic_distribution;

  private:
    //OnyxNetwork constructor for instantiation
    OnyxNetwork(const OnyxNetwork& obj);
    OnyxNetwork& operator=(const OnyxNetwork& obj);

    std::vector<VNET_type > m_vnet_type; // type of vnet (control, data, etc.)
    std::vector<Switcher *> m_routers;   // All Routers in Network
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    std::vector<Bus *> m_busses;   // All Busses in Network
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    std::vector<NetLink *> m_networklinks; // All flit links in the network
    std::vector<NetBridge *> m_networkbridges; // All network bridges
    std::vector<AckLink *> m_creditlinks; // All credit links in the network
    std::vector<InterfaceModule *> m_nis;   // All NI's in Network
    int m_next_packet_id; // static vairable for packet id allocation
};

//for printing an OnyxNetwork object
inline std::ostream&
operator<<(std::ostream& out, const OnyxNetwork& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_ONYX_0_ONYXNETWORK_HH__

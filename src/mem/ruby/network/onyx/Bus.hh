/*
 * Copyright (c) 2020 Inria
 * Copyright (c) 2016 Georgia Institute of Technology
 * Copyright (c) 2008 Princeton University
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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_BUS_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_BUS_HH__

#include <iostream>
#include <memory>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/BasicRouter.hh"
#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "mem/ruby/network/onyx/BusCrossbar.hh"
#include "mem/ruby/network/onyx/OnyxNetwork.hh"
#include "mem/ruby/network/onyx/RoutingTable.hh"
#include "mem/ruby/network/onyx/BusSwitchManager.hh"
#include "mem/ruby/network/onyx/chunk.hh"
#include "params/OnyxBus.hh"

namespace gem5
{

namespace ruby
{

class FaultModel;

namespace onyx
{

class NetLink;
class AckLink;
class BusInport;
class BusOutport;

//class Bus inherites from both BasicBus and Consumer
class Bus : public BasicBus, public Consumer
{
  public:
    typedef OnyxBusParams Params;
    Bus(const Params &p); //Bus constructor

    ~Bus() = default; //Bus destructor

    //Loop through all InputUnits and call their wakeup()
    //Loop through all OutputUnits and call their wakeup()
    //Call SwitchAllocator's wakeup()
    //Call CrossbarSwitch's wakeup()
    //The bus's wakeup function is called whenever any of its modules
    //(InputUnit, OutputUnit, SwitchAllocator, CrossbarSwitch) have a
    //ready flit/credit to act upon this cycle.
    void wakeup();
    //for printing this Bus
    void print(std::ostream& out) const {};

    //calls the init function of BasicBus,
    //SwitchAllocator, and CrossbarSwitch
    void init();
    //add an inport to the bus
    void addInPort(PortDirection inport_dirn, NetLink *link,
                   AckLink *credit_link);
    //add an outport to the bus
    void addOutPort(PortDirection outport_dirn, NetLink *link,
                    std::vector<NetDest>& routing_table_entry,
                    int link_weight, AckLink *credit_link,
                    uint32_t consumerVcs);

    //get the latency of the bus in cycles
    Cycles get_pipe_stages(){ return m_latency; }
    //get the number of vcs for bus
    uint32_t get_num_vcs()       { return m_num_vcs; }
    //get the number of vnets for bus
    uint32_t get_num_vnets()     { return m_virtual_networks; }
    //get the number of vcs per vnet for bus
    uint32_t get_vc_per_vnet()   { return m_vc_per_vnet; }
    //get the number of bus inports
    int get_num_inports()   { return m_input_unit.size(); }
    //get the number of bus outports
    int get_num_outports()  { return m_output_unit.size(); }
    //get the id of the bus
    int get_id()            { return m_id; }

    //get the layer of a bus based on its id
    int get_bus_layer(int bus_id);

    //initialize the pointer to the OnyxNetwork
    void init_net_ptr(OnyxNetwork* net_ptr)
    {
        m_network_ptr = net_ptr;
    }

    //get the pointer to the OnyxNetwork
    OnyxNetwork* get_net_ptr()  { return m_network_ptr; }

    //get the InputUnit (inport) by the port number
    BusInport*
    getInputUnit(unsigned port)
    {
        //make sure the given port number is valid
        assert(port < m_input_unit.size());
        return m_input_unit[port].get();
    }

    //get the OutputUnit (outport) by the port number
    BusOutport*
    getOutputUnit(unsigned port)
    {
        //make sure the given port number is valid
        assert(port < m_output_unit.size());
        return m_output_unit[port].get();
    }

    //get the link bandwidth for the bus
    int getBitWidth() { return m_bit_width; }

    //get the direction of an outport
    PortDirection getOutportDirection(int outport);
    //get the direction of an inport
    PortDirection getInportDirection(int inport);

    //compute the route for the flit by having RouteInfo, inport, and PortDirection
    int route_compute(RouteInfo route, int inport, PortDirection direction);
    //This function grants the switch to an inport, so the flit could pass
    //the crossbar.
    void grant_switch(int inport, chunk *t_flit);
    //This function gives the bus, time cycles delay.
    void schedule_wakeup(Cycles time);

    //Getting the direction of a port as a string
    //(North, South, East, West)
    std::string getPortDirectionName(PortDirection direction);
    //For printing fault vector based on temperature.
    void printFaultVector(std::ostream& out);
    //For printing aggregate fault probability based on temperature.
    void printAggregateFaultProbability(std::ostream& out);

    //This function is for creating statistics for every bus
    //in the stats.txt file.
    void regStats();
    //This function collates the stats for the bus.
    void collateStats();
    //Resetting statistics for inports, crossbarSwitch, and switchAllocator.
    void resetStats();

    // For Fault Model:
    bool get_fault_vector(int temperature, float fault_vector[]) {
        return m_network_ptr->fault_model->fault_vector(m_id, temperature,
                                                        fault_vector);
    }
    bool get_aggregate_fault_probability(int temperature,
                                         float *aggregate_fault_prob) {
        return m_network_ptr->fault_model->fault_prob(m_id, temperature,
                                                      aggregate_fault_prob);
    }

    bool functionalRead(Packet *pkt, WriteMask &mask);

    //Getting the total number of functional writes for a packet.
    uint32_t functionalWrite(Packet *);

  private:
    //latency of this bus
    Cycles m_latency;
    //number of vnets, vcs, and vcs_per_vnet
    uint32_t m_virtual_networks, m_vc_per_vnet, m_num_vcs;
    //link bandwidth of the bus
    uint32_t m_bit_width;
    //pointer to the OnyxNetwork
    OnyxNetwork *m_network_ptr;

    //RoutingUnit of this bus
    RoutingTable routingUnit;
    //SwitchAllocator of this bus
    BusSwitchManager switchAllocator;
    //CrossbarSwitch of this bus
    BusCrossbar crossbarSwitch;

    //vector containing the bus inports
    std::vector<std::shared_ptr<BusInport>> m_input_unit;
    //vector containing the bus outports
    std::vector<std::shared_ptr<BusOutport>> m_output_unit;

    // Statistical variables required for power computations
    statistics::Scalar m_buffer_reads; //inport buffer_read activity
    statistics::Scalar m_buffer_writes; //inport buffer_write activity

    //input_arbiter activity of the SwitchAllocator
    statistics::Scalar m_sw_input_arbiter_activity;
    //output_arbiter activity of the SwitchAllocator
    statistics::Scalar m_sw_output_arbiter_activity;

    //crossbar activity of the CrossbarSwitch
    statistics::Scalar m_crossbar_activity;
};

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_ONYX_0_BUS_HH__

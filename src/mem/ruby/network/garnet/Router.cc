/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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


#include "mem/ruby/network/garnet/Router.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/CreditLink.hh"
#include "mem/ruby/network/garnet/GarnetNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/NetworkLink.hh"
#include "mem/ruby/network/garnet/OutputUnit.hh"

//=====================================
#include <iostream>
//=====================================

namespace gem5
{

namespace ruby
{

namespace garnet
{

//Router constructor
Router::Router(const Params &p)
  : BasicRouter(p), Consumer(this), m_latency(p.latency),
    m_virtual_networks(p.virt_nets), m_vc_per_vnet(p.vcs_per_vnet),
    m_num_vcs(m_virtual_networks * m_vc_per_vnet), m_bit_width(p.width),
    m_network_ptr(nullptr), routingUnit(this), switchAllocator(this),
    crossbarSwitch(this)
{
    m_input_unit.clear(); //clear the inports
    m_output_unit.clear(); //clear the outports
}

//calls the init function of BasicRouter, SwitchAllocator,
//and CrossbarSwitch
void
Router::init()
{
    BasicRouter::init();

    switchAllocator.init();
    crossbarSwitch.init();
}

//Loop through all InputUnits and call their wakeup()
//Loop through all OutputUnits and call their wakeup()
//Call SwitchAllocator's wakeup()
//Call CrossbarSwitch's wakeup()
//The router's wakeup function is called whenever any of its modules
//(InputUnit, OutputUnit, SwitchAllocator, CrossbarSwitch) have a
//ready flit/credit to act upon this cycle.
void
Router::wakeup()
{
    DPRINTF(RubyNetwork, "Router %d woke up\n", m_id);
    //ensure the router woke up on a clockEdge (the tick when a cycle begins)
    assert(clockEdge() == curTick());

    // check for incoming flits (wake up all the inports)
    for (int inport = 0; inport < m_input_unit.size(); inport++) {
        m_input_unit[inport]->wakeup();
    }

    // check for incoming credits (wake up all the outports)
    // Note: the credit update is happening before SA
    // buffer turnaround time =
    //     credit traversal (1-cycle) + SA (1-cycle) + Link Traversal (1-cycle)
    // if we want the credit update to take place after SA, this loop should
    // be moved after the SA request
    for (int outport = 0; outport < m_output_unit.size(); outport++) {
        m_output_unit[outport]->wakeup();
    }

    // Switch Allocation
    switchAllocator.wakeup();

    // Switch Traversal
    crossbarSwitch.wakeup();
}

//The following function of the Router class adds one input port or
//inport to the Router object.
void
Router::addInPort(PortDirection inport_dirn,
                  NetworkLink *in_link, CreditLink *credit_link)
{
    //======================================================
    // std::cout << "=================================================\n";
    // std::cout << "name of the link (from addInPort in Router.cc): " << in_link->name() <<"\n";
    // std::cout << "width of the link (from addInPort in Router.cc): " << in_link->bitWidth <<"\n";
    // std::cout << "id of the router (from addInPort in Router.cc): " << m_id <<"\n";
    // std::cout << "width of the router (from addInPort in Router.cc): " << m_bit_width <<"\n";
    // std::cout << "=================================================\n";
    //======================================================

    fatal_if(in_link->bitWidth != m_bit_width, "Widths of link %s(%d)does"
            " not match that of Router%d(%d). Consider inserting SerDes "
            "Units.", in_link->name(), in_link->bitWidth, m_id, m_bit_width);

    //port number of this inport
    //every time we push_back into vector, the size increases, and that is
    //the port number of our new inport
    int port_num = m_input_unit.size();

    //Defining an input port for this router object.
    //This refers to an object of the Router class in InputUnit
    //class instantiation. inport_dirn is the direction of the
    //input port that we are creating.
    InputUnit *input_unit = new InputUnit(port_num, inport_dirn, this);
    //set network link for this inport (to this router)
    input_unit->set_in_link(in_link);
    //set credit link for this inport (from this router to the adjacent one)
    input_unit->set_credit_link(credit_link);
    //the consumer of the network link is this router
    in_link->setLinkConsumer(this);
    //set the number of virtual channels per virtual network for the network link
    in_link->setVcsPerVnet(get_vc_per_vnet());
    //set the source queue for the credit link
    credit_link->setSourceQueue(input_unit->getCreditQueue(), this);
    //set the number of virtual channels per virtual network for the credit link
    credit_link->setVcsPerVnet(get_vc_per_vnet());
    //add the input port we created to the router object
    m_input_unit.push_back(std::shared_ptr<InputUnit>(input_unit));
    //add the new inport and its direction to the routingUnit
    routingUnit.addInDirection(inport_dirn, port_num);
}

//The following function of the Router class adds one output port or
//outport to the Router object.
void
Router::addOutPort(PortDirection outport_dirn,
                   NetworkLink *out_link,
                   std::vector<NetDest>& routing_table_entry, int link_weight,
                   CreditLink *credit_link, uint32_t consumerVcs)
{
    //======================================================
    // std::cout << "=================================================\n";
    // std::cout << "name of the link (from addOutPort in Router.cc): " << out_link->name() <<"\n";
    // std::cout << "width of the link (from addOutPort in Router.cc): " << out_link->bitWidth <<"\n";
    // std::cout << "id of the router (from addOutPort in Router.cc): " << m_id <<"\n";
    // std::cout << "width of the router (from addOutPort in Router.cc): " << m_bit_width <<"\n";
    // std::cout << "=================================================\n";
    //======================================================
    fatal_if(out_link->bitWidth != m_bit_width, "Widths of units do not match."
            " Consider inserting SerDes Units");

    //port number of this outport
    //every time we push_back into vector, the size increases, and that is
    //the port number of our new outport
    int port_num = m_output_unit.size();

    //Defining an output port for this router object.
    //outport_dirn is the direction of the output port that we
    //are creating. consumerVcs is the virtual channels that
    //would consume from each outport.
    OutputUnit *output_unit = new OutputUnit(port_num, outport_dirn, this,
                                             consumerVcs);
    //set network link for this outport (from this router to the adjacent one)
    output_unit->set_out_link(out_link);
    //set credit link for this outport (to this router)
    output_unit->set_credit_link(credit_link);
    //the consumer of the credit link is this router
    credit_link->setLinkConsumer(this);
    //set the number of virtual channels per virtual network for the credit link
    //These virtual channels are consumerVcs.
    credit_link->setVcsPerVnet(consumerVcs);
    //set the source queue for the network link
    out_link->setSourceQueue(output_unit->getOutQueue(), this);
    //set the number of virtual channels per virtual network for the network link
    out_link->setVcsPerVnet(consumerVcs);
    //add the output port we created to the router object
    m_output_unit.push_back(std::shared_ptr<OutputUnit>(output_unit));

    //add the route of the routing_table_entry to the routingUnit
    routingUnit.addRoute(routing_table_entry);
    //add the weight of the link to the routingUnit
    //This is for the network link, and since a link connects one
    //outport to an inport, by giving weight while creating an outport,
    //we cover the total links in the network.
    routingUnit.addWeight(link_weight);
    //add the new outport and its direction to the routingUnit
    routingUnit.addOutDirection(outport_dirn, port_num);
}

//Getting the direction of an outport in the router
PortDirection
Router::getOutportDirection(int outport)
{
    return m_output_unit[outport]->get_direction();
}

//Getting the direction of an inport in the router
PortDirection
Router::getInportDirection(int inport)
{
    return m_input_unit[inport]->get_direction();
}

//The following function of the Router class computes which outport should
//be chosen for the flits, based on route, inport, and inport direction.
int
Router::route_compute(RouteInfo route, int inport, PortDirection inport_dirn)
{
    return routingUnit.outportCompute(route, inport, inport_dirn);
}

//This function grants the switch to an inport, so the flit could pass
//the crossbar.
void
Router::grant_switch(int inport, flit *t_flit)
{
    crossbarSwitch.update_sw_winner(inport, t_flit);
}

//This function gives the router, time cycles delay.
void
Router::schedule_wakeup(Cycles time)
{
    // wake up after time cycles
    scheduleEvent(time);
}

//get the layer of a router based on its id
int
Router::get_router_layer(int router_id) {
    int num_rows = m_network_ptr->getNumRows();
    int num_cols = m_network_ptr->getNumCols();
    int num_layers = m_network_ptr->getNumLayers();
    assert(num_rows > 0 && num_cols > 0 && num_layers > 0);
    //number of routers or RLIs per layer
    int num_routers_layer = num_rows * num_cols;
    if (num_layers > 1) { return floor(router_id/num_routers_layer); }
    //return 0 if we only have one layer
    return 0;
}

//Getting the direction of a port as a string
//(North, South, East, West)
std::string
Router::getPortDirectionName(PortDirection direction)
{
    // PortDirection is actually a string
    // If not, then this function should add a switch
    // statement to convert direction to a string
    // that can be printed out
    return direction;
}

//This function is for creating statistics for every router
//in the stats.txt file.
void
Router::regStats()
{
    //call the regStats() function of the parent class
    BasicRouter::regStats();

    m_buffer_reads
        .name(name() + ".buffer_reads")
        .flags(statistics::nozero)
    ;

    m_buffer_writes
        .name(name() + ".buffer_writes")
        .flags(statistics::nozero)
    ;

    m_crossbar_activity
        .name(name() + ".crossbar_activity")
        .flags(statistics::nozero)
    ;

    m_sw_input_arbiter_activity
        .name(name() + ".sw_input_arbiter_activity")
        .flags(statistics::nozero)
    ;

    m_sw_output_arbiter_activity
        .name(name() + ".sw_output_arbiter_activity")
        .flags(statistics::nozero)
    ;
}

//This function collates the stats for the router.
void
Router::collateStats()
{
    for (int j = 0; j < m_virtual_networks; j++) {
        for (int i = 0; i < m_input_unit.size(); i++) {
            m_buffer_reads += m_input_unit[i]->get_buf_read_activity(j);
            m_buffer_writes += m_input_unit[i]->get_buf_write_activity(j);
        }
    }

    m_sw_input_arbiter_activity = switchAllocator.get_input_arbiter_activity();
    m_sw_output_arbiter_activity =
        switchAllocator.get_output_arbiter_activity();
    m_crossbar_activity = crossbarSwitch.get_crossbar_activity();
}

//Resetting statistics for inports, crossbarSwitch, and switchAllocator.
void
Router::resetStats()
{
    for (int i = 0; i < m_input_unit.size(); i++) {
            m_input_unit[i]->resetStats();
    }

    crossbarSwitch.resetStats();
    switchAllocator.resetStats();
}

//For printing fault vector based on temperature.
void
Router::printFaultVector(std::ostream& out)
{
    int temperature_celcius = BASELINE_TEMPERATURE_CELCIUS;
    int num_fault_types = m_network_ptr->fault_model->number_of_fault_types;
    float fault_vector[num_fault_types];
    get_fault_vector(temperature_celcius, fault_vector);
    out << "Router-" << m_id << " fault vector: " << std::endl;
    for (int fault_type_index = 0; fault_type_index < num_fault_types;
         fault_type_index++) {
        out << " - probability of (";
        out <<
        m_network_ptr->fault_model->fault_type_to_string(fault_type_index);
        out << ") = ";
        out << fault_vector[fault_type_index] << std::endl;
    }
}

//For printing aggregate fault probability based on temperature.
void
Router::printAggregateFaultProbability(std::ostream& out)
{
    int temperature_celcius = BASELINE_TEMPERATURE_CELCIUS;
    float aggregate_fault_prob;
    get_aggregate_fault_probability(temperature_celcius,
                                    &aggregate_fault_prob);
    out << "Router-" << m_id << " fault probability: ";
    out << aggregate_fault_prob << std::endl;
}

bool
Router::functionalRead(Packet *pkt, WriteMask &mask)
{
    bool read = false;
    if (crossbarSwitch.functionalRead(pkt, mask))
        read = true;

    for (uint32_t i = 0; i < m_input_unit.size(); i++) {
        if (m_input_unit[i]->functionalRead(pkt, mask))
            read = true;
    }

    for (uint32_t i = 0; i < m_output_unit.size(); i++) {
        if (m_output_unit[i]->functionalRead(pkt, mask))
            read = true;
    }

    return read;
}

//getting the total number of functional writes for a packet
uint32_t
Router::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    num_functional_writes += crossbarSwitch.functionalWrite(pkt);

    for (uint32_t i = 0; i < m_input_unit.size(); i++) {
        num_functional_writes += m_input_unit[i]->functionalWrite(pkt);
    }

    for (uint32_t i = 0; i < m_output_unit.size(); i++) {
        num_functional_writes += m_output_unit[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5

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


#include "mem/ruby/network/onyx/BusInport.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/onyx/Ack.hh"
#include "mem/ruby/network/onyx/Bus.hh"

//=====================================
#include <iostream>
//=====================================

namespace gem5
{

namespace ruby
{

namespace onyx
{

//BusInport constructor for instantiation
BusInport::BusInport(int id, PortDirection direction, Bus *bus)
  : Consumer(bus), m_bus(bus), m_id(id), m_direction(direction),
    m_vc_per_vnet(m_bus->get_vc_per_vnet())
{
    //number of VCs in this InputUnit
    const int m_num_vcs = m_bus->get_num_vcs();
    //buffer_reads=buffer_writes=(num_VCs/num_of_VCs_per_Vnet)
    //we have this many buffers in this InputUnit
    m_num_buffer_reads.resize(m_num_vcs/m_vc_per_vnet);
    m_num_buffer_writes.resize(m_num_vcs/m_vc_per_vnet);
    //set the buffer reads & writes activity to zero for all buffers
    for (int i = 0; i < m_num_buffer_reads.size(); i++) {
        m_num_buffer_reads[i] = 0;
        m_num_buffer_writes[i] = 0;
    }

    // Instantiating the virtual channels
    virtualChannels.reserve(m_num_vcs);
    for (int i=0; i < m_num_vcs; i++) {
        virtualChannels.emplace_back(); //initialize a VC object
    }
}

/*
 * The InputUnit wakeup function reads the input flit from its input link.
 * Each flit arrives with an input VC (VC identifier).
 * For HEAD/HEAD_TAIL flits, performs route computation,
 * and updates route in the input VC.
 * The flit is buffered for (m_latency - 1) cycles in the input VC
 * and marked as valid for SwitchAllocation starting that cycle.
 *
 */

void
BusInport::wakeup()
{
    chunk *t_flit; //define a flit
    //if the input link to the inport is ready at the current tick
    if (m_in_link->isReady(curTick())) {
        //update the flit with the link content
        t_flit = m_in_link->consumeLink();
        //printing what bus consumes from what link, link_width & flit
        DPRINTF(RubyNetwork, "Bus[%d] Consuming:%s Width: %d Flit:%s\n",
        m_bus->get_id(), m_in_link->name(),
        m_bus->getBitWidth(), *t_flit);
        //make sure the flit size is the same as the router link size
        assert(t_flit->m_width == m_bus->getBitWidth());
        //get the VC from the flit that just arrived (VC identifier)
        int vc = t_flit->get_vc();
        //for stats (number of hops the flit traveled so far)
        t_flit->increment_hops();

        //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
        //change the broadcast flag to one so the routers would know
        //the flit is coming from a bus
        t_flit->set_broadcast(1);
        //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

        //if our flit is of type HEAD_ or HEAD_TAIL_, then we need route
        //computation and must update the route in the VC
        if ((t_flit->get_type() == HEAD_) ||
            (t_flit->get_type() == HEAD_TAIL_)) {

            //make sure the VC_state of the VC the flit is in, is IDLE_
            assert(virtualChannels[vc].get_state() == IDLE_);
            //change the state of vc from IDLE_ to ACTIVE_ at
            //the current tick
            set_vc_active(vc, curTick());

        } else { //the flit is of type BODY/TAIL
            //make sure the VC_state of the VC the flit is in, is ACTIVE_
            //no need for route computation (the route is already set for vc)
            assert(virtualChannels[vc].get_state() == ACTIVE_);
        }


        // Buffer the flit (insert the flit into vc)
        virtualChannels[vc].insertFlit(t_flit);

        //determine the Vnet (vnet = vc_id/number_of_VCs_per_Vnet)
        int vnet = vc/m_vc_per_vnet;
        // number of writes same as reads
        // any flit that is written will be read only once
        m_num_buffer_writes[vnet]++;
        m_num_buffer_reads[vnet]++;

        //get the number of pipeline stages (router latency)
        Cycles pipe_stages = m_bus->get_pipe_stages();
        if (pipe_stages == 1) {
            // 1-cycle router
            // Flit goes for SA directly
            t_flit->advance_stage(SA_, curTick());
        } else {
            assert(pipe_stages > 1);
            // Router delay is modeled by making flit wait in buffer for
            // (pipe_stages cycles - 1) cycles before going for SA

            //wait_time before advancing to Switch_Allocation stage
            Cycles wait_time = pipe_stages - Cycles(1);
            //advance the flit stage to SA_ after m_latency-1 cycles,
            //so the router latency becomes m_latncy cycles
            t_flit->advance_stage(SA_, m_bus->clockEdge(wait_time));

            // Wakeup the router in that cycle to perform SA
            m_bus->schedule_wakeup(Cycles(wait_time));
        }

        //if the input link to the inport is ready at the current tick
        if (m_in_link->isReady(curTick())) {
            //Wakeup the router in one cycle to consume the flit
            m_bus->schedule_wakeup(Cycles(1));
        }
    }
}

// Send a credit back to upstream router for this VC.
// Called by SwitchAllocator when the flit in this VC wins the Switch.
//Each InputUnit (inport) has one credit_link, and the credits sent back,
//are for specific VCs, showing the upstream router the free space in each VC.
void
BusInport::increment_credit(int in_vc, bool free_signal, Tick curTime)
{
    //printing the router_id, inport VC, free_signal, and the credit_link
    DPRINTF(RubyNetwork, "Bus[%d]: Sending a credit vc:%d free:%d to %s\n",
    m_bus->get_id(), in_vc, free_signal, m_credit_link->name());
    //create a credit flit with the following info
    Ack *t_credit = new Ack(in_vc, free_signal, curTime);
    //insert the created credit flit into the creditQueue
    creditQueue.insert(t_credit);
    //the credit link of the InputUnit will send t_credit in one cycle
    m_credit_link->scheduleEventAbsolute(m_bus->clockEdge(Cycles(1)));
}

bool
BusInport::functionalRead(Packet *pkt, WriteMask &mask)
{
    bool read = false;
    for (auto& virtual_channel : virtualChannels) {
        if (virtual_channel.functionalRead(pkt, mask))
            read = true;
    }

    return read;
}

//updating InputUnit VC messages with the data from the packet
//It returns the number of functional writes.
uint32_t
BusInport::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    for (auto& virtual_channel : virtualChannels) {
        num_functional_writes += virtual_channel.functionalWrite(pkt);
    }

    return num_functional_writes;
}

//for resetting InputUnit statistics
void
BusInport::resetStats()
{
    //reset buffers read & write activity
    for (int j = 0; j < m_num_buffer_reads.size(); j++) {
        m_num_buffer_reads[j] = 0;
        m_num_buffer_writes[j] = 0;
    }
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

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


#include "mem/ruby/network/onyx/BusCrossbar.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/onyx/BusOutport.hh"
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

//BusCrossbar constructor for instantiation
BusCrossbar::BusCrossbar(Bus *bus)
  : Consumer(bus), m_bus(bus), m_num_vcs(m_bus->get_num_vcs()),
    m_crossbar_activity(0), switchBuffers(0)
{
}

//for resizing switchBuffers vector to the number of input ports (inports)
//We have a switchBuffer for every inport (west_in, east_in, south_in,
//north_in).
void
BusCrossbar::init()
{
    switchBuffers.resize(m_bus->get_num_inports());
}

/*
 * The wakeup function of the CrossbarSwitch loops through all input ports,
 * and sends the winning flit (from SA) out of its output port on to the
 * output link. The output link is scheduled for wakeup in the next cycle.
 */

void
BusCrossbar::wakeup()
{
    //print the time that the crossbar woke up
    DPRINTF(RubyNetwork, "BusCrossbarSwitch at Bus %d woke up "
            "at time: %lld\n",
            m_bus->get_id(), m_bus->curCycle());

    //loop through all the switchBuffers
    for (auto& switch_buffer : switchBuffers) {
        //if the switch_buffer doesn't have a ready flit at the
        //current_tick then move on
        if (!switch_buffer.isReady(curTick())) {
            continue;
        }

	//peek the top flit of the switch_buffer that has a ready flit
        chunk *t_flit = switch_buffer.peekTopFlit();
        //if the flit is in the Switch_Traversal pipeline stage
        if (t_flit->is_stage(ST_, curTick())) {
            //find out which outport the flit is going to
            int outport = t_flit->get_outport();

            // flit performs Link_Traversal in the next cycle
            //(advancing the pipeline stage: ST_ ==> LT_)
            // t_flit->advance_stage(LT_, m_bus->clockEdge(Cycles(1)));
            // t_flit->set_time(m_bus->clockEdge(Cycles(1)));
            t_flit->advance_stage(LT_, m_bus->clockEdge(Cycles(0)));
            t_flit->set_time(m_bus->clockEdge(Cycles(0)));

            // This will take care of waking up the Network Link
            // in the next cycle
            //insert the flit into its outport
            m_bus->getOutputUnit(outport)->insert_flit(t_flit);
            //get the top flit of the switch_buffer (1 place is freed)
            switch_buffer.getTopFlit();
            //increment the crossbar activity
            m_crossbar_activity++;
        }
    }
}

bool
BusCrossbar::functionalRead(Packet *pkt, WriteMask &mask)
{
    bool read = false;
    for (auto& switch_buffer : switchBuffers) {
        if (switch_buffer.functionalRead(pkt, mask))
            read = true;
   }
   return read;
}

//Function for figuring out if any of the messages in switchBuffer
//needs to be updated with the data from the packet.
//It returns the number of functional writes.
uint32_t
BusCrossbar::functionalWrite(Packet *pkt)
{
   uint32_t num_functional_writes = 0;

   for (auto& switch_buffer : switchBuffers) {
       num_functional_writes += switch_buffer.functionalWrite(pkt);
   }

   return num_functional_writes;
}

//for resetting CrossbarSwitch statistics
void
BusCrossbar::resetStats()
{
    m_crossbar_activity = 0;
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

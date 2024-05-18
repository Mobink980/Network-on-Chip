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


#include "mem/ruby/network/garnet/BusCrossbarSwitch.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/BusOutputUnit.hh"
#include "mem/ruby/network/garnet/Bus.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

//BusCrossbarSwitch constructor for instantiation
BusCrossbarSwitch::BusCrossbarSwitch(Bus *bus)
  : Consumer(bus), m_bus(bus), m_num_vcs(m_bus->get_num_vcs()),
    m_crossbar_activity(0), m_num_outports(m_bus->get_num_inports())
{
    switchBuffer = flitBuffer();
}

/*
 * The wakeup function of the CrossbarSwitch loops through all input ports,
 * and sends the winning flit (from SA) out of its output port on to the
 * output link. The output link is scheduled for wakeup in the next cycle.
 */

void
BusCrossbarSwitch::wakeup()
{
    //print the time that the crossbar woke up
    DPRINTF(RubyNetwork, "BusCrossbarSwitch at Bus %d woke up "
            "at time: %lld\n",
            m_bus->get_id(), m_bus->curCycle());

    //Ensure that switchBuffer has a flit in the current tick
    if (switchBuffer.isReady(curTick())) {
        //peek the top flit of the switchBuffer that has a ready flit
        flit *t_flit = switchBuffer.peekTopFlit();
        //if the flit is in the Switch_Traversal pipeline stage
        if (t_flit->is_stage(ST_, curTick())) {
            //===============================================
            // We broadcast the flit to all the outports
            //===============================================
            // flit performs Link_Traversal in the next cycle 
            //(advancing the pipeline stage: ST_ ==> LT_)
            t_flit->advance_stage(LT_, m_bus->clockEdge(Cycles(1)));
            t_flit->set_time(m_bus->clockEdge(Cycles(1)));

            // This will take care of waking up the Network Link
            // in the next cycle
            // Insert the flit into all of the outports.
            for(int outport = 0; outport < m_num_outports; outport++) {
                m_bus->getOutputUnit(outport)->insert_flit(t_flit);
            }
            
            //get the top flit of the switchBuffer (1 place is freed)
            switchBuffer.getTopFlit();
            //increment the crossbar activity
            m_crossbar_activity++;
        }
    }
}

bool
BusCrossbarSwitch::functionalRead(Packet *pkt, WriteMask &mask)
{
    bool read = false;
    if (switchBuffer.functionalRead(pkt, mask))
        read = true;

    return read;
}

//Function for figuring out if any of the messages in switchBuffer
//needs to be updated with the data from the packet.
//It returns the number of functional writes.
uint32_t
BusCrossbarSwitch::functionalWrite(Packet *pkt)
{
   uint32_t num_functional_writes = switchBuffer.functionalWrite(pkt);
   return num_functional_writes;
}

//for resetting BusCrossbarSwitch statistics
void
BusCrossbarSwitch::resetStats()
{
    m_crossbar_activity = 0;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5

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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_BUSCROSSBARSWITCH_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_BUSCROSSBARSWITCH_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/CommonTypes.hh"
#include "mem/ruby/network/garnet/flitBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

//BusCrossbarSwitch is part of the Bus
class Bus;

//BusCrossbarSwitch inherites from Consumer
class BusCrossbarSwitch : public Consumer
{
  public:
    BusCrossbarSwitch(Bus *bus); //constructor
    ~BusCrossbarSwitch() = default; //destructor
    //Loop through all input ports, and send the winning 
    //flit out of its output port onto the output link.
    void wakeup(); 
    //for resizing switchBuffers vector to the number of 
    //input ports (inports)
    void init(); 
    //printing the CrossbarSwitch
    void print(std::ostream& out) const {};

    //=========================================================
    //insert the flit t_flit into the switchBuffer 
    inline void
    update_sw_winner(flit *t_flit)
    {
        switchBuffer.insert(t_flit);
    }
    //=========================================================

    //returns the number of times the crossbar is used so far
    inline double get_crossbar_activity() { return m_crossbar_activity; }

    bool functionalRead(Packet *pkt, WriteMask &mask);
    //Function for figuring out if any of the messages in switchBuffer 
    //needs to be updated with the data from the packet.
    uint32_t functionalWrite(Packet *pkt);
    //for resetting CrossbarSwitch stats
    void resetStats();

  private:
    //the bus this BusCrossbarSwitch is part of
    Bus *m_bus;
    //number of VCs
    int m_num_vcs;
    //number of times the crossbar is used so far
    double m_crossbar_activity;
    //=========================================================
    //only one buffer to hold the flit that we are going to broadcast
    flitBuffer switchBuffer;
    //we need the number of outports to broadcast
    int m_num_outports;
    //=========================================================
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_GARNET_0_BUSCROSSBARSWITCH_HH__


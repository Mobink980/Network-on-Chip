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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_BUSOUTPUTUNIT_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_BUSOUTPUTUNIT_HH__

#include <iostream>
#include <vector>

#include "base/compiler.hh"
#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/CommonTypes.hh"
#include "mem/ruby/network/garnet/NetworkLink.hh"
#include "mem/ruby/network/garnet/OutVcState.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

class CreditLink;
class BroadcastLink;

//OutputUnit, output port, and outport are the same thing.
//BusOutputUnit inherites from Consumer
class BusOutputUnit : public Consumer
{
  public:
    //BusOutputUnit constructor
    BusOutputUnit(int id, PortDirection direction, BroadcastLink *bus,
               uint32_t consumerVcs);
    //BusOutputUnit destructor
    ~BusOutputUnit() = default;
    //set the output (network) link for the OutputUnit
    void set_out_link(NetworkLink *link);
    //set the credit link for the OutputUnit
    void set_credit_link(CreditLink *credit_link);
    //read input credit from downstream router if it is ready,
    //increment the credit in the appropriate output VC state,
    //mark output VC as free if the credit carries is_free_signal as true.
    void wakeup();
    //get the OutputUnit network queue
    flitBuffer* getOutQueue();
    //printing the OutputUnit
    void print(std::ostream& out) const {};
    //for decrementing the credit in the appropriate output VC
    void decrement_credit(int out_vc);
    //for incrementing the credit in the appropriate output VC
    void increment_credit(int out_vc);
    //to check whether an output VC has any credit left
    bool has_credit(int out_vc);
    //to check whether a Vnet has a free VC
    bool has_free_vc(int vnet);
    //for selecting a free VC from a Vnet
    int select_free_vc(int vnet);

    //get the direction of the outport (east, west, south, north)
    inline PortDirection get_direction() { return m_direction; }

    //get the number of credits in a VC
    int
    get_credit_count(int vc)
    {
        return outVcState[vc].get_credit_count();
    }

    //get the id of the output (network) link for the OutputUnit
    inline int
    get_outlink_id()
    {
        return m_out_link->get_id();
    }

    //set the state of the OutputUnit VC at current time
    //(IDLE_, VC_AB_, ACTIVE_)
    inline void
    set_vc_state(VC_state_type state, int vc, Tick curTime)
    {
      outVcState[vc].setState(state, curTime);
    }

    //check to see whether the state of a OutputUnit VC is IDLE_
    inline bool
    is_vc_idle(int vc, Tick curTime)
    {
        return (outVcState[vc].isInState(IDLE_, curTime));
    }

    //for inserting a flit into an output VC
    void insert_flit(flit *t_flit);

    //get the number of VCs per Vnet in the OutputUnit
    inline int
    getVcsPerVnet()
    {
        return m_vc_per_vnet;
    }

    bool functionalRead(Packet *pkt, WriteMask &mask);

    //updating outBuffer flits with the data from the packet
    uint32_t functionalWrite(Packet *pkt);

  private:
    //the bus this OutputUnit is part of
    BroadcastLink *m_bus;
    //id of the OutputUnit (outport)
    GEM5_CLASS_VAR_USED int m_id;
    //the direction of the OutputUnit or outport
    PortDirection m_direction;
    //number of VCs per Vnet in the OutputUnit
    int m_vc_per_vnet;
    //output (network) link of the OutputUnit (outport)
    NetworkLink *m_out_link;
    //credit link of the OutputUnit (outport)
    CreditLink *m_credit_link;

    // This is for the network link to consume
    flitBuffer outBuffer;
    // vc state of downstream router
    std::vector<OutVcState> outVcState;
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_GARNET_0_BUSOUTPUTUNIT_HH__

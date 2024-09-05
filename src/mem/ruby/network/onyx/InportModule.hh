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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_INPORTMODULE_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_INPORTMODULE_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "mem/ruby/network/onyx/AckLink.hh"
#include "mem/ruby/network/onyx/NetLink.hh"
#include "mem/ruby/network/onyx/Switcher.hh"
#include "mem/ruby/network/onyx/VirtualPath.hh"
#include "mem/ruby/network/onyx/chunkBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{
//InputUnit, input port, and inport are the same thing.
//InputUnit inherites from Consumer
class InportModule : public Consumer
{
  public:
    //InportModule constructor
    InportModule(int id, PortDirection direction, Switcher *router);
    //InportModule destructor
    ~InportModule() = default;

    //read input flit from upstream router if it is ready,
    //buffer the flit for m_latency-1 cycles, and mark it
    //valid for SwitchAllocation starting that cycle.
    void wakeup();
    //printing the InputUnit
    void print(std::ostream& out) const {};

    //get the direction of the inport (east, west, south, north)
    inline PortDirection get_direction() { return m_direction; }

    //set the VC state as idle at the current_time
    inline void
    set_vc_idle(int vc, Tick curTime)
    {
        virtualChannels[vc].set_idle(curTime);
    }

    //set the VC state as active at the current_time
    inline void
    set_vc_active(int vc, Tick curTime)
    {
        virtualChannels[vc].set_active(curTime);
    }

    //grant the outport to the VC
    inline void
    grant_outport(int vc, int outport)
    {
        virtualChannels[vc].set_outport(outport);
    }

    //select a free VC from the outport for this inport VC
    //(for HEAD/HEAD_TAIL flits)
    inline void
    grant_outvc(int vc, int outvc)
    {
        virtualChannels[vc].set_outvc(outvc);
    }

    //get the outport for this inport VC
    inline int
    get_outport(int invc)
    {
        return virtualChannels[invc].get_outport();
    }

    //get the outport VC for this inport VC (for BODY/TAIL flits)
    inline int
    get_outvc(int invc)
    {
        return virtualChannels[invc].get_outvc();
    }

    //get the time of enqueue for this inport VC
    inline Tick
    get_enqueue_time(int invc)
    {
        return virtualChannels[invc].get_enqueue_time();
    }

    //increment the credit for this inport VC (one more free space)
    void increment_credit(int in_vc, bool free_signal, Tick curTime);

    //peek the top flit from the VC
    inline chunk*
    peekTopFlit(int vc)
    {
        return virtualChannels[vc].peekTopFlit();
    }

    //get the top flit from the VC (it peeks and pops the top flit)
    inline chunk*
    getTopFlit(int vc)
    {
        return virtualChannels[vc].getTopFlit();
    }

    //returns true if the VC needs a specific pipeline stage
    //at a specific time
    inline bool
    need_stage(int vc, flit_stage stage, Tick time)
    {
        return virtualChannels[vc].need_stage(stage, time);
    }

    //returns true if the inport VC is ready at the current_time
    inline bool
    isReady(int invc, Tick curTime)
    {
        return virtualChannels[invc].isReady(curTime);
    }

    //get the InputUnit credit queue
    chunkBuffer* getCreditQueue() { return &creditQueue; }

    //set the input (network) link for the InputUnit
    inline void
    set_in_link(NetLink *link)
    {
        m_in_link = link;
    }

    //get the id of the network link for the InputUnit
    inline int get_inlink_id() { return m_in_link->get_id(); }

    //set the credit link for the InputUnit
    inline void
    set_credit_link(AckLink *credit_link)
    {
        m_credit_link = credit_link;
    }

    //get the number of buffer reads of the InputUnit for the Vnet
    double get_buf_read_activity(unsigned int vnet) const
    { return m_num_buffer_reads[vnet]; }
    //get the number of buffer writes of the InputUnit for the Vnet
    double get_buf_write_activity(unsigned int vnet) const
    { return m_num_buffer_writes[vnet]; }

    bool functionalRead(Packet *pkt, WriteMask &mask);

    //updating InputUnit VC messages with the data from the packet
    uint32_t functionalWrite(Packet *pkt);
    //for resetting InputUnit stats
    void resetStats();

  private:
    //the router (Switcher) this InputUnit is part of
    Switcher *m_router;
    //id of the InputUnit (inport)
    int m_id;
    //the direction of the InputUnit or inport
    PortDirection m_direction;
    //number of VCs per Vnet in the InputUnit
    int m_vc_per_vnet;
    //input (network) link of the InputUnit (inport)
    NetLink *m_in_link;
    //credit link of the InputUnit (inport)
    AckLink *m_credit_link;
    //InputUnit queue for holding credits
    chunkBuffer creditQueue;

    // Input Virtual channels (VCs of the inport)
    std::vector<VirtualPath> virtualChannels;

    // Statistical variables
    //InputUnit buffer write activity
    std::vector<double> m_num_buffer_writes;
    //InputUnit buffer read activity
    std::vector<double> m_num_buffer_reads;
};

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_ONYX_0_INPORTMODULE_HH__

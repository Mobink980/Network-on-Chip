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


#ifndef __MEM_RUBY_NETWORK_EMERALD_0_GRIDLINK_HH__
#define __MEM_RUBY_NETWORK_EMERALD_0_GRIDLINK_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/emerald/CommonTypes.hh"
#include "mem/ruby/network/emerald/fragmentBuffer.hh"
#include "params/GridLink.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

class EmeraldNetwork;

//GridLink inherites from ClockedObject and Consumer
class GridLink : public ClockedObject, public Consumer
{
  public:
    typedef GridLinkParams Params;
    GridLink(const Params &p); //constructor
    ~GridLink() = default; //destructor

    //NetworkLink consumes flits
    void setLinkConsumer(Consumer *consumer);
    //setting the source queue (or flit buffer) on srcClockObject
    //for the NetworkLink
    void setSourceQueue(fragmentBuffer *src_queue, ClockedObject *srcClockObject);
    //number of VCs per Vnet (e.g., 4)
    virtual void setVcsPerVnet(uint32_t consumerVcs);
    //set the type of the link (int_, ext_in, ext_out, etc)
    void setType(link_type type) { m_type = type; }
    //get the link type (int_, ext_in, ext_out, etc)
    link_type getType() { return m_type; }
    //print the NetworkLink or CreditLink
    void print(std::ostream& out) const {}
    //get the NetworkLink id
    int get_id() const { return m_id; }
    //get the buffer of the link
    fragmentBuffer *getBuffer() { return &linkBuffer;}
    //link like buffer is a consumer, so it needs a wakeup() function
    virtual void wakeup();

    //get the utilization of the NetworkLink or CreditLink
    unsigned int getLinkUtilization() const { return m_link_utilized; }
    //get the load of the VC connected to the link
    const std::vector<unsigned int> & getVcLoad() const { return m_vc_load; }

    //check whether the link buffer is ready at the
    //current tick
    inline bool isReady(Tick curTime)
    {
        return linkBuffer.isReady(curTime);
    }

    //for peeking and getting the top flit from the linkBuffer
    inline fragment* peekLink() { return linkBuffer.peekTopFlit(); }
    inline fragment* consumeLink() { return linkBuffer.getTopFlit(); }

    bool functionalRead(Packet *pkt, WriteMask &mask);
    uint32_t functionalWrite(Packet *); //functional write
    void resetStats(); //resetting statistics

    std::vector<int> mVnets; //Vnets for the link
    uint32_t bitWidth; //link width(same for NetworkLink & CreditLink)

  private:
    const int m_id; //id of the link
    link_type m_type; //type of the link (int_, ext_in, ext_out, etc)
    const Cycles m_latency; //latency of the link (in cycles)

    ClockedObject *src_object; //ClockedObject at the src of the link

    // Statistical variables
    unsigned int m_link_utilized; //link utilization
    std::vector<unsigned int> m_vc_load; //load of the VC

  protected:
    uint32_t m_virt_nets; //Vnets
    fragmentBuffer linkBuffer; //link buffer (for holding flits)
    Consumer *link_consumer; //link consumes flits
    //the queue or flitBuffer the link receives flits from
    fragmentBuffer *link_srcQueue;

};

} // namespace emerald
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_EMERALD_0_GRIDLINK_HH__

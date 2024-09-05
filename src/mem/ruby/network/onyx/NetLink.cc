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


#include "mem/ruby/network/onyx/NetLink.hh"

#include "base/trace.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/onyx/AckLink.hh"

//=====================================
#include <iostream>
//=====================================

//Both NetworkLink.hh and CreditLink.hh are included here.
//There is no CreditLink.cc.
namespace gem5
{

namespace ruby
{

namespace onyx
{

//NetLink constructor
NetLink::NetLink(const Params &p)
    : ClockedObject(p), Consumer(this), m_id(p.link_id),
      m_type(NUM_LINK_TYPES_),
      m_latency(p.link_latency), m_link_utilized(0),
      m_virt_nets(p.virt_nets), linkBuffer(),
      link_consumer(nullptr), link_srcQueue(nullptr)
{
    int num_vnets = (p.supported_vnets).size(); //number of Vnets
    mVnets.resize(num_vnets); //update the size of the Vnets
    bitWidth = p.width; //update the link width
    for (int i = 0; i < num_vnets; i++) {
        mVnets[i] = p.supported_vnets[i]; //set the Vnets
    }
}

//set the consumer for the NetworkLink or CreditLink
void
NetLink::setLinkConsumer(Consumer *consumer)
{
    link_consumer = consumer;
}

//set the VCs per Vnet
void
NetLink::setVcsPerVnet(uint32_t consumerVcs)
{
    //update the size of m_vc_load
    m_vc_load.resize(m_virt_nets * consumerVcs);
}

//set the source queue and source ClockedObject for the link
void
NetLink::setSourceQueue(chunkBuffer *src_queue, ClockedObject *srcClockObj)
{
    link_srcQueue = src_queue;
    src_object = srcClockObj;
}

//This function wakes up the link to transfer flits from a
//source ClockedObject to a destination ClockedObject.
void
NetLink::wakeup()
{
    DPRINTF(RubyNetwork, "Woke up to transfer flits from %s\n",
        src_object->name());
    assert(link_srcQueue != nullptr); //ensure there's a srcQueue
    assert(curTick() == clockEdge()); //current tick must be the clock edge

    if (link_srcQueue->isReady(curTick())) {
        //get the top flit from link_srcQueue
        chunk *t_flit = link_srcQueue->getTopFlit();
        //it takes m_latency cycles for flit t_flit to traverse the link
        DPRINTF(RubyNetwork, "Transmission will finish at %ld :%s\n",
                clockEdge(m_latency), *t_flit);
        if (m_type != NUM_LINK_TYPES_) {
            // Only for assertions and debug messages
            //flit width must be equal to link width
            //======================================================
            // std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
            // std::cout << "ID of the flit: " << t_flit->get_id() <<"\n";
            // std::cout << "Did t_flit came from bus? " << t_flit->is_broadcast() <<"\n";
            // std::cout << "t_flit source router is: R" << t_flit->get_route().src_router <<"\n";
            // std::cout << "t_flit destination router is: R" << t_flit->get_route().dest_router <<"\n";
            // std::cout << "Size of the flit: " << t_flit->m_width <<"\n";
            // std::cout << "Size of the network link: " << bitWidth <<"\n";
            // std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
            //======================================================
            assert(t_flit->m_width == bitWidth);
            //ensure the Vnet of the flit is valid, and also
            //the size of the Vnets is not zero
            assert((std::find(mVnets.begin(), mVnets.end(),
                t_flit->get_vnet()) != mVnets.end()) ||
                (mVnets.size() == 0));
        }
        //set the time for the flit (the time it reaches to the
        //other side of the link)
        t_flit->set_time(clockEdge(m_latency));
        //insert the flit into the linkBuffer
        linkBuffer.insert(t_flit);
        //consume the arrived flit (t_flit) after m_latency cycles
        link_consumer->scheduleEventAbsolute(clockEdge(m_latency));
        //increment m_link_utilized (link utilization)
        m_link_utilized++;
        //increment the load of the VC that t_flit is
        //going to be stored
        m_vc_load[t_flit->get_vc()]++;
    }

    //if link_srcQueue is not empty (if it's empty of flits,
    //there would be no need to schedule an event)
    if (!link_srcQueue->isEmpty()) {
        scheduleEvent(Cycles(1)); //schedule the event in one cycle
    }
}

//for resetting link statistics
void
NetLink::resetStats()
{
    for (int i = 0; i < m_vc_load.size(); i++) {
        m_vc_load[i] = 0;
    }

    m_link_utilized = 0;
}

bool
NetLink::functionalRead(Packet *pkt, WriteMask &mask)
{
    return linkBuffer.functionalRead(pkt, mask);
}

//write the packet into the link buffer
uint32_t
NetLink::functionalWrite(Packet *pkt)
{
    return linkBuffer.functionalWrite(pkt);
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

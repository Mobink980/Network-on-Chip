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


#include "mem/ruby/network/garnet/OutputUnit.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/Credit.hh"
#include "mem/ruby/network/garnet/CreditLink.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/network/garnet/flitBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

//OutputUnit constructor for instantiation
OutputUnit::OutputUnit(int id, PortDirection direction, Router *router,
  uint32_t consumerVcs)
  : Consumer(router), m_router(router), m_id(id), m_direction(direction),
    m_vc_per_vnet(consumerVcs)
{
    //number of VCs in this OutputUnit
    const int m_num_vcs = consumerVcs * m_router->get_num_vnets();
    //preallocate memory for m_num_vcs elements in outVcState vector
    //for each VC, we need to save its state in outVcState vector
    outVcState.reserve(m_num_vcs);
    //Instantiating outVcState vector
    for (int i = 0; i < m_num_vcs; i++) {
        //initialize a VcState object
        outVcState.emplace_back(i, m_router->get_net_ptr(), consumerVcs);
    }
}

//for decrementing the credit in the appropriate output VC
void
OutputUnit::decrement_credit(int out_vc)
{
    //printing router_id, the OutputUnit, outvc credits, outvc,
    //current cycle, and credit_link
    DPRINTF(RubyNetwork, "Router %d OutputUnit %s decrementing credit:%d for "
            "outvc %d at time: %lld for %s\n", m_router->get_id(),
            m_router->getPortDirectionName(get_direction()),
            outVcState[out_vc].get_credit_count(),
            out_vc, m_router->curCycle(), m_credit_link->name());

    //decrement credit for out_vc
    outVcState[out_vc].decrement_credit();
}

//for incrementing the credit in the appropriate output VC
void
OutputUnit::increment_credit(int out_vc)
{
    //printing router_id, the OutputUnit, outvc credits, outvc,
    //current cycle, and credit_link
    DPRINTF(RubyNetwork, "Router %d OutputUnit %s incrementing credit:%d for "
            "outvc %d at time: %lld from:%s\n", m_router->get_id(),
            m_router->getPortDirectionName(get_direction()),
            outVcState[out_vc].get_credit_count(),
            out_vc, m_router->curCycle(), m_credit_link->name());

    //increment credit for out_vc
    outVcState[out_vc].increment_credit();
}

// Check if the output VC (i.e., input VC at next router)
// has free credits (i..e, buffer slots).
// This is tracked by OutVcState
bool
OutputUnit::has_credit(int out_vc)
{
    //make sure out_vc state is ACTIVE_
    assert(outVcState[out_vc].isInState(ACTIVE_, curTick()));
    return outVcState[out_vc].has_credit();
}


// Check if the output port (i.e., input port at next router) has free VCs.
bool
OutputUnit::has_free_vc(int vnet)
{
    //the first VC in the given Vnet
    int vc_base = vnet*m_vc_per_vnet;
    //go through all VCs in the given Vnet, if you found a VC that
    //is in IDLE_ state, then we have a free VC
    for (int vc = vc_base; vc < vc_base + m_vc_per_vnet; vc++) {
        if (is_vc_idle(vc, curTick()))
            return true;
    }

    return false;
}

// Assign a free output VC to the winner of Switch Allocation
int
OutputUnit::select_free_vc(int vnet)
{
    //the first VC in the given Vnet
    int vc_base = vnet*m_vc_per_vnet;
    //go through all VCs in the given Vnet, find the first VC that
    //is in IDLE_ state (that VC is free), change its state to ACTIVE_,
    //and assign that free outvc to the winner of SA
    for (int vc = vc_base; vc < vc_base + m_vc_per_vnet; vc++) {
        if (is_vc_idle(vc, curTick())) {
            outVcState[vc].setState(ACTIVE_, curTick());
            return vc;
        }
    }
    //it returns -1 if we can't find a free VC in the outport
    return -1;
}

/*
 * The wakeup function of the OutputUnit reads the credit signal from the
 * downstream router for the output VC (i.e., input VC at downstream router).
 * It increments the credit count in the appropriate output VC state.
 * If the credit carries is_free_signal as true,
 * the output VC is marked IDLE (meaning that VC is free).
 */

void
OutputUnit::wakeup()
{
    //if the credit link of the outport is ready at the current tick
    if (m_credit_link->isReady(curTick())) {
        //put the content of the credit link into t_credit
        Credit *t_credit = (Credit*) m_credit_link->consumeLink();
        //increment the credit for the outvc of t_credit
        //It means that outvc (i.e., input VC of the downstream router)
        //has one more free slot.
        increment_credit(t_credit->get_vc());

        //if is_free_signal in t_credit is true, then set the VC state
        //for that outvc to IDLE_
        if (t_credit->is_free_signal())
            set_vc_state(IDLE_, t_credit->get_vc(), curTick());

        //deleting the created variable
        delete t_credit;

        //if the credit link of the outport is ready at the current tick
        if (m_credit_link->isReady(curTick())) {
            //schedule the consumption event for the next cycle
            scheduleEvent(Cycles(1));
        }
    }
}

//get the OutputUnit network queue (buffer that sends the flits
//on the output (network) link, to a VC in downstream router InputUnit)
flitBuffer*
OutputUnit::getOutQueue()
{
    return &outBuffer;
}

//set the output (network) link for the OutputUnit
void
OutputUnit::set_out_link(NetworkLink *link)
{
    m_out_link = link;
}

//set the credit link for the OutputUnit
void
OutputUnit::set_credit_link(CreditLink *credit_link)
{
    m_credit_link = credit_link;
}

//for inserting a flit into an output VC
//(i.e., input VC of the downstream router)
void
OutputUnit::insert_flit(flit *t_flit)
{
    //insert t_flit into outBuffer flitBuffer
    outBuffer.insert(t_flit);
    //schedule consumption event for m_out_link for the next cycle
    m_out_link->scheduleEventAbsolute(m_router->clockEdge(Cycles(1)));
}

bool
OutputUnit::functionalRead(Packet *pkt, WriteMask &mask)
{
    return outBuffer.functionalRead(pkt, mask);
}

//updating outBuffer flits with the data from the packet
uint32_t
OutputUnit::functionalWrite(Packet *pkt)
{
    return outBuffer.functionalWrite(pkt);
}

} // namespace garnet
} // namespace ruby
} // namespace gem5

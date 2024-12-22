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


#include "mem/ruby/network/emerald/VirtualWay.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

//VC constructor
VirtualWay::VirtualWay()
  : inputBuffer(), m_vc_state(IDLE_, Tick(0)), m_output_port(-1),
    m_enqueue_time(INFINITE_), m_output_vc(-1), m_broadcast_output_vcs()
{
}

//set the vc state to idle (the VC is free)
void
VirtualWay::set_idle(Tick curTime)
{
    m_vc_state.first = IDLE_;
    m_vc_state.second = curTime;
    m_enqueue_time = Tick(INFINITE_);
    m_output_port = -1;
    m_output_vc = -1;
    //===========================================================
    //Remove all elements from m_broadcast_output_vcs vector
    m_broadcast_output_vcs.clear();
    //===========================================================
}

//set the vc state to active (the VC is in use)
void
VirtualWay::set_active(Tick curTime)
{
    m_vc_state.first = ACTIVE_;
    m_vc_state.second = curTime;
    m_enqueue_time = curTime;
}

//check the stage of the inputBuffer top flit at time and
//see if it is a specific state
bool
VirtualWay::need_stage(flit_stage stage, Tick time)
{
    if (inputBuffer.isReady(time)) {
        //make sure the vc state is active and the time is greater than
        //or equal to the time the active state was set
        assert(m_vc_state.first == ACTIVE_ && m_vc_state.second <= time);
        //peek the top flit from inputBuffer
        fragment *t_flit = inputBuffer.peekTopFlit();
        //if the stage of the flit at time is the given stage, return true
        return(t_flit->is_stage(stage, time));
    }
    //otherwise return false
    return false;
}

bool
VirtualWay::functionalRead(Packet *pkt, WriteMask &mask)
{
    return inputBuffer.functionalRead(pkt, mask);
}

//updating inputBuffer flits with the data from the packet
uint32_t
VirtualWay::functionalWrite(Packet *pkt)
{
    return inputBuffer.functionalWrite(pkt);
}

} // namespace emerald
} // namespace ruby
} // namespace gem5

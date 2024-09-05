/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
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


#include "mem/ruby/network/onyx/VcState.hh"

#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

//VcState constructor
VcState::VcState(int id, OnyxNetwork *network_ptr,
    uint32_t consumerVcs)
    : m_time(0)
{
    m_id = id; //set the id for the vc
    m_vc_state = IDLE_; //initial state for the vc is IDLE_
    /*
     * We find the virtual network using the number of
     * vcs per vnet. This assumes that the same vcs per
     * vnet is used throughout the given object.
     */
    int vnet = floor(id/consumerVcs);

    //determine the maximum number of credits for the VC
    //based on the vnet_type (DATA_VNET_ or CTRL_VNET_)
    if (network_ptr->get_vnet_type(vnet) == DATA_VNET_)
        m_max_credit_count = network_ptr->getBuffersPerDataVC();
    else
        m_max_credit_count = network_ptr->getBuffersPerCtrlVC();

    //initialize vc credits (free slots or buffers) to m_max_credit_count
    m_credit_count = m_max_credit_count;
    //ensure we have at least one credit for the vc
    assert(m_credit_count >= 1);
}

//incrementing credit for the vc
void
VcState::increment_credit()
{
    m_credit_count++;
    assert(m_credit_count <= m_max_credit_count);
}

//decrementing credit for the vc
void
VcState::decrement_credit()
{
    m_credit_count--;
    assert(m_credit_count >= 0);
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

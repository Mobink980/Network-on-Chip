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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_OUTVCSTATE_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_OUTVCSTATE_HH__

#include "mem/ruby/network/garnet/CommonTypes.hh"
#include "mem/ruby/network/garnet/GarnetNetwork.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

class OutVcState
{
  public:
    //OutVcState constructor
    OutVcState(int id, GarnetNetwork *network_ptr, uint32_t consumerVcs);

    //get the number of credits (free slots) in an outvc
    int get_credit_count()          { return m_credit_count; }
    //check whether an outvc has any credit left
    inline bool has_credit()       { return (m_credit_count > 0); }
    //incrementing credit for an outvc
    void increment_credit();
    //decrementing credit for an outvc
    void decrement_credit();

    //check whether the VC is in a specific state at request_time
    inline bool
    isInState(VC_state_type state, Tick request_time)
    {
        return ((m_vc_state == state) && (request_time >= m_time) );
    }
    //set the state of the VC at time
    inline void
    setState(VC_state_type state, Tick time)
    {
        m_vc_state = state;
        m_time = time;
    }

  private:
    //vc id
    int m_id ;
    //time of setting the state for the vc
    Tick m_time;
    //state of the vc
    VC_state_type m_vc_state;
    //number of credits (free slots) in the vc
    int m_credit_count;
    //maximum number of credits (free slots) in the vc
    int m_max_credit_count;
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_GARNET_0_OUTVCSTATE_HH__

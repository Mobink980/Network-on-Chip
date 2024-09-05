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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_VIRTUALPATH_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_VIRTUALPATH_HH__

#include <utility>
#include <vector>

#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "mem/ruby/network/onyx/chunkBuffer.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

class VirtualPath
{
  public:
    //VC constructor
    VirtualPath();
    //VC destructor
    ~VirtualPath() = default;

    //check the stage of the inputBuffer top flit at time and
    //see if it is a specific state
    bool need_stage(flit_stage stage, Tick time);
    //set the vc state to idle
    void set_idle(Tick curTime);
    //set the vc state to active
    void set_active(Tick curTime);
    //set the output vc
    void set_outvc(int outvc)               { m_output_vc = outvc; }
    //get the output vc
    inline int get_outvc()                  { return m_output_vc; }
    //==============================================================
    //set the output vc for a specific outport
    void set_broadcast_outvc(int outvc) {
        m_broadcast_output_vcs.push_back(outvc);
    }
    //get the outvc for a specific outport
    inline int get_broadcast_outvc(int outport) {
        assert(!m_broadcast_output_vcs.empty());
        return m_broadcast_output_vcs[outport];
    }
    //to check the need for vc allocations in broadcast mode
    inline bool is_outvc_allocated() {
        if(m_broadcast_output_vcs.empty()) {
            return false;
        }
        return true;
    }
    //==============================================================
    //set the outport for the vc
    void set_outport(int outport)           { m_output_port = outport; };
    //get the outport of the vc
    inline int get_outport()                  { return m_output_port; }

    //get enqueue time for the vc
    inline Tick get_enqueue_time()          { return m_enqueue_time; }
    //set enqueue time for the vc
    inline void set_enqueue_time(Tick time) { m_enqueue_time = time; }
    //get the state of the vc
    inline VC_state_type get_state()        { return m_vc_state.first; }

    //check to see whether VC inputBuffer is ready
    inline bool
    isReady(Tick curTime)
    {
        return inputBuffer.isReady(curTime);
    }

    //insert a flit into the VC inputBuffer
    inline void
    insertFlit(chunk *t_flit)
    {
        inputBuffer.insert(t_flit);
    }

    //set the state for the vc
    inline void
    set_state(VC_state_type m_state, Tick curTime)
    {
        m_vc_state.first = m_state;
        m_vc_state.second = curTime;
    }

    //peek the top flit from the VC (i.e., inputBuffer)
    inline chunk*
    peekTopFlit()
    {
        return inputBuffer.peekTopFlit();
    }

    //get the top flit from the VC (i.e., inputBuffer)
    inline chunk*
    getTopFlit()
    {
        return inputBuffer.getTopFlit();
    }

    bool functionalRead(Packet *pkt, WriteMask &mask);

    //updating inputBuffer flits with the data from the packet
    uint32_t functionalWrite(Packet *pkt);

  private:
    //the flitBuffer for holding the flits of the VC
    chunkBuffer inputBuffer;
    //state of the virtual channel (VC)
    std::pair<VC_state_type, Tick> m_vc_state;
    //outport of the vc
    int m_output_port;
    //enqueue time for the vc
    Tick m_enqueue_time;
    //output vc
    int m_output_vc;
    //========================================
    //used in broadcasting (to save the output vc of multiple outports)
    std::vector<int> m_broadcast_output_vcs;
    //========================================
};

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_ONYX_0_VIRTUALPATH_HH__

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


#ifndef __MEM_RUBY_NETWORK_EMERALD_0_TSVSWITCHHANDLER_HH__
#define __MEM_RUBY_NETWORK_EMERALD_0_TSVSWITCHHANDLER_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/emerald/CommonTypes.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

class TSV;
class TSVInport;
class TSVOutport;

//TSVSwitchHandler inherits from Consumer
class TSVSwitchHandler : public Consumer
{
  public:
    //constructor
    TSVSwitchHandler(TSV *bus);
    //arbitrate inports (SA-I), places a request from the VC in
    //each inport to the outport it wants, arbitrate outports (SA-II),
    //read the flit out from the input vc, and send it to the CrossbarSwitch,
    //send an increment_credit signal to the upstream bus for this input vc.
    void wakeup();
    //initializing TSVSwitchHandler class variables
    void init();
    //Clear the request vector within the allocator at end of SA-II.
    //Was populated by SA-I.
    void clear_request_vector();
    //Wakeup the bus next cycle to perform SA again
    //if there are flits ready.
    void check_for_wakeup();
    //get the vnet of an input vc
    int get_vnet (int invc);
    //for printing the TSVSwitchHandler
    void print(std::ostream& out) const {};
    //SA-I: Loop through all input VCs at every
    //inport, and select one in a round-robin manner.
    void arbitrate_inports();
    //SA-II: Loop through all outports, and select one input vc
    //(that placed a request during SA-I) as the winner for this
    //outport in a round robin manner.
    void arbitrate_outports();
    //Check to see if a flit in an invc is allowed to be sent
    //to its desired output
    bool send_allowed(int inport, int invc, int outport, int outvc);
    //Assign a free VC to the winner of the outport (for HEAD/HEAD_TAIL flits)
    int vc_allocate(int outport, int inport, int invc);

    //get the input_arbiter activity for stats
    inline double
    get_input_arbiter_activity()
    {
        return m_input_arbiter_activity;
    }
    //get the output_arbiter activity for stats
    inline double
    get_output_arbiter_activity()
    {
        return m_output_arbiter_activity;
    }

    //resetting TSVSwitchHandler stats
    void resetStats();

  private:
    //number of inports/outports in the bus
    int m_num_inports, m_num_outports;
    //number of VCs; how many VCs per vnet
    int m_num_vcs, m_vc_per_vnet;

    //input_arbiter/output_arbiter activity stats
    double m_input_arbiter_activity, m_output_arbiter_activity;

    //the bus this TSVSwitchHandler is a part of
    TSV *m_bus;
    //to pick the invc we're choosing from every inport
    //in a round-robin manner
    std::vector<int> m_round_robin_invc;
    //for choosing an inport in a round-robin manner
    std::vector<int> m_round_robin_inport;
    //to hold each inport (the winning invc in that inport) wants
    //what outport in a cycle
    std::vector<int> m_port_requests;
    //to hold the winning invc in each inport
    std::vector<int> m_vc_winners;
};

} // namespace emerald
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_EMERALD_0_TSVSWITCHHANDLER_HH__

/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_RUBY_NETWORK_EMERALD_0_GRIDOVERPASS_HH__
#define __MEM_RUBY_NETWORK_EMERALD_0_GRIDOVERPASS_HH__

#include <iostream>
#include <queue>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/emerald/CommonTypes.hh"
#include "mem/ruby/network/emerald/AffirmLink.hh"
#include "mem/ruby/network/emerald/EmeraldLink.hh"
#include "mem/ruby/network/emerald/GridLink.hh"
#include "mem/ruby/network/emerald/fragmentBuffer.hh"
#include "params/GridOverpass.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

class EmeraldNetwork;

//GridOverpass inherites from AffirmLink
class GridOverpass: public AffirmLink
{
  public:
    typedef GridOverpassParams Params;
    GridOverpass(const Params &p); //constructor
    ~GridOverpass(); //destructor

    //initialize NetworkBridge class variables
    void initBridge(GridOverpass *coBrid, bool cdc_en, bool serdes_en);

    //Check if SerDes is enabled and do appropriate calculations for
    //serializing or deserializing the flits.
    //Check if CDC is enabled and schedule all the flits according to
    //the consumers clock domain.
    void wakeup();
    //pushes the eCredit in extraCredit queue for vc
    void neutralize(int vc, int eCredit);

    //schedule a flit to traverse the link
    void scheduleFlit(fragment *t_flit, Cycles latency);
    //to flitisize a flit (for SerDes) and sending it
    void flitisizeAndSend(fragment *t_flit);
    //set the number of vcs per vnet
    void setVcsPerVnet(uint32_t consumerVcs);

  protected:
    // Pointer to co-existing bridge
    // CreditBridge for Network Bridge and vice versa
    GridOverpass *coBridge;

    // Link connected toBridge
    // could be a source or destination
    // depending on mType
    GridLink *nLink;

    // CDC enable/disable
    bool enCdc;
    // SerDes enable/disable
    bool enSerDes;

    // Type of Bridge
    int mType;

    //latency of CDC unit
    Cycles cdcLatency;
    //latency of SerDes unit
    Cycles serDesLatency;

    //the last time flit was scheduled to be consumed by the link
    Tick lastScheduledAt;

    // Used by Credit Deserializer
    std::vector<int> lenBuffer;
    std::vector<int> sizeSent;
    std::vector<int> flitsSent;
    std::vector<std::queue<int>> extraCredit;

};

} // namespace emerald
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_EMERALD_0_GRIDOVERPASS_HH__

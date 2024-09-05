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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_COMMONTYPES_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_COMMONTYPES_HH__

#include "mem/ruby/common/NetDest.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

// All common enums and typedefs go here
//types of flits
enum flit_type {HEAD_, BODY_, TAIL_, HEAD_TAIL_,
                CREDIT_, NUM_FLIT_TYPE_};
//the state VC is in
enum VC_state_type {IDLE_, VC_AB_, ACTIVE_, NUM_VC_STATE_TYPE_};
//Vnets are used to avoid deadlock by eliminating message dependence
enum VNET_type {CTRL_VNET_, DATA_VNET_, NULL_VNET_, NUM_VNET_TYPE_};
//I:Invalid , VA:VC_Allocation, SA:Switch_Allocation,
//ST:Switch_Traversal, LT:Link_Traversal
enum flit_stage {I_, VA_, SA_, ST_, LT_, NUM_FLIT_STAGE_};
//EXT_IN_: from NI to the network
//EXT_OUT_: from network to NI (going out of the network)
//INT_: internal links between routers
// enum link_type { EXT_IN_, EXT_OUT_, INT_, NUM_LINK_TYPES_ };
//=============================================================
enum link_type { EXT_IN_, EXT_OUT_, INT_, NUM_LINK_TYPES_ };
//=============================================================
//0: table based (based on link weights)
//1: XY routing
//2: CUSTOM routing algorithm
enum RoutingAlgorithm { TABLE_ = 0, XY_ = 1, CUSTOM_ = 2,
                        NUM_ROUTING_ALGORITHM_};

//routing information for a packet
struct RouteInfo
{
    RouteInfo()
        : vnet(0), src_ni(0), src_router(0), dest_ni(0), dest_router(0),
          hops_traversed(0), broadcast(0)
    {}

    // destination format for table-based routing
    int vnet; //Vnet id
    NetDest net_dest; //network destination of the packet

    // src and dest format for topology-specific routing
    int src_ni; //id of the src NI
    int src_router; //id of the src Router
    int dest_ni; //id of the dest NI
    int dest_router; //id of the dest Router
    int hops_traversed; //number of hops the packet is traversed so far
    int broadcast; //A flag to check whether a flit is coming from a Bus
};
//a constant for infinity
#define INFINITE_ 10000

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_ONYX_0_COMMONTYPES_HH__
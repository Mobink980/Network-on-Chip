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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_CHUNK_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_CHUNK_HH__

#include <cassert>
#include <iostream>

#include "base/types.hh"
#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

class chunk
{
  public:
    //constructors
    chunk() {}
    chunk(int packet_id, int id, int vc, int vnet, RouteInfo route, int size,
         MsgPtr msg_ptr, int MsgSize, uint32_t bWidth, Tick curTime);

    //destructor
    virtual ~chunk(){};

    int get_outport() {return m_outport; } //flit outport
    int get_size() { return m_size; } //flit size
    //time it takes to enqueue the flit into the FIFO
    Tick get_enqueue_time() { return m_enqueue_time; }
    //time it takes to dequeue the flit from FIFO
    Tick get_dequeue_time() { return m_dequeue_time; }
    int getPacketID() { return m_packet_id; }
    //flit id
    int get_id() { return m_id; }
    //flit time of creation
    Tick get_time() { return m_time; }
    //get the vnet for an input channel
    int get_vnet() { return m_vnet; }
    //get a specfic vc from a vnet
    int get_vc() { return m_vc; }
    //routing information for a flit
    RouteInfo get_route() { return m_route; }
    //the message which this flit is a part of
    MsgPtr& get_msg_ptr() { return m_msg_ptr; }
    //type of the flit (HEAD, HEAD_TAIL, TAIL, etc)
    flit_type get_type() { return m_type; }
    //pipeline stage of the flit
    std::pair<flit_stage, Tick> get_stage() { return m_stage; }
    Tick get_src_delay() { return src_delay; }

    //set the outport of the flit
    void set_outport(int port) { m_outport = port; }
    //set the time for the flit
    void set_time(Tick time) { m_time = time; }
    //set the VC for the flit
    void set_vc(int vc) { m_vc = vc; }
    //set the route for the flit
    void set_route(RouteInfo route) { m_route = route; }
    void set_src_delay(Tick delay) { src_delay = delay; }
    //time it takes to dequeue the flit from FIFO
    void set_dequeue_time(Tick time) { m_dequeue_time = time; }
    //time it takes to enqueue the flit into the FIFO
    void set_enqueue_time(Tick time) { m_enqueue_time = time; }

    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    //set the broadcast state of a flit
    //0: flit is coming from the router, 1: flit is coming from the bus
    void set_broadcast(int state) {
        m_route.broadcast = state;
    }
    //returns true if a flit is coming from a Bus
    bool is_broadcast() {
        if(m_route.broadcast == 1){
            return true;
        }
        return false;
    }
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

    //for couting the hops of a flit till destination
    void increment_hops() { m_route.hops_traversed++; }
    virtual void print(std::ostream& out) const;

    //returns true if we have a valid flit stage
    //and the time is right.
    bool
    is_stage(flit_stage stage, Tick time)
    {
        return (stage == m_stage.first &&
                time >= m_stage.second);
    }

    //advance to the next stage of the pipeline.
    void
    advance_stage(flit_stage t_stage, Tick newTime)
    {
        m_stage.first = t_stage; //update the stage
        m_stage.second = newTime; //update the time
    }

    //finding out which flit is before the other one
    static bool
    greater(chunk* n1, chunk* n2)
    {
        if (n1->get_time() == n2->get_time()) {
            //assert(n1->flit_id != n2->flit_id);
            return (n1->get_id() > n2->get_id());
        } else {
            return (n1->get_time() > n2->get_time());
        }
    }

    bool functionalRead(Packet *pkt, WriteMask &mask);
    bool functionalWrite(Packet *pkt);

    //for flit serialization (it is implemented in derived classes)
    virtual chunk* serialize(int ser_id, int parts, uint32_t bWidth);
    //for flit deserialization (it is implemented in derived classes)
    virtual chunk* deserialize(int des_id, int num_flits, uint32_t bWidth);

    uint32_t m_width; //width of the flit
    int msgSize; //message size
  protected:
    int m_packet_id;
    int m_id; //flit id
    int m_vnet; //vnet of the flit
    int m_vc; //VC of the flit
    RouteInfo m_route; //route of the flit
    int m_size; //size of the flit
    Tick m_enqueue_time, m_dequeue_time; //enqueue & dequeue time
    Tick m_time; //flit time of creation
    flit_type m_type; //flit type
    MsgPtr m_msg_ptr; //the message flit is part of
    int m_outport; //flit outport
    Tick src_delay;
    std::pair<flit_stage, Tick> m_stage; //pipeline stage of the flit
};

//for printing flit information
inline std::ostream&
operator<<(std::ostream& out, const chunk& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_ONYX_0_CHUNK_HH__

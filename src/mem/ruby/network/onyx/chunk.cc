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


#include "mem/ruby/network/onyx/chunk.hh"

#include "base/intmath.hh"
#include "debug/RubyNetwork.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

// Constructor for the flit
chunk::chunk(int packet_id, int id, int vc, int vnet, RouteInfo route, int size,
    MsgPtr msg_ptr, int MsgSize, uint32_t bWidth, Tick curTime)
{
    //flit properties when creating a flit object
    m_size = size;
    m_msg_ptr = msg_ptr;
    m_enqueue_time = curTime;
    m_dequeue_time = curTime;
    m_time = curTime;
    m_packet_id = packet_id;
    m_id = id;
    m_vnet = vnet;
    m_vc = vc;
    m_route = route;
    m_stage.first = I_;
    m_stage.second = curTime;
    m_width = bWidth;
    msgSize = MsgSize;

    //If flit size is one, flit is either header
    //or tailer. If flit id is 0, we have header
    //flit. id = size-1 means we have tailer flit.
    //otherwise we have body flit (payload).
    if (size == 1) {
        m_type = HEAD_TAIL_;
        return;
    }
    if (id == 0)
        m_type = HEAD_;
    else if (id == (size - 1))
        m_type = TAIL_;
    else
        m_type = BODY_;
}

//function for serializing a flit into parts
chunk *
chunk::serialize(int ser_id, int parts, uint32_t bWidth)
{
    //ensure flit width is more than link bandwidth,
    //otherwise we won't be needing serialization.
    assert(m_width > bWidth);

    //ratio = div_ceiling(flit_width/link_bandwidth)
    int ratio = (int)divCeil(m_width, bWidth);
    //assigning a new id to the serialized flit
    int new_id = (m_id*ratio) + ser_id;
    //new_size (flit part size) = message_size/link_bandwidth
    int new_size = (int)divCeil((float)msgSize, (float)bWidth);
    assert(new_id < new_size);

    //create the new flit by calling the constructor
    chunk *fl = new chunk(m_packet_id, new_id, m_vc, m_vnet, m_route,
                    new_size, m_msg_ptr, msgSize, bWidth, m_time);
    //set the enqueue_time and src_delay for the created flit
    fl->set_enqueue_time(m_enqueue_time);
    fl->set_src_delay(src_delay);
    return fl;
}

//function for deserialization of parts into a flit
chunk *
chunk::deserialize(int des_id, int num_flits, uint32_t bWidth)
{
    //ratio = div_ceiling(link_bandwidth/flit_width)
    int ratio = (int)divCeil((float)bWidth, (float)m_width);
    //id of the deserialized flit
    int new_id = ((int)divCeil((float)(m_id+1), (float)ratio)) - 1;
    //size of the deserialized flit (msg_size/link_bandwidth)
    int new_size = (int)divCeil((float)msgSize, (float)bWidth);
    assert(new_id < new_size);

    //create the new flit by calling the constructor
    chunk *fl = new chunk(m_packet_id, new_id, m_vc, m_vnet, m_route,
                    new_size, m_msg_ptr, msgSize, bWidth, m_time);
    //set the enqueue_time and src_delay for the created flit
    fl->set_enqueue_time(m_enqueue_time);
    fl->set_src_delay(src_delay);
    return fl;
}

// Flit can be printed out for debugging purposes
//printing flit information
void
chunk::print(std::ostream& out) const
{
    out << "[flit:: ";
    out << "PacketId=" << m_packet_id << " ";
    out << "Id=" << m_id << " ";
    out << "Type=" << m_type << " ";
    out << "Size=" << m_size << " ";
    out << "Vnet=" << m_vnet << " ";
    out << "VC=" << m_vc << " ";
    out << "Src NI=" << m_route.src_ni << " ";
    out << "Src Router=" << m_route.src_router << " ";
    out << "Dest NI=" << m_route.dest_ni << " ";
    out << "Dest Router=" << m_route.dest_router << " ";
    out << "Set Time=" << m_time << " ";
    out << "Width=" << m_width<< " ";
    out << "]";
}

bool
chunk::functionalRead(Packet *pkt, WriteMask &mask)
{
    Message *msg = m_msg_ptr.get();
    return msg->functionalRead(pkt, mask);
}

bool
chunk::functionalWrite(Packet *pkt)
{
    Message *msg = m_msg_ptr.get();
    return msg->functionalWrite(pkt);
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

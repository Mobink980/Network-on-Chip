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


#ifndef __MEM_RUBY_NETWORK_EMERALD_0_FRAGMENTBUFFER_HH__
#define __MEM_RUBY_NETWORK_EMERALD_0_FRAGMENTBUFFER_HH__

#include <algorithm>
#include <iostream>
#include <vector>

#include "mem/ruby/network/emerald/CommonTypes.hh"
#include "mem/ruby/network/emerald/fragment.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

//buffer for holding flits
class fragmentBuffer
{
  public:
    fragmentBuffer(); //constructor (for infinite size)
    fragmentBuffer(int maximum_size); //another constructor

    //check whether the flitBuffer is ready
    bool isReady(Tick curTime);
    //check to see if the flitBuffer is empty
    bool isEmpty();
    //printing the flitBBuffer
    void print(std::ostream& out) const;
    //check to see if the flitBuffer is full
    bool isFull();
    //set the maximum size for the flitBuffer
    void setMaxSize(int maximum);
    //get the size of the flitBuffer
    int getSize() const { return m_buffer.size(); }

    //get the top flit of the flitBuffer (it peeks the first element
    //of the flitBuffer and frees one space)
    fragment *
    getTopFlit()
    {
        fragment *f = m_buffer.front();
        m_buffer.pop_front();
        return f;
    }

    //peek the top flit of the flitBuffer (this doesn't free space
    //in the flitBuffer)
    fragment *
    peekTopFlit()
    {
        return m_buffer.front();
    }

    //insert a flit into the flitBuffer
    void
    insert(fragment *flt)
    {
        m_buffer.push_back(flt);
    }

    bool functionalRead(Packet *pkt, WriteMask &mask);

    //for writing the data of the packet into the messages
    //of the flitBuffer
    uint32_t functionalWrite(Packet *pkt);

  private:
    //the container representing the flitBuffer
    std::deque<fragment *> m_buffer;
    //maximum size of the buffer
    int max_size;
};

//for printing the flitBuffer
inline std::ostream&
operator<<(std::ostream& out, const fragmentBuffer& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace emerald
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_EMERALD_0_FRAGMENTBUFFER_HH__

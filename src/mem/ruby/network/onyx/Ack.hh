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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_ACK_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_ACK_HH__

#include <cassert>
#include <iostream>

#include "base/types.hh"
#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "mem/ruby/network/onyx/chunk.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

// Credit (Ack) Signal for buffers inside VC
// Carries m_vc (inherits from chunk.hh)
// and m_is_free_signal (whether VC is free or not)

class Ack : public chunk
{
  public:
    Ack() {}; //constructor
    //another constructor
    Ack(int vc, bool is_free_signal, Tick curTime);

    // Functions used by SerDes
    //for serializing a flit (chunk) into parts
    chunk* serialize(int ser_id, int parts, uint32_t bWidth);
    //for deserializing several parts of a flit (chunk) into one
    chunk* deserialize(int des_id, int num_flits, uint32_t bWidth);
    void print(std::ostream& out) const;

    ~Ack() {}; //destructor
    //telling the previous router whether VC is free
    bool is_free_signal() { return m_is_free_signal; }

  private:
    bool m_is_free_signal;
};

} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_ONYX_0_ACK_HH__

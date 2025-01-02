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


#ifndef __MEM_RUBY_NETWORK_EMERALD_0_AFFIRM_HH__
#define __MEM_RUBY_NETWORK_EMERALD_0_AFFIRM_HH__

#include <cassert>
#include <iostream>

#include "base/types.hh"
#include "mem/ruby/network/emerald/CommonTypes.hh"
#include "mem/ruby/network/emerald/fragment.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

// Credit (Affirm) Signal for buffers inside VC
// Carries m_vc (inherits from fragment.hh)
// and m_is_free_signal (whether VC is free or not)

class Affirm : public fragment
{
  public:
    Affirm() {}; //constructor
    //another constructor
    Affirm(int vc, bool is_free_signal, Tick curTime);

    // Functions used by SerDes
    //for serializing a flit (fragment) into parts
    fragment* serialize(int ser_id, int parts, uint32_t bWidth);
    //for deserializing several parts of a flit (fragment) into one
    fragment* deserialize(int des_id, int num_flits, uint32_t bWidth);
    void print(std::ostream& out) const;

    ~Affirm() {}; //destructor
    //telling the previous router whether VC is free
    bool is_free_signal() { return m_is_free_signal; }

  private:
    bool m_is_free_signal;
};

} // namespace emerald
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_EMERALD_0_AFFIRM_HH__

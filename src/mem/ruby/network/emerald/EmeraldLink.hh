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


#ifndef __MEM_RUBY_NETWORK_EMERALD_0_EMERALDLINK_HH__
#define __MEM_RUBY_NETWORK_EMERALD_0_EMERALDLINK_HH__

#include <iostream>
#include <string>
#include <vector>

#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/network/emerald/AffirmLink.hh"
#include "mem/ruby/network/emerald/GridOverpass.hh"
#include "mem/ruby/network/emerald/GridLink.hh"
#include "params/EmeraldExtLink.hh"
#include "params/EmeraldIntLink.hh"
#include "params/EmeraldBusLink.hh"

namespace gem5
{

namespace ruby
{

namespace emerald
{

//EmeraldIntLink is inherited from BasicIntLink
class EmeraldIntLink : public BasicIntLink
{
  public:
    typedef EmeraldIntLinkParams Params;
    EmeraldIntLink(const Params &p); //constructor

    void init(); //initializing bridge for int links

    void print(std::ostream& out) const;

    //Make the EmeraldNetwork class a friend of EmeraldIntLink.
    //This gives EmeraldNetwork access to all private members
    //of EmeraldIntLink class.
    friend class EmeraldNetwork;

  protected:
    GridLink* m_network_link; //network link
    AffirmLink* m_credit_link; //credit link

    //automatically enabled when either SerDes or
    //CDC is enabled.
    bool srcBridgeEn;
    bool dstBridgeEn;

    //enabling Serializer-Deserializer units
    bool srcSerdesEn;
    bool dstSerdesEn;

    //enabling Clock Domain Crossing units
    bool srcCdcEn;
    bool dstCdcEn;

    //src and dst nodes of bridge for network link
    GridOverpass* srcNetBridge;
    GridOverpass* dstNetBridge;

    //src and dst nodes of bridge for credit link
    GridOverpass* srcCredBridge;
    GridOverpass* dstCredBridge;
};

//printing properties of a emerald internal link
inline std::ostream&
operator<<(std::ostream& out, const EmeraldIntLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

//EmeraldExtLink is inherited from BasicExtLink
class EmeraldExtLink : public BasicExtLink
{
  public:
    typedef EmeraldExtLinkParams Params;
    EmeraldExtLink(const Params &p); //constructor

    void init(); //initializing bridge for ext links

    void print(std::ostream& out) const;

    //Make the EmeraldNetwork class a friend of EmeraldExtLink.
    //This gives EmeraldNetwork access to all private members
    //of EmeraldExtLink class.
    friend class EmeraldNetwork;

  protected:
    //automatically enabled when either SerDes or
    //CDC is enabled.
    bool extBridgeEn;
    bool intBridgeEn;

    //enabling Serializer-Deserializer units
    bool extSerdesEn;
    bool intSerdesEn;

    //enabling Clock Domain Crossing units
    bool extCdcEn;
    bool intCdcEn;

    //external links are bi-directional.
    //we have two network_links and two
    //credit_links.
    GridLink* m_network_links[2];
    AffirmLink* m_credit_links[2];

    //ext and int nodes of bridge for network link
    GridOverpass* extNetBridge[2];
    GridOverpass* intNetBridge[2];

    //ext and int nodes of bridge for credit link
    GridOverpass* extCredBridge[2];
    GridOverpass* intCredBridge[2];

};

//printing properties of a Emerald external link
inline std::ostream&
operator<<(std::ostream& out, const EmeraldExtLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

//=================================================================
//=================================================================
//EmeraldBusLink is inherited from BasicBusLink
class EmeraldBusLink : public BasicBusLink
{
  public:
    typedef EmeraldBusLinkParams Params;
    EmeraldBusLink(const Params &p); //constructor

    void init(); //initializing bridge for NI-Bus links

    void print(std::ostream& out) const;

    //Make the EmeraldNetwork class a friend of EmeraldBusLink.
    //This gives EmeraldNetwork access to all private members
    //of EmeraldBusLink class.
    friend class EmeraldNetwork;

  protected:
    //automatically enabled when either SerDes or
    //CDC is enabled.
    bool extBridgeEn;
    bool intBridgeEn;

    //enabling Serializer-Deserializer units
    bool extSerdesEn;
    bool intSerdesEn;

    //enabling Clock Domain Crossing units
    bool extCdcEn;
    bool intCdcEn;

    //external links are bi-directional.
    //we have two network_links and two
    //credit_links.
    GridLink* m_network_links[2];
    AffirmLink* m_credit_links[2];

    //ext and int nodes of bridge for network link
    GridOverpass* extNetBridge[2];
    GridOverpass* intNetBridge[2];

    //ext and int nodes of bridge for credit link
    GridOverpass* extCredBridge[2];
    GridOverpass* intCredBridge[2];

};

//printing properties of a Emerald NIBus link
inline std::ostream&
operator<<(std::ostream& out, const EmeraldBusLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}
//=================================================================
//=================================================================


} // namespace emerald
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_EMERALD_0_EMERALDLINK_HH__

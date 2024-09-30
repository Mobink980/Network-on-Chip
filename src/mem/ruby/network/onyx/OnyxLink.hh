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


#ifndef __MEM_RUBY_NETWORK_ONYX_0_ONYXLINK_HH__
#define __MEM_RUBY_NETWORK_ONYX_0_ONYXLINK_HH__

#include <iostream>
#include <string>
#include <vector>

#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/network/onyx/AckLink.hh"
#include "mem/ruby/network/onyx/NetBridge.hh"
#include "mem/ruby/network/onyx/NetLink.hh"
#include "params/OnyxExtLink.hh"
#include "params/OnyxIntLink.hh"
#include "params/OnyxBusLink.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

//OnyxIntLink is inherited from BasicIntLink
class OnyxIntLink : public BasicIntLink
{
  public:
    typedef OnyxIntLinkParams Params;
    OnyxIntLink(const Params &p); //constructor

    void init(); //initializing bridge for int links

    void print(std::ostream& out) const;

    //Make the OnyxNetwork class a friend of OnyxIntLink.
    //This gives OnyxNetwork access to all private members
    //of OnyxIntLink class.
    friend class OnyxNetwork;

  protected:
    NetLink* m_network_link; //network link
    AckLink* m_credit_link; //credit link

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
    NetBridge* srcNetBridge;
    NetBridge* dstNetBridge;

    //src and dst nodes of bridge for credit link
    NetBridge* srcCredBridge;
    NetBridge* dstCredBridge;
};

//printing properties of a onyx internal link
inline std::ostream&
operator<<(std::ostream& out, const OnyxIntLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

//OnyxExtLink is inherited from BasicExtLink
class OnyxExtLink : public BasicExtLink
{
  public:
    typedef OnyxExtLinkParams Params;
    OnyxExtLink(const Params &p); //constructor

    void init(); //initializing bridge for ext links

    void print(std::ostream& out) const;

    //Make the OnyxNetwork class a friend of OnyxExtLink.
    //This gives OnyxNetwork access to all private members
    //of OnyxExtLink class.
    friend class OnyxNetwork;

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
    NetLink* m_network_links[2];
    AckLink* m_credit_links[2];

    //ext and int nodes of bridge for network link
    NetBridge* extNetBridge[2];
    NetBridge* intNetBridge[2];

    //ext and int nodes of bridge for credit link
    NetBridge* extCredBridge[2];
    NetBridge* intCredBridge[2];

};

//printing properties of a Onyx external link
inline std::ostream&
operator<<(std::ostream& out, const OnyxExtLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

//=================================================================
//=================================================================
//OnyxBusLink is inherited from BasicBusLink
class OnyxBusLink : public BasicBusLink
{
  public:
    typedef OnyxBusLinkParams Params;
    OnyxBusLink(const Params &p); //constructor

    void init(); //initializing bridge for NI-Bus links

    void print(std::ostream& out) const;

    //Make the OnyxNetwork class a friend of OnyxBusLink.
    //This gives OnyxNetwork access to all private members
    //of OnyxBusLink class.
    friend class OnyxNetwork;

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
    NetLink* m_network_links[2];
    AckLink* m_credit_links[2];

    //ext and int nodes of bridge for network link
    NetBridge* extNetBridge[2];
    NetBridge* intNetBridge[2];

    //ext and int nodes of bridge for credit link
    NetBridge* extCredBridge[2];
    NetBridge* intCredBridge[2];

};

//printing properties of a Onyx NIBus link
inline std::ostream&
operator<<(std::ostream& out, const OnyxBusLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}
//=================================================================
//=================================================================


} // namespace onyx
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_ONYX_0_ONYXLINK_HH__

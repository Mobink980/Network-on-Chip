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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_GARNETLINK_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_GARNETLINK_HH__

#include <iostream>
#include <string>
#include <vector>

#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/network/garnet/CreditLink.hh"
#include "mem/ruby/network/garnet/NetworkBridge.hh"
#include "mem/ruby/network/garnet/NetworkLink.hh"
#include "params/GarnetExtLink.hh"
#include "params/GarnetIntLink.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

//GarnetIntLink is inherited from BasicIntLink
class GarnetIntLink : public BasicIntLink
{
  public:
    typedef GarnetIntLinkParams Params;
    GarnetIntLink(const Params &p); //constructor

    void init(); //initializing bridge for int links

    void print(std::ostream& out) const;

    //Make the GarnetNetwork class a friend of GarnetIntLink.
    //This gives GarnetNetwork access to all private members 
    //of GarnetIntLink class.
    friend class GarnetNetwork;

  protected:
    NetworkLink* m_network_link; //network link
    CreditLink* m_credit_link; //credit link

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
    NetworkBridge* srcNetBridge;
    NetworkBridge* dstNetBridge;

    //src and dst nodes of bridge for credit link
    NetworkBridge* srcCredBridge;
    NetworkBridge* dstCredBridge;
};

//printing properties of a garnet internal link
inline std::ostream&
operator<<(std::ostream& out, const GarnetIntLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

//GarnetExtLink is inherited from BasicExtLink
class GarnetExtLink : public BasicExtLink
{
  public:
    typedef GarnetExtLinkParams Params;
    GarnetExtLink(const Params &p); //constructor

    void init(); //initializing bridge for ext links

    void print(std::ostream& out) const;

    //Make the GarnetNetwork class a friend of GarnetExtLink.
    //This gives GarnetNetwork access to all private members 
    //of GarnetExtLink class.
    friend class GarnetNetwork;

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
    NetworkLink* m_network_links[2];
    CreditLink* m_credit_links[2];

    //ext and int nodes of bridge for network link
    NetworkBridge* extNetBridge[2];
    NetworkBridge* intNetBridge[2];

    //ext and int nodes of bridge for credit link
    NetworkBridge* extCredBridge[2];
    NetworkBridge* intCredBridge[2];

};

//printing properties of a garnet external link
inline std::ostream&
operator<<(std::ostream& out, const GarnetExtLink& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif //__MEM_RUBY_NETWORK_GARNET_0_GARNETLINK_HH__


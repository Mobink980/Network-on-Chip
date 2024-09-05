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


#include "mem/ruby/network/onyx/NetBridge.hh"

#include <cmath>

#include "debug/RubyNetwork.hh"
#include "params/OnyxIntLink.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

//NetBridge constructor
NetBridge::NetBridge(const Params &p)
    :AckLink(p)
{
    //enable cdc
    enCdc = true;
    //enable SerDes
    enSerDes = true;
    //set the type of the bridge
    mType = p.vtype;

    //set he latency for the cdc unit
    cdcLatency = p.cdc_latency;
    //set the latency for SerDes unit
    serDesLatency = p.serdes_latency;
    //initialize the flit last scheduled time
    lastScheduledAt = 0;

    //set the link that is connected to the bridge
    nLink = p.link;
    if (mType == enums::LINK_OBJECT) { //we have a src link
        //NetworkBridge is the consumer
        nLink->setLinkConsumer(this);
        setSourceQueue(nLink->getBuffer(), nLink);
    } else if (mType == enums::OBJECT_LINK) { //we have a dest link
        nLink->setSourceQueue(&linkBuffer, this);
        //the link connected to NetworkBridge is the consumer
        setLinkConsumer(nLink);
    } else {
        // CDC type must be set
        panic("CDC type must be set");
    }
}

//set the number of vcs per vnet
void
NetBridge::setVcsPerVnet(uint32_t consumerVcs)
{
    DPRINTF(RubyNetwork, "VcsPerVnet VC: %d\n", consumerVcs);
    //call the setVcsPerVnet function of the NetworkLink class
    NetLink::setVcsPerVnet(consumerVcs);
    //bufffer_length = num_vnets * num_vcs_per_vnet
    lenBuffer.resize(consumerVcs * m_virt_nets);
    sizeSent.resize(consumerVcs * m_virt_nets);
    flitsSent.resize(consumerVcs * m_virt_nets);
    extraCredit.resize(consumerVcs * m_virt_nets);

    //call the setVcsPerVnet function for the link connected
    //to bridge
    nLink->setVcsPerVnet(consumerVcs);
}

//initialize NetworkBridge class variables
void
NetBridge::initBridge(NetBridge *coBrid, bool cdc_en, bool serdes_en)
{
    coBridge = coBrid; //set the pointer to the coexisting bridge
    enCdc = cdc_en; //set the CDC enable/disable
    enSerDes = serdes_en; //set the SerDes enable/disable
}

//NetworkBridge destructor
NetBridge::~NetBridge()
{
}

//schedule a flit to traverse the link after latency cycles (for cdc)
void
NetBridge::scheduleFlit(chunk *t_flit, Cycles latency)
{
    //total latency in cycles
    Cycles totLatency = latency;
    //if CDC is enabled
    if (enCdc) {
        // Add the CDC latency
        totLatency = latency + cdcLatency;
    }

    //calculate sendTime ==> after totLatency cycles
    Tick sendTime = link_consumer->getObject()->clockEdge(totLatency);
    //calculate the next available tick (lastScheduledAt + 1_cycle)
    Tick nextAvailTick = lastScheduledAt + link_consumer->getObject()->\
            cyclesToTicks(Cycles(1));
    //flit sending time is whichever the greatest between
    //nextAvailTick & sendTime
    sendTime = std::max(nextAvailTick, sendTime);
    //set the time for flit to be scheduled
    t_flit->set_time(sendTime);
    //update the lastScheduledAt variable
    lastScheduledAt = sendTime;
    //insert t_flit in linkBuffer
    linkBuffer.insert(t_flit);
    //schedule the link consumption event for sendTime
    link_consumer->scheduleEventAbsolute(sendTime);
}

//pushes the eCredit in extraCredit queue for vc
void
NetBridge::neutralize(int vc, int eCredit)
{
    extraCredit[vc].push(eCredit);
}

//to flitisize a flit (for SerDes) and sending it
void
NetBridge::flitisizeAndSend(chunk *t_flit)
{
    // Serialize-Deserialize only if it is enabled
    if (enSerDes) {
        // Calculate the target-width
        //=======================================
        //When the NetworkBridge itself is the consumer of
        //the flit, the current width is the bitWidth of the
        //link connected to bridge (nlink), and the target width
        //is the bitWidth of the link.
        int target_width = bitWidth;
        int cur_width = nLink->bitWidth;
        //if the link connected to bridge (nLink) is the consumer,
        //then the target bitWidth is nLink bitWidth, and the current
        //width is the width of the link.
        if (mType == enums::OBJECT_LINK) {
            target_width = nLink->bitWidth;
            cur_width = bitWidth;
        }

        //print the current and target bitWidth
        DPRINTF(RubyNetwork, "Target width: %d Current: %d\n",
            target_width, cur_width);
        //make sure they are not equal (otherwise, no SerDes unit
        //would be needed)
        assert(target_width != cur_width);

        //get the vc of the flit
        int vc = t_flit->get_vc();

        if (target_width > cur_width) { //we need to Deserialize
            // Deserialize
            // This deserializer combines flits from the
            // same message together
            int num_flits = 0;
            int flitPossible = 0;
            if (t_flit->get_type() == CREDIT_) {
                lenBuffer[vc]++;
                assert(extraCredit[vc].front());
                if (lenBuffer[vc] == extraCredit[vc].front()) {
                    flitPossible = 1;
                    extraCredit[vc].pop();
                    lenBuffer[vc] = 0;
                }
            } else if (t_flit->get_type() == TAIL_ ||
                       t_flit->get_type() == HEAD_TAIL_) {
                // If its the end of packet, then send whatever
                // is available.
                int sizeAvail = (t_flit->msgSize - sizeSent[vc]);
                flitPossible = ceil((float)sizeAvail/(float)target_width);
                assert (flitPossible < 2);
                num_flits = (t_flit->get_id() + 1) - flitsSent[vc];
                // Stop tracking the packet.
                flitsSent[vc] = 0;
                sizeSent[vc] = 0;
            } else {
                // If we are yet to receive the complete packet,
                // track the size recieved and flits deserialized.
                int sizeAvail =
                    ((t_flit->get_id() + 1)*cur_width) - sizeSent[vc];
                flitPossible = floor((float)sizeAvail/(float)target_width);
                assert (flitPossible < 2);
                num_flits = (t_flit->get_id() + 1) - flitsSent[vc];
                if (flitPossible) {
                    sizeSent[vc] += target_width;
                    flitsSent[vc] = t_flit->get_id() + 1;
                }
            }

            DPRINTF(RubyNetwork, "Deserialize :%dB -----> %dB "
                " vc:%d\n", cur_width, target_width, vc);

            chunk *fl = NULL;
            if (flitPossible) {
                fl = t_flit->deserialize(lenBuffer[vc], num_flits,
                    target_width);
            }

            // Inform the credit serializer about the number
            // of flits that were generated.
            if (t_flit->get_type() != CREDIT_ && fl) {
                coBridge->neutralize(vc, num_flits);
            }

            // Schedule only if we are done deserializing
            if (fl) {
                DPRINTF(RubyNetwork, "Scheduling a flit\n");
                lenBuffer[vc] = 0;
                scheduleFlit(fl, serDesLatency);
            }
            // Delete this flit, new flit is sent in any case
            delete t_flit;
        } else {
            // Serialize
            DPRINTF(RubyNetwork, "Serializing flit :%d -----> %d "
            "(vc:%d, Original Message Size: %d)\n",
                cur_width, target_width, vc, t_flit->msgSize);

            int flitPossible = 0;
            if (t_flit->get_type() == CREDIT_) {
                // We store the deserialization ratio and then
                // access it when serializing credits in the
                // oppposite direction.
                assert(extraCredit[vc].front());
                flitPossible = extraCredit[vc].front();
                extraCredit[vc].pop();
            } else if (t_flit->get_type() == HEAD_ ||
                    t_flit->get_type() == BODY_) {
                int sizeAvail =
                    ((t_flit->get_id() + 1)*cur_width) - sizeSent[vc];
                flitPossible = floor((float)sizeAvail/(float)target_width);
                if (flitPossible) {
                    sizeSent[vc] += flitPossible*target_width;
                    flitsSent[vc] += flitPossible;
                }
            } else {
                int sizeAvail = t_flit->msgSize - sizeSent[vc];
                flitPossible = ceil((float)sizeAvail/(float)target_width);
                sizeSent[vc] = 0;
                flitsSent[vc] = 0;
            }
            assert(flitPossible > 0);

            // Schedule all the flits
            // num_flits could be zero for credits
            for (int i = 0; i < flitPossible; i++) {
                // Ignore neutralized credits
                chunk *fl = t_flit->serialize(i, flitPossible, target_width);
                scheduleFlit(fl, serDesLatency);
                DPRINTF(RubyNetwork, "Serialized to flit[%d of %d parts]:"
                " %s\n", i+1, flitPossible, *fl);
            }

            if (t_flit->get_type() != CREDIT_) {
                coBridge->neutralize(vc, flitPossible);
            }
            // Delete this flit, new flit is sent in any case
            delete t_flit;
        }
        return;
    }

    // If only CDC is enabled schedule it
    scheduleFlit(t_flit, Cycles(0));
}

//Check if SerDes is enabled and do appropriate calculations for
//serializing or deserializing the flits.
//Check if CDC is enabled and schedule all the flits according to
//the consumers clock domain.
void
NetBridge::wakeup()
{
    chunk *t_flit;
    //if the link queue has a ready flit
    if (link_srcQueue->isReady(curTick())) {
        //get the top flit from the link queue
        t_flit = link_srcQueue->getTopFlit();
        //print the received flit
        DPRINTF(RubyNetwork, "Recieved flit %s\n", *t_flit);
        //flitisize t_flit and send it
        flitisizeAndSend(t_flit);
    }

    // Reschedule in case there is a waiting flit.
    if (!link_srcQueue->isEmpty()) {
        scheduleEvent(Cycles(1));
    }
}

} // namespace onyx
} // namespace ruby
} // namespace gem5

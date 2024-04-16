/*
 * Copyright (c) 2019-2021 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
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

#include "mem/ruby/network/MessageBuffer.hh"

#include <cassert>

#include "base/cprintf.hh"
#include "base/logging.hh"
#include "base/random.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyQueue.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

using stl_helpers::operator<<;

MessageBuffer::MessageBuffer(const Params &p)
    : SimObject(p), m_stall_map_size(0), m_max_size(p.buffer_size),
    m_max_dequeue_rate(p.max_dequeue_rate), m_dequeues_this_cy(0),
    m_time_last_time_size_checked(0),
    m_time_last_time_enqueue(0), m_time_last_time_pop(0),
    m_last_arrival_time(0), m_last_message_strict_fifo_bypassed(false),
    m_strict_fifo(p.ordered),
    m_randomization(p.randomization),
    m_allow_zero_latency(p.allow_zero_latency),
    m_routing_priority(p.routing_priority),
    ADD_STAT(m_not_avail_count, statistics::units::Count::get(),
             "Number of times this buffer did not have N slots available"),
    ADD_STAT(m_msg_count, statistics::units::Count::get(),
             "Number of messages passed the buffer"),
    ADD_STAT(m_buf_msgs, statistics::units::Rate<
                statistics::units::Count, statistics::units::Tick>::get(),
             "Average number of messages in buffer"),
    ADD_STAT(m_stall_time, statistics::units::Tick::get(),
             "Total number of ticks messages were stalled in this buffer"),
    ADD_STAT(m_stall_count, statistics::units::Count::get(),
             "Number of times messages were stalled"),
    ADD_STAT(m_avg_stall_time, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Average stall ticks per message"),
    ADD_STAT(m_occupancy, statistics::units::Rate<
                statistics::units::Ratio, statistics::units::Tick>::get(),
             "Average occupancy of buffer capacity")
{
    //initializing MessageBuffer class variables
    m_msg_counter = 0; //counting the messages coming into the MessageBuffer
    m_consumer = NULL;
    m_size_last_time_size_checked = 0;
    m_size_at_cycle_start = 0;
    m_stalled_at_cycle_start = 0;
    m_msgs_this_cycle = 0;
    m_priority_rank = 0;

    //clear the FIFO for the stalled messages
    m_stall_msg_map.clear();
    m_input_link_id = 0;
    m_vnet_id = 0;

    m_buf_msgs = 0;
    m_stall_time = 0;

    m_dequeue_callback = nullptr;

    // stats (defining stats we declared in MessageBuffer.hh)
    m_not_avail_count
        .flags(statistics::nozero);

    m_msg_count
        .flags(statistics::nozero);

    m_buf_msgs
        .flags(statistics::nozero);

    m_stall_count
        .flags(statistics::nozero);

    m_avg_stall_time
        .flags(statistics::nozero | statistics::nonan);

    m_occupancy
        .flags(statistics::nozero);

    m_stall_time
        .flags(statistics::nozero);

    if (m_max_size > 0) {
        m_occupancy = m_buf_msgs / m_max_size;
    } else { //if MessageBuffer size is infinite
        m_occupancy = 0; //MessageBuffer occupancy is zero 
    }

    m_avg_stall_time = m_stall_time / m_msg_count;
}

//get the size of the MessageBuffer at the current tick
unsigned int
MessageBuffer::getSize(Tick curTime)
{
    if (m_time_last_time_size_checked != curTime) {
        m_time_last_time_size_checked = curTime;
        m_size_last_time_size_checked = m_prio_heap.size();
    }

    return m_size_last_time_size_checked;
}

//to check whether n slots are available in the MessageBuffer 
//at the currentTick
bool
MessageBuffer::areNSlotsAvailable(unsigned int n, Tick current_time)
{

    // fast path when message buffers have infinite size
    if (m_max_size == 0) {
        return true;
    }

    // determine the correct size for the current cycle
    // pop operations shouldn't effect the network's visible size
    // until schd cycle, but enqueue operations effect the visible
    // size immediately
    //current size of unstalled messages (m_prio_heap size)
    unsigned int current_size = 0;
    //current stalled messages size (m_stall_map_size)
    unsigned int current_stall_size = 0;

    if (m_time_last_time_pop < current_time) {
        // no pops this cycle - heap and stall queue size is correct
        current_size = m_prio_heap.size();
        current_stall_size = m_stall_map_size;
    } else { //last message pop time from MessageBuffer >= current_time
        if (m_time_last_time_enqueue < current_time) {
            // no enqueues this cycle - m_size_at_cycle_start is correct
            current_size = m_size_at_cycle_start;
        } else {
            // both pops and enqueues occured this cycle - add new
            // enqueued msgs to m_size_at_cycle_start
            current_size = m_size_at_cycle_start + m_msgs_this_cycle;
        }

        // Stall queue size at start is considered
        //(pop takes effect in the schd cycle)
        current_stall_size = m_stalled_at_cycle_start;
    }

    // now compare the new size with our max size
    if (current_size + current_stall_size + n <= m_max_size) {
        return true; //we have n empty slots avialable
    } else {
        DPRINTF(RubyQueue, "n: %d, current_size: %d, heap size: %d, "
                "m_max_size: %d\n",
                n, current_size + current_stall_size,
                m_prio_heap.size(), m_max_size);
        m_not_avail_count++;
        return false;
    }
}

const Message*
MessageBuffer::peek() const
{
    DPRINTF(RubyQueue, "Peeking at head of queue.\n");
    //get the message at the head of m_prio_heap vector
    const Message* msg_ptr = m_prio_heap.front().get();
    assert(msg_ptr); //ensure the message is nonempty

    //printing the message itself(using * operator)
    DPRINTF(RubyQueue, "Message: %s\n", (*msg_ptr));
    return msg_ptr; //return the pointer to the message
}

//return a random tick for the random enqueue delay of the messages
// FIXME - move me somewhere else
Tick
random_time()
{
    Tick time = 1;
    time += random_mt.random(0, 3);  // [0...3]
    if (random_mt.random(0, 7) == 0) {  // 1 in 8 chance
        time += 100 + random_mt.random(1, 15); // 100 + [1...15]
    }
    return time;
}


//enqueue a message with delta ticks delay 
//We enqueue at arrival_time and can dequeue at arrival_time.
//The queuing delay takes effect in the enqueue function, by
//calculating the arrival_time. dequeue is done instantly when 
//it's called.
void
MessageBuffer::enqueue(MsgPtr message, Tick current_time, Tick delta,
                       bool bypassStrictFIFO)
{
    // record current time incase we have a pop that also adjusts my size
    if (m_time_last_time_enqueue < current_time) {
        m_msgs_this_cycle = 0;  // first msg this cycle
        //update last enqueue time
        m_time_last_time_enqueue = current_time;
    }

    m_msg_counter++; //one message came to MessageBuffer
    m_msgs_this_cycle++; //one message added to MessageBuffer at this cycle

    // Calculate the arrival time of the message, that is, the first
    // cycle the message can be dequeued.
    panic_if((delta == 0) && !m_allow_zero_latency,
           "Delta equals zero and allow_zero_latency is false during enqueue");
    Tick arrival_time = 0; //the first cycle the message can be dequeued

    // random delays are inserted if the RubySystem level randomization flag
    // is turned on and this buffer allows it
    if ((m_randomization == MessageRandomization::disabled) ||
        ((m_randomization == MessageRandomization::ruby_system) &&
          !RubySystem::getRandomization())) {
        // No randomization
        arrival_time = current_time + delta;
    } else {
        // Randomization - ignore delta
        if (m_strict_fifo) {
            if (m_last_arrival_time < current_time) {
                //a new message enqueues here
                m_last_arrival_time = current_time;
            }
            //the message can be dequeued a random_time after
            //the last arrival time   
            arrival_time = m_last_arrival_time + random_time();
        } else {
            //no FIFO - so each message can be dequeued completely 
            //random, regardless of the sequence
            arrival_time = current_time + random_time();
        }
    }

    // Check the arrival time
    assert(arrival_time >= current_time);
    if (m_strict_fifo &&
        !(bypassStrictFIFO || m_last_message_strict_fifo_bypassed)) {
        if (arrival_time < m_last_arrival_time) {
            panic("FIFO ordering violated: %s name: %s current time: %d "
                  "delta: %d arrival_time: %d last arrival_time: %d\n",
                  *this, name(), current_time, delta, arrival_time,
                  m_last_arrival_time);
        }
    }

    // If running a cache trace, don't worry about the last arrival checks
    if (!RubySystem::getWarmupEnabled()) {
        m_last_arrival_time = arrival_time;
    }

    m_last_message_strict_fifo_bypassed = bypassStrictFIFO;

    // compute the delay cycles and set enqueue time
    Message* msg_ptr = message.get();
    assert(msg_ptr != NULL); //we better be having a message to enqueue

    //this enqueue should be later than the last time the message was
    //enqueued.
    assert(current_time >= msg_ptr->getLastEnqueueTime() &&
           "ensure we aren't dequeued early");

    //UPDATING THE FIELDS OF THE MESSAGE
    //current_time is set as the delay the message experienced so far
    msg_ptr->updateDelayedTicks(current_time);
    //the last time the message was enqueued is now this time
    msg_ptr->setLastEnqueueTime(arrival_time);
    //update the message counter for this message
    msg_ptr->setMsgCounter(m_msg_counter);

    // Insert the message into the priority heap
    m_prio_heap.push_back(message);
    push_heap(m_prio_heap.begin(), m_prio_heap.end(), std::greater<MsgPtr>());
    // Increment the number of messages statistic
    m_buf_msgs++;

    //either the buffer has infinite capacity, or has enough capacity 
    //to enqueue one more message 
    assert((m_max_size == 0) ||
           ((m_prio_heap.size() + m_stall_map_size) <= m_max_size));

    //print the arrival_time and the message content
    DPRINTF(RubyQueue, "Enqueue arrival_time: %lld, Message: %s\n",
            arrival_time, *(message.get()));

    // Schedule the wakeup
    assert(m_consumer != NULL); //MessageBuffer must have a consumer
    //schedule the consumption or dequeue event
    //arrival_time is the first cycle the message can be dequeued
    m_consumer->scheduleEventAbsolute(arrival_time);
    //store the event information for the MessageBuffer vnet 
    m_consumer->storeEventInfo(m_vnet_id);
}

//for dequeuing a message from the MessageBuffer
Tick
MessageBuffer::dequeue(Tick current_time, bool decrement_messages)
{
    //We are popping the first element of m_prio_heap.
    //It is a priority heap, with the most important message as the 
    //first element. It can be FIFO so the message with the most  
    //priority is the first one arrived; this avoids starvation.
    DPRINTF(RubyQueue, "Popping\n");
    //make sure the time for dequeue has come 
    //(current_time <= SystemTime). In this function, current_time
    //is the dequeue time, and the SystemTime should pass that; 
    //meaning it needs to be greater or equal to current_time.
    assert(isReady(current_time));

    // get MsgPtr of the message about to be dequeued
    MsgPtr message = m_prio_heap.front();

    // get the delay cycles
    //current_time is set as the delay the message experienced so far
    message->updateDelayedTicks(current_time);
    //how many ticks the message was delayed
    Tick delay = message->getDelayedTicks();

    // record previous size and time so the current buffer size isn't
    // adjusted until schd cycle
    if (m_time_last_time_pop < current_time) {
        //updating the helper variables
        m_size_at_cycle_start = m_prio_heap.size();
        m_stalled_at_cycle_start = m_stall_map_size;
        m_time_last_time_pop = current_time;
        m_dequeues_this_cy = 0;
    }
    ++m_dequeues_this_cy;

    //pop the message with the most priority from the heap 
    pop_heap(m_prio_heap.begin(), m_prio_heap.end(), std::greater<MsgPtr>());
    m_prio_heap.pop_back(); //shrink m_prio_heap vector by one
    if (decrement_messages) {
        // Record how much time is passed since the message was enqueued
        m_stall_time += curTick() - message->getLastEnqueueTime();
        m_msg_count++;

        // If the message will be removed from the queue, decrement the
        // number of message in the queue.
        m_buf_msgs--;
    }

    // if a dequeue callback was requested, call it now
    if (m_dequeue_callback) {
        m_dequeue_callback();
    }

    return delay;
}

//to rgister dequeue callback (to undo a dequeue from the MessageBuffer)
void
MessageBuffer::registerDequeueCallback(std::function<void()> callback)
{
    //change m_dequeue_callback from nullptr to callback
    m_dequeue_callback = callback;
}

//to unregister dequeue callback
void
MessageBuffer::unregisterDequeueCallback()
{
    //change m_dequeue_callback from callback to nullptr
    m_dequeue_callback = nullptr;
}

//to clear a MessageBuffer (by clearing m_prio_heap and other 
//associated variables)
void
MessageBuffer::clear()
{
    m_prio_heap.clear();

    m_msg_counter = 0;
    m_time_last_time_enqueue = 0;
    m_time_last_time_pop = 0;
    m_size_at_cycle_start = 0;
    m_stalled_at_cycle_start = 0;
    m_msgs_this_cycle = 0;
}

//it takes the first element of m_prio_heap, pops the heap without
//shrinking it, and pushes that element to the back of the heap,
//but it can be enqueued after a recycle_latency after current_time 
void
MessageBuffer::recycle(Tick current_time, Tick recycle_latency)
{
    DPRINTF(RubyQueue, "Recycling.\n");
    assert(isReady(current_time)); //ensure current_time <= SystemTime
    //get the message at the front of the heap (first element)
    MsgPtr node = m_prio_heap.front();
    //pop the message from the heap
    pop_heap(m_prio_heap.begin(), m_prio_heap.end(), std::greater<MsgPtr>());

    //future_time is the time the message will be enqueued again
    Tick future_time = current_time + recycle_latency;
    //update the LastEnqueueTime for the message
    node->setLastEnqueueTime(future_time);

    //get the message at the back of the heap (last element)
    m_prio_heap.back() = node;
    //push the message to the heap
    push_heap(m_prio_heap.begin(), m_prio_heap.end(), std::greater<MsgPtr>());
    //schedule a consumption event for enqueue
    m_consumer->scheduleEventAbsolute(future_time);
}

//to push back messages to the m_prio_heap after being stalled 
//or deferred. It takes a list of message pointers and pushes 
//the elements of the list to m_prio_heap vector and schedules 
//a consumption event at schdTick (the requeue arrival time)
void
MessageBuffer::reanalyzeList(std::list<MsgPtr> &lt, Tick schdTick)
{
    while (!lt.empty()) { //while the list is not empty
        MsgPtr m = lt.front(); //get the front message
        //make sure LastEnqueueTime of the message is less than
        //or equal to schdTick (otherwise the concept of requeue 
        //goes under question)
        assert(m->getLastEnqueueTime() <= schdTick);

        //push the message at the back of the m_prio_heap vector
        m_prio_heap.push_back(m);
        //push the message to the heap
        push_heap(m_prio_heap.begin(), m_prio_heap.end(),
                  std::greater<MsgPtr>());

        //schedule the consumption event at schdTick
        m_consumer->scheduleEventAbsolute(schdTick);

        //printing the requeue arrival time (schdTick) and the message
        DPRINTF(RubyQueue, "Requeue arrival_time: %lld, Message: %s\n",
            schdTick, *(m.get()));

        //remove the first element from the list lt
        lt.pop_front();
    }
}


//to move back stalled messages from the m_stall_msg_map vector
//to the m_prio_heap vector with address addr at current_time.
//reanalyzing means sending back from stall queue to process queue.
void
MessageBuffer::reanalyzeMessages(Addr addr, Tick current_time)
{
    //print the address of the messages we are reanalyzing
    DPRINTF(RubyQueue, "ReanalyzeMessages %#x\n", addr);
    //make sure at least one stalled message with address addr 
    //exists in the m_stall_msg_map vector 
    assert(m_stall_msg_map.count(addr) > 0);

    //
    // Put all stalled messages associated with this address back on the
    // prio heap.  The reanalyzeList call will make sure the consumer is
    // scheduled for the current cycle so that the previously stalled messages
    // will be observed before any younger messages that may arrive this cycle
    //
    //Decrease the size of the reanalyzed messages from the m_stall_msg_map.
    //m_stall_map_size is the size of the m_stall_msg_map.
    m_stall_map_size -= m_stall_msg_map[addr].size();
    //size of m_stall_msg_map must not be negative
    assert(m_stall_map_size >= 0);
    //reanalyze all the messages with the address addr at current_time
    reanalyzeList(m_stall_msg_map[addr], current_time);
    //erase the address addr from m_stall_msg_map
    m_stall_msg_map.erase(addr);
}

//to move back all the stalled messages from the m_stall_msg_map 
//vector to the m_prio_heap vector at current_time, regardless of 
//the address.
void
MessageBuffer::reanalyzeAllMessages(Tick current_time)
{
    DPRINTF(RubyQueue, "ReanalyzeAllMessages\n");

    //
    // Put all stalled messages associated with this address back on the
    // prio heap.  The reanalyzeList call will make sure the consumer is
    // scheduled for the current cycle so that the previously stalled messages
    // will be observed before any younger messages that may arrive this cycle.
    //
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end(); ++map_iter) {
        //Decrease the size of the reanalyzed messages from the m_stall_msg_map.
        m_stall_map_size -= map_iter->second.size();
        //size of m_stall_msg_map must not be negative
        assert(m_stall_map_size >= 0);
        //reanalyze all the messages at current_time for all the addresses
        reanalyzeList(map_iter->second, current_time);
    }
    //clear the stalled messages queue
    m_stall_msg_map.clear();
}

//for pushing messages to the stall queue from m_prio_heap
void
MessageBuffer::stallMessage(Addr addr, Tick current_time)
{
    //print the address of the message
    DPRINTF(RubyQueue, "Stalling due to %#x\n", addr);
    //ensure current_time <= SystemTime (the time has come)
    assert(isReady(current_time));
    //ensure offset of the addr is zero
    assert(getOffset(addr) == 0);
    //get the first element of the m_prio_heap
    MsgPtr message = m_prio_heap.front();

    // Since the message will just be moved to stall map, indicate that the
    // buffer should not decrement the m_buf_msgs statistic
    dequeue(current_time, false);

    //
    // Note: no event is scheduled to analyze the map at a later time.
    // Instead the controller is responsible to call reanalyzeMessages when
    // these addresses change state.
    //
    //add the message to the end of the m_stall_msg_map[addr]
    (m_stall_msg_map[addr]).push_back(message);
    //increment the size of the m_stall_msg_map
    m_stall_map_size++;
    //increment the number of times messages were stalled
    m_stall_count++;
}

//return true if the stall map has a message of this address
bool
MessageBuffer::hasStalledMsg(Addr addr) const
{
    return (m_stall_msg_map.count(addr) != 0);
}

//Defer enqueueing a message to a later cycle by putting it aside and not
//enqueueing it in this cycle.
void
MessageBuffer::deferEnqueueingMessage(Addr addr, MsgPtr message)
{
    //printing the content and the address of the deferred message
    DPRINTF(RubyQueue, "Deferring enqueueing message: %s, Address %#x\n",
            *(message.get()), addr);
    //add the message to the end of the m_deferred_msg_map[addr]
    (m_deferred_msg_map[addr]).push_back(message);
}

//enqueue all the previously deferred messages that are associated with 
//the input address (enqueuing after delay ticks)
void
MessageBuffer::enqueueDeferredMessages(Addr addr, Tick curTime, Tick delay)
{
    //make sure there is a deferred message with the address addr
    assert(!isDeferredMsgMapEmpty(addr));
    //put m_deferred_msg_map[addr] into a std:vector of type MsgPtr 
    std::vector<MsgPtr>& msg_vec = m_deferred_msg_map[addr];
    //check that msg_vec (our std:vector) isn't empty
    assert(msg_vec.size() > 0);

    // enqueue all deferred messages associated with this address
    for (MsgPtr m : msg_vec) { //for every message in msg_vec
        enqueue(m, curTime, delay); //enque the message with delay
    }

    //clear msg_vec
    msg_vec.clear();
    //erase the address addr from m_deferred_msg_map
    m_deferred_msg_map.erase(addr);
}

//check whether there is a deferred message with the address addr
bool
MessageBuffer::isDeferredMsgMapEmpty(Addr addr) const
{
    return m_deferred_msg_map.count(addr) == 0;
}

//printing a MessageBuffer
void
MessageBuffer::print(std::ostream& out) const
{
    ccprintf(out, "[MessageBuffer: ");
    if (m_consumer != NULL) {
        ccprintf(out, " consumer-yes ");
    }

    std::vector<MsgPtr> copy(m_prio_heap);
    std::sort_heap(copy.begin(), copy.end(), std::greater<MsgPtr>());
    ccprintf(out, "%s] %s", copy, name());
}

//m_prio_heap should not be empty and the LastEnqueueTime 
//of the first element of the m_prio_heap should not be 
//greater than the current_time.
bool
MessageBuffer::isReady(Tick current_time) const
{
    assert(m_time_last_time_pop <= current_time);
    bool can_dequeue = (m_max_dequeue_rate == 0) ||
                       (m_time_last_time_pop < current_time) ||
                       (m_dequeues_this_cy < m_max_dequeue_rate);
    bool is_ready = (m_prio_heap.size() > 0) &&
                   (m_prio_heap.front()->getLastEnqueueTime() <= current_time);
    if (!can_dequeue && is_ready) {
        // Make sure the Consumer executes next cycle to dequeue the ready msg
        m_consumer->scheduleEvent(Cycles(1));
    }
    return can_dequeue && is_ready;
}

Tick
MessageBuffer::readyTime() const
{
    if (m_prio_heap.empty())
        return MaxTick;
    else
        return m_prio_heap.front()->getLastEnqueueTime();
}


//Functional read or write (for reading from the MessageBuffer messages, or
//writing the packet data into some of the messages in the MessageBuffer).
//It returns the number of functional accesses.
uint32_t
MessageBuffer::functionalAccess(Packet *pkt, bool is_read, WriteMask *mask)
{
    //printing the functionalaccess type (read or write) and 
    //the address of the packet
    DPRINTF(RubyQueue, "functional %s for %#x\n",
            is_read ? "read" : "write", pkt->getAddr());

    //keep track of the number of functional accesses
    uint32_t num_functional_accesses = 0;

    // Check the priority heap and write any messages that may
    // correspond to the address in the packet.
    for (unsigned int i = 0; i < m_prio_heap.size(); ++i) {
        Message *msg = m_prio_heap[i].get();
        //reading without mask, with mask, and writing respectively
        if (is_read && !mask && msg->functionalRead(pkt))
            return 1;
        else if (is_read && mask && msg->functionalRead(pkt, *mask))
            num_functional_accesses++;
        else if (!is_read && msg->functionalWrite(pkt))
            num_functional_accesses++;
    }

    // Check the stall queue and write any messages that may
    // correspond to the address in the packet.
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end();
         ++map_iter) {

        for (std::list<MsgPtr>::iterator it = (map_iter->second).begin();
            it != (map_iter->second).end(); ++it) {

            Message *msg = (*it).get();
            //reading without mask, with mask, and writing respectively
            if (is_read && !mask && msg->functionalRead(pkt))
                return 1;
            else if (is_read && mask && msg->functionalRead(pkt, *mask))
                num_functional_accesses++;
            else if (!is_read && msg->functionalWrite(pkt))
                num_functional_accesses++;
        }
    }

    return num_functional_accesses;
}

} // namespace ruby
} // namespace gem5

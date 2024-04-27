# Copyright (c) 2011 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from m5.params import *

from m5.objects.ClockedObject import ClockedObject

# BasicRouter is a ClockedObject
class BasicRouter(ClockedObject):
    type = 'BasicRouter'
    cxx_header = "mem/ruby/network/BasicRouter.hh"
    cxx_class = 'gem5::ruby::BasicRouter'

    router_id = Param.Int("ID in relation to other routers")

    # only used by garnet (default is 1-cycle latency)
    latency   = Param.Cycles(1, "number of cycles inside router")

#&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
# BasicBus is a ClockedObject
class BasicBus(ClockedObject):
    type = 'BasicBus'
    cxx_header = "mem/ruby/network/BasicRouter.hh"
    cxx_class = 'gem5::ruby::BasicBus'

    bus_id = Param.Int("ID in relation to other busses")

    # only used by garnet (default is 1-cycle latency)
    latency   = Param.Cycles(1, "number of cycles inside bus")
#&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

#**************************************************************
# BasicCrossbar is a ClockedObject
class BasicCrossbar(ClockedObject):
    type = 'BasicCrossbar'
    cxx_header = "mem/ruby/network/BasicRouter.hh"
    cxx_class = 'gem5::ruby::BasicCrossbar'

    crossbar_id = Param.Int("ID in relation to other crossbars")

    # only used by garnet (default is 1-cycle latency)
    latency   = Param.Cycles(1, "number of cycles inside crossbar")
#**************************************************************

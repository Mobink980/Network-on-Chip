After the changes made, the segmentation fault persisted.

1. Completely Make Onyx like Garnet. There is no problem with OnyxTest topology. It runs with garnet and it should run with onyx as well. Specifically, get rid of  the links to unnecessarry objects like Chain and Configuration and run Onyx without the garnet in the network directory. You absolutely need to get a successful test at this point. (A slight change of plan: gonna build emerald from garnet and then we build the new onyx from emerald without touching the emerald anymore. Emerald will be the fail-safe and would remain intact)

2. After You have done that, we have the current state of the onyx in the videos directory. As our assumption that the segfault problem was originating from the modified NI failed, this time, you don't need to worry about having garnet and onyx at the same time. We first revert back step by step to the designed onyx, and try to see where a problem rises (with the same Topology and Network objects). 

3. We can still test both garnet and onyx and compare their results. This time, we make sure both garnet and onyx are operational on their own before trying to have them at the same time for easier testing.

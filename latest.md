After the changes made, the segmentation fault persisted.

1. Build emerald from garnet and then we build the new onyx from emerald without touching the emerald anymore. Emerald will be the fail-safe and would remain intact. First try building and running the OnyxTest topology without garnet and onyx in place. If not successful, make it more like garnet until successful. If succeeded, make emerald to run beside garnet and onyx and again, make sure things work out (this time make sure the changes to files outside network directory are recorded).

2. At this point, you need to have garnet and emerald work alongside, But this triumph does not give us much. Because emerald is a fail-safe version of onyx that looks like onyx but lacks the functionality to make single-hop transfer across all layers. Therefore, you need to start from emerald and make your way back to the current onyx (in the videos directory) step by step without facing segfault. As segfault cannot be traced and resolved, we need to find out which portion is causing it, so we can rewrite it until it is obviated.

3. If the previous two stages were a success, we now have a single-hop NoC in vertical layers. Then we can go after testing, revising, and improving until an acceptable outcome is seen. We talk to Shekarian after that to see how to go forward.

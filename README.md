# SoftIO
A high-level MCU manipulation library through USB or any serial port, with memory sync paradigm

## Design——why software-defined and why memory sync?

I developed several MCU project before I realized the necessity of a framework to communicate with PC. The requirements could vary with different project but the essential functionalities remain the same, concluded below:

1. read/write data
2. read/write stream

For example, PC may want to write data to MCU to set the DAC value, and even MCU may want to write to PC if something happened (just like interrupt). With simple read and write, one could realize most of things, however, may not suitable for high-load systems. The point is that, if you want to read ADC data at about 4Mbps but the overall throughput is limited to 6Mbps, that would be a hard problem to solve, which really occurs to me when I was developing a customized SDR (Software-Defined Radio) device on visible light communication. The continuity and packet-loss-free requirement leads to the special design of stream I/O, which exactly reach the high-load requirement above.

In my experience, developing a MCU program is annoying with long waiting of compiling, flashing, debugging. SDR inspired my that for a research project, a flexible configuration is much more valuable than compact design of MCU system. For example, although you may not want to change the timer frequency now, it would be great if you could change it only modify the program on PC, without recompiling the firmware and flashing it (possibly even debug new program). It concludes as below:

> Every parameter should be adjustable outside MCU, at least those needed

Then how to control? For a beginner, a string parser would be easy to implement using `sscanf` on MCU, however, suffered poor performance. Others would like to use or design packet structure and decode them at MCU side, which leads to difficulty in developing and debugging. Actually I did try those method and finally I came up with the idea of `memory synchronization`. For most use case, data and I/O are asynchronous, which means you may not read ADC data exactly when MCU receive your read request, but often in a fixed time interval. When you read ADC in timer interrupt, you don't know whether to throw away the data or give it to a previous read request, just save it somewhere in memory. In this condition, a memory synchronization would work if PC request to sync the data from MCU to PC, then PC would have a copy of the variable in MCU memory. It works perfectly for most of time, MCU would ignore the existing of PC, just do its own work of writing data to specific variable in memory. Then, I want to say, even those procedure call would be realized by the memory synchronization scheme, by adding hook functions for read and write. Assuming that MCU would write to GPIO when PC requests, it simply add a hook function that modify the GPIO register when the write request satisfy some requirements, e.g. exactly writing to a specific address.
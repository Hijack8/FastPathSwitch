# A l2 switch

This is a switch implemented by dpdk, using the run-to-completion model.

## Features

- RTC (run-to-completion) model

- High performance

- l2 self learning && l2 forwarding

## Environment

- DPDK 23.07

- gcc 11.4

- Ubuntu 22.04.1 LTS

- Intel(R) Xeon(R) CPU E5-2686 v4 @ 2.30GHz

## building

Build DPDK via [Compilation of the DPDK](https://doc.dpdk.org/guides-24.03/linux_gsg/sys_reqs.html#compilation-of-the-dpdk)

## Running

```
make
sudo ./build/switch -c 0x55 -- -p 0xf 
```

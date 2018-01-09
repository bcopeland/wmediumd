# Introduction

This is a wireless medium simulation tool for Linux, based on the netlink API
implemented in the `mac80211_hwsim` kernel driver.  Unlike the default in-kernel
forwarding mode of `mac80211_hwsim`, wmediumd allows simulating frame loss and
delay.

This version is forked from an earlier version, hosted here:

    https://github.com/cozybit/wmediumd

# Prerequisites

First, you need a recent Linux kernel with the `mac80211_hwsim` module
available.  If you do not have this module, you may be able to build it using
the [backports project](https://backports.wiki.kernel.org/index.php/Main_Page).

Wmediumd requires libnl3.0.

# Building
```
cd wmediumd && make
```

# Using Wmediumd

Starting wmediumd with an appropriate config file is enough to make frames
pass through wmediumd:
```
sudo modprobe mac80211_hwsim radios=2
sudo ./wmediumd/wmediumd -c tests/2node.cfg &
# run some hwsim test
```
However, please see the next section on some potential pitfalls.

A complete example using network namespaces is given at the end of
this document.

# Configuration

Wmediumd supports multiple ways of configuring the wireless medium.

## Perfect medium

With this configuration, all traffic flows between the configured interfaces, identified by their mac address:

```
ifaces :
{
	ids = [
		"02:00:00:00:00:00",
		"02:00:00:00:01:00",
		"02:00:00:00:02:00",
		"02:00:00:00:03:00"
	];
};
```

## Per-link loss probability model

You can simulate a slightly more realistic channel by assigning fixed error
probabilities to each link.

```
ifaces :
{
	ids = [
		"02:00:00:00:00:00",
		"02:00:00:00:01:00",
		"02:00:00:00:02:00",
		"02:00:00:00:03:00"
	];
};

model:
{
	type = "prob";

	default_prob = 1.0;
	links = (
		(0, 2, 0.000000),
		(2, 3, 0.000000)
	);
};
```

The above configuration would assign 0% loss probability (perfect medium) to
all frames flowing between nodes 0 and 2, and 100% loss probability to all
other links.

This is a very simplistic model that does not take into account that losses
depend on transmission rates and signal-to-noise ratio.  For that, keep reading.

## Asymmetric loss probability

To model links with asymmetric probabilities you can use the model type "prob_asymmetric".

```
ifaces :
{
	ids = [
		"02:00:00:00:00:00",
		"02:00:00:00:01:00",
		"02:00:00:00:02:00",
		"02:00:00:00:03:00"
	];
};

model:
{
	type = "prob_asymmetric";

	default_prob = 1.0;
	links = (
		(0, 2, 0.200000),
		(2, 0, 0.800000)
		(0, 1, 0.500000)
	);
};
```

The above configuration would assign 20% loss probability to all frames flowing
between nodes 0 and 2, and 80% loss probability to frames flowing between 2 and
0.

If only one link direction is explicitly listed in the `links` section, the
opposite direction will be assigned the `default_prob`.  In the example above,
traffic originated in node 1 and destined to node 0 would be assigned
100% loss probability.


## Per-link signal-to-noise ratio (SNR) model

You can model different signal-to-noise ratios for each link by including a
list of link tuples in the form of (sta1, sta2, snr).

```
ifaces :
{
	ids = [
		"02:00:00:00:00:00",
		"02:00:00:00:01:00",
		"02:00:00:00:02:00",
		"02:00:00:00:03:00"
	];

	links = (
		(0, 1, 0),
		(0, 2, 0),
		(0, 3, 0),
		(1, 2, 30),
		(1, 3, 10),
		(2, 3, 20)
	);
};
```

The snr values configured this way are assumed to be symmetrical.
The snr will affect the maximum data rates that are successfully transmitted
over the link.

The packet loss error probabilities are derived from this snr.  See function
`get_error_prob_from_snr()`.  Or you can provide a packet-error-rate table like
the one in `tests/signal_table_ieee80211ax`

## Path loss model

The path loss model derives signal-to-noise and probabilities from the
coordinates of each node.  This is an example configuration file for it.

```
ifaces : {...};
model :
{
	type = "path_loss";
	positions = (
		(-50.0,   0.0),
		(  0.0,  40.0),
		(  0.0, -70.0),
		( 50.0,   0.0)
	);
	directions = (
		(  0.0,   0.0),
		(  0.0,  10.0),
		(  0.0,  10.0),
		(  0.0,   0.0)
	);
	tx_powers = (15.0, 15.0, 15.0, 15.0);

	model_name = "log_distance";
	path_loss_exp = 3.5;
	xg = 0.0;
};
```

## Gotchas

### Allowable MAC addresses

The kernel only allows wmediumd to work on the second available hardware
address, which has bit 6 set in the most significant octet
(i.e. 42:00:00:xx:xx:xx, not 02:00:00:xx:xx:xx).  Set this appropriately
using 'ip link set address'.

This issue was fixed in commit cd37a90b2a417e5882414e19954eeed174aa4d29
in Linux, released in kernel 4.1.0.

### Rates

wmediumd's rate table is currently hardcoded to 802.11a OFDM rates.
Therefore, either operate wmediumd networks in 5 GHz channels, or supply
a rateset for the BSS with no CCK rates.

### Send-to-self

By default, traffic between local devices in Linux will not go over
the wire / wireless medium.  This is true of vanilla hwsim as well.
In order to make this happen, you need to either run the hwsim interfaces
in separate network namespaces, or you need to set up routing rules with
the hwsim devices at a higher priority than local forwarding.

`tests/test-001.sh` contains an example of the latter setup.

# Example session

The following sequence of commands establishes a two-node mesh using network
namespaces.
```
sudo modprobe -r mac80211_hwsim
sudo modprobe mac80211_hwsim
sudo ./wmediumd/wmediumd -c ./tests/2node.cfg

# in window 2
sudo lxc-unshare -s NETWORK bash
ps | grep bash  # note pid

# in window 1
sudo iw phy phy2 set netns $pid

sudo ip link set wlan1 down
sudo iw dev wlan1 set type mp
sudo ip link set addr 42:00:00:00:00:00 dev wlan1
sudo ip link set wlan1 up
sudo ip addr add 10.10.10.1/24 dev wlan1
sudo iw dev wlan1 set channel 149
sudo iw dev wlan1 mesh join meshabc

# in window 2
ip link set lo

sudo ip link set wlan2 down
sudo iw dev wlan2 set type mp
sudo ip link set addr 42:00:00:00:01:00 dev wlan2
sudo ip link set wlan2 up
sudo ip addr add 10.10.10.2/24 dev wlan2
sudo iw dev wlan2 set channel 149
sudo iw dev wlan2 mesh join meshabc

iperf -u -s -i 10 -B 10.10.10.2

# in window 1
iperf -u -c 10.10.10.2 -b 100M -i 10 -t 120
```

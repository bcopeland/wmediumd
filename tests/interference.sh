#!/bin/bash
# 3 mesh nodes in a linear topology
# 4 additional mesh nodes exists to prevent transmission
# When enable_interference=true, ping always fails.
# (This test is not perfect because of random values)

num_nodes=7
session=wmediumd
subnet=10.10.10
macfmt='02:00:00:00:%02x:00'

. func

if [[ $UID -ne 0 ]]; then
	echo "Sorry, run me as root."
	exit 1
fi

modprobe -r mac80211_hwsim
modprobe mac80211_hwsim radios=$num_nodes
iw reg set US

for i in `seq 0 $((num_nodes-1))`; do
	addrs[$i]=`printf $macfmt $i`
done

cat <<__EOM > diamond.cfg
ifaces :
{
	ids = [
		"02:00:00:00:00:00",
		"02:00:00:00:01:00",
		"02:00:00:00:02:00",
		"02:00:00:00:03:00",
		"02:00:00:00:04:00",
		"02:00:00:00:05:00",
		"02:00:00:00:06:00"
	];
	enable_interference = true;
};

model :
{
	band = 2;
	type = "path_loss";
	positions = (
		(-69.0,   0.0),
		(  0.0,   0.0),
		( 69.0,   0.0),
		(130.0,  -2.0),
		(130.0,  -1.0),
		(130.0,   2.0),
		(130.0,   1.0)
	);
	tx_powers = (15.0, 15.0, 15.0, 11.0, 11.0, 11.0, 11.0);

	model_name = "log_distance";
	path_loss_exp = 3.5;
	xg = 0.0;
};
__EOM

tmux new -s $session -d

rm /tmp/netns.pid.* 2>/dev/null
i=0
for addr in ${addrs[@]}; do
	phy=`addr2phy $addr`
	dev=`ls /sys/class/ieee80211/$phy/device/net`
	phys[$i]=$phy
	devs[$i]=$dev

	ip=${subnet}.$((10 + i))

	# put this phy in own netns and tmux window, and start a mesh node
	win=$session:$((i+1)).0
	tmux new-window -t $session -n $ip

	# start netns
	pidfile=/tmp/netns.pid.$i
	tmux send-keys -t $win 'lxc-unshare -s NETWORK /bin/bash' C-m
	tmux send-keys -t $win 'echo $$ > '$pidfile C-m

	# wait for netns to exist
	while [[ ! -e $pidfile ]]; do
		echo "Waiting for netns $i -- $pidfile"
		sleep 0.5
	done

	tmux send-keys -t $session:0.0 'iw phy '$phy' set netns `cat '$pidfile'`' C-m

	# wait for phy to exist in netns
	while [[ -e /sys/class/ieee80211/$phy ]]; do
		echo "Waiting for $phy to move to netns..."
		sleep 0.5
	done

	# start mesh node
	tmux send-keys -t $win '. func' C-m
	tmux send-keys -t $win 'meshup-iw '$dev' diamond 2412 '$ip C-m

	i=$((i+1))
done
winct=$i

# start wmediumd
win=$session:$((winct+1)).0
winct=$((winct+1))
tmux new-window -a -t $session -n wmediumd
tmux send-keys -t $win '../wmediumd/wmediumd -c diamond.cfg' C-m

# start iperf server on 10.10.10.14
tmux send-keys -t $session:5 'iperf -s' C-m
# start iperf server on 10.10.10.16
tmux send-keys -t $session:7 'iperf -s' C-m

# enable monitor
tmux send-keys -t $session:0 'ip link set hwsim0 up' C-m

tmux send-keys -t $session:4 'iperf -u -b 100M -c 10.10.10.14 -t 10' C-m
tmux send-keys -t $session:6 'iperf -u -b 100M -c 10.10.10.16 -t 10' C-m

tmux select-window -t $session:1
tmux send-keys -t $session:1 'sleep 2; ping -c 5 -W 1 10.10.10.12' C-m

tmux attach

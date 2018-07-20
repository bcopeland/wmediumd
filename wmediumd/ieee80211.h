/*
 *	wmediumd, wireless medium simulator for mac80211_hwsim kernel module
 *	Copyright (c) 2011 cozybit Inc.
 *
 *	Author:	Javier Lopez	<jlopex@cozybit.com>
 *		Javier Cardona	<javier@cozybit.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version 2
 *	of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *	02110-1301, USA.
 */

#ifndef IEEE80211_H_
#define IEEE80211_H_

#define IEEE80211_AVAILABLE_RATES 12
#define IEEE80211_TX_MAX_RATES 4
#define IEEE80211_NUM_ACS 4

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define FCTL_FTYPE		0x0c
#define FCTL_STYPE		0xf0

#define FCTL_TODS		0x01
#define FCTL_FROMDS		0x02

#define FTYPE_MGMT		0x00
#define FTYPE_DATA		0x08

#define STYPE_QOS_DATA		0x80
#define STYPE_AUTH		0xb0
#define STYPE_ACTION		0xd0

#define QOS_CTL_TAG1D_MASK	0x07

enum ieee80211_ac_number {
	IEEE80211_AC_VO		= 0,
	IEEE80211_AC_VI		= 1,
	IEEE80211_AC_BE		= 2,
	IEEE80211_AC_BK		= 3,
};

static const enum ieee80211_ac_number ieee802_1d_to_ac[8] = {
	IEEE80211_AC_BE,
	IEEE80211_AC_BK,
	IEEE80211_AC_BK,
	IEEE80211_AC_BE,
	IEEE80211_AC_VI,
	IEEE80211_AC_VI,
	IEEE80211_AC_VO,
	IEEE80211_AC_VO
};

struct ieee80211_hdr {
	unsigned char frame_control[2];
	unsigned char duration_id[2];
	unsigned char addr1[ETH_ALEN];
	unsigned char addr2[ETH_ALEN];
	unsigned char addr3[ETH_ALEN];
	unsigned char seq_ctrl[2];
	union {
		unsigned char addr4[ETH_ALEN];
		unsigned char data[0];
	};
};

#endif /* IEEE80211_H_ */

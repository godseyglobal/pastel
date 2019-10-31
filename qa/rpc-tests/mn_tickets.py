#!/usr/bin/env python2
# Copyright (c) 2018-19 The Pastel developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from __future__ import print_function

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, \
    assert_false, assert_true, initialize_chain_clean, \
    initialize_datadir, start_nodes, start_node, connect_nodes_bi, \
    bitcoind_processes, wait_and_assert_operationid_status, p2p_port, \
    stop_node
from mn_common import MasterNodeCommon
from test_framework.authproxy import JSONRPCException
import json
import time
import base64

from decimal import Decimal, getcontext
getcontext().prec = 16

# 12 Master Nodes
private_keys_list = ["91sY9h4AQ62bAhNk1aJ7uJeSnQzSFtz7QmW5imrKmiACm7QJLXe", #0
                     # "923JtwGJqK6mwmzVkLiG6mbLkhk1ofKE1addiM8CYpCHFdHDNGo", #1
                     # "91wLgtFJxdSRLJGTtbzns5YQYFtyYLwHhqgj19qnrLCa1j5Hp5Z", #2
                     # "92XctTrjQbRwEAAMNEwKqbiSAJsBNuiR2B8vhkzDX4ZWQXrckZv", #3
                     # "923JCnYet1pNehN6Dy4Ddta1cXnmpSiZSLbtB9sMRM1r85TWym6", #4
                     # "93BdbmxmGp6EtxFEX17FNqs2rQfLD5FMPWoN1W58KEQR24p8A6j", #5
                     # "92av9uhRBgwv5ugeNDrioyDJ6TADrM6SP7xoEqGMnRPn25nzviq", #6
                     # "91oHXFR2NVpUtBiJk37i8oBMChaQRbGjhnzWjN9KQ8LeAW7JBdN", #7
                     # "92MwGh67mKTcPPTCMpBA6tPkEE5AK3ydd87VPn8rNxtzCmYf9Yb", #8
                     # "92VSXXnFgArfsiQwuzxSAjSRuDkuirE1Vf7KvSX7JE51dExXtrc", #9
                     # "91hruvJfyRFjo7JMKnAPqCXAMiJqecSfzn9vKWBck2bKJ9CCRuo", #10
                     # "92sYv5JQHzn3UDU6sYe5kWdoSWEc6B98nyY5JN7FnTTreP8UNrq"  #11
                     ]

class MasterNodeTicketsTest (MasterNodeCommon):
    number_of_master_nodes = len(private_keys_list)
    number_of_simple_nodes = 2
    non_mn1 = number_of_master_nodes
    non_mn2 = number_of_master_nodes+1
    total_number_of_nodes = number_of_master_nodes+number_of_simple_nodes
    mining_node_num = number_of_master_nodes
    hot_node_num = number_of_master_nodes+1

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, self.total_number_of_nodes)

    def setup_network(self, split=False):
        self.nodes = []
        self.is_network_split = False
        self.setup_masternodes_network(private_keys_list, self.number_of_simple_nodes)

    def run_test (self):
        # self.mining_enough(self.mining_node_num, self.number_of_master_nodes)
        # cold_nodes = {k: v for k, v in enumerate(private_keys_list)}
        # _, _, _ = self.start_mn(self.mining_node_num, self.hot_node_num, cold_nodes, self.total_number_of_nodes)

        self.reconnect_nodes(0, self.number_of_master_nodes)
        self.sync_all()


        errorString = ""

        print("== Pastelid test ==")
        #1. pastelid tests
        #a. Generate new PastelID and associated keys (EdDSA448). Return PastelID base58-encoded
        #a.a - generate with no errors two keys at MN and non-MN

        pastelid_mn0_1 = self.nodes[0].pastelid("newkey", "passphrase")["pastelid"]
        assert_true(pastelid_mn0_1, "No Pastelid was created")
        pastelid_mn0_2 = self.nodes[0].pastelid("newkey", "passphrase")["pastelid"]
        assert_true(pastelid_mn0_2, "No Pastelid was created")

        pastelid_nonmn1_1 = self.nodes[self.non_mn1].pastelid("newkey", "passphrase")["pastelid"]
        assert_true(pastelid_nonmn1_1, "No Pastelid was created")
        pastelid_nonmn1_2 = self.nodes[self.non_mn1].pastelid("newkey", "passphrase")["pastelid"]
        assert_true(pastelid_nonmn1_2, "No Pastelid was created")

        #a.b - fail if empty passphrase
        try:
            self.nodes[self.non_mn1].pastelid("newkey", "")
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("passphrase for new key cannot be empty" in errorString, True)

        #b. Import private "key" (EdDSA448) as PKCS8 encrypted string in PEM format. Return PastelID base58-encoded
        # NOT IMPLEMENTED

        #c. List all internally stored PastelID and keys
        idlist = self.nodes[0].pastelid("list")
        idlist = dict((key+str(i), val) for i,k in enumerate(idlist) for key, val in k.items())
        print(idlist)
        assert_true(pastelid_mn0_1 in idlist.values(), "PastelID " + pastelid_mn0_1 + " not in the list")
        assert_true(pastelid_mn0_2 in idlist.values(), "PastelID " + pastelid_mn0_2 + " not in the list")

        idlist = self.nodes[self.non_mn1].pastelid("list")
        idlist = dict((key+str(i), val) for i,k in enumerate(idlist) for key, val in k.items())
        print(idlist)
        assert_true(pastelid_nonmn1_1 in idlist.values(), "PastelID " + pastelid_nonmn1_1 + " not in the list")
        assert_true(pastelid_nonmn1_2 in idlist.values(), "PastelID " + pastelid_nonmn1_2 + " not in the list")

        print("Pastelid test: 2 PastelID's each generate at node0 (MN ) and node" + str(self.non_mn1) + "(non-MN)")

        #d. Sign "text" with the internally stored private key associated with the PastelID
        #d.a - sign with no errors using key from 1.a.a
        signature_mn0_1 = self.nodes[0].pastelid("sign", "1234567890", pastelid_mn0_1, "passphrase")["signature"]
        assert_true(signature_mn0_1, "No signature was created")
        assert_equal(len(base64.b64decode(signature_mn0_1)), 114)

        #e. Sign "text" with the private "key" (EdDSA448) as PKCS8 encrypted string in PEM format
        # NOT IMPLEMENTED

        #f. Verify "text"'s "signature" with the PastelID
        #f.a - verify with no errors using key from 1.a.a
        result = self.nodes[0].pastelid("verify", "1234567890", signature_mn0_1, pastelid_mn0_1)["verification"]
        assert_equal(result, "OK")
        #f.b - fail to verify with the different key from 1.a.a
        result = self.nodes[0].pastelid("verify", "1234567890", signature_mn0_1, pastelid_mn0_2)["verification"]
        assert_equal(result, "Failed")
        #f.c - fail to verify modified text
        result = self.nodes[0].pastelid("verify", "1234567890AAA", signature_mn0_1, pastelid_mn0_1)["verification"]
        assert_equal(result, "Failed")

        print("Pastelid test: Message signed and verified")

        print("== Tickets test ==")
        #2. tickets tests
        #a. PastelID ticket
        #   a.a register MN PastelID
        #       a.a.1 fail if not active MN
        #       a.a.2 fail if active MN, but not enough coins - ~11PSL
        #       a.a.3 register without errors from active MN with enough coins
        #       a.a.4 from another node - get ticket transaction and check
        #           - there are P2MS outputs with non-zerro amounts
        #           - amounts is totaling 10PSL
        #       a.a.5 fail if already registered
        #   a.b find MN PastelID ticket
        #       a.b.1 by PastelID
        #       a.b.2 by Collateral output, compare to ticket from a.b.1
        #       a.b.3 verify ticket:
        #           - signature matches pastelID
        #           - correct MN info
        #   a.c get the same ticket by txid from a.a.3 and compare with ticket from a.b.1
        #   a.d list all id tickets, check PastelIDs

        #b. personal PastelID ticket
        #   b.a register personal PastelID
        #       b.a.1 fail from non MN, if not enough coins - ~11PSL
        #       b.a.2 register without errors from non MN with enough coins
        #       b.a.3 fail from MN, if not enough coins - ~11PSL
        #       b.a.4 register without errors from MN with enough coins
        #       b.a.5 from another node - get ticket transaction and check
        #           - there are P2MS outputs with non-zerro amounts
        #           - amounts is totaling 10PSL
        #       b.a.6 fail if already registered
        #   b.b find personal PastelID
        #       b.b.1 by PastelID
        #       b.b.2 verify ticket:
        #           - signature matches pastelID
        #           - correct MN info
        #   b.c get the ticket by txid from b.a.3 and compare with ticket from b.b.1
        #   b.d list all id tickets, check PastelIDs

        #c. art registration ticket
        #   c.a register art registration ticket
        #       c.a.1 fail if not active MN
        #       c.a.2 fail if artist's signature is not matching
        #       c.a.3 fail if MN2 and MN3 signatures are not matching
        #       c.a.4 fail if artist's PastelID is not registered
        #       c.a.5 fail if MN1, MN2 and MN3 are not from top 10 list at the ticket's blocknum
        #       c.a.6 register without errors, if enough coins for tnx fee
        #       c.a.7 fail if already registered
        #   c.b find registration ticket
        #       c.b.1 by artists PastelID (can be multiple)
        #       c.b.2 by hash
        #       c.b.3 by fingerprints, compare to ticket from c.b.2
        #       c.b.4 verify ticket:
        #           - all signatures match PastelIDs
        #           - all PastelIDs are registered
        #           - MN1, MN2 and MN3 are from top 10 list at the ticket's blocknum
        #   c.c get the same ticket by txid from c.a.6 and compare with ticket from c.b.2
        #   c.d list all art registration tickets, check PastelIDs

        #d. art activation ticket
        #   d.a register art activation ticket
        #       d.a.1 fail if not enough coins to pay 90% of registration price (from artReg ticket) + tnx fee
        #       d.a.2 fail if artist's PastelID in the activation ticket is not matching artist's PastelID in the registration ticket
        #       d.a.3 register without errors, if enough coins for tnx fee
        #       d.a.4 fail if already registered
        #       d.a.5 fail if Registration Ticket is already activated (is it the same as c.a.4?)
        #       d.a.6 from another node - get ticket transaction and check
        #           - there are 3 outputs to MN1, MN2 and MN3 with correct amounts (MN1: 60%; MN2, MN3: 20% each, of registration price)
        #   d.b find activation ticket
        #       d.b.1 by artists PastelID (can be multiple)
        #       d.b.2 by Registration height - reg_height from registration ticket
        #       d.b.3 by Registration txid - reg_txid from registration ticket, compare to ticket from d.b.2
        #       d.b.4 verify ticket:
        #           - signatures matches PastelID
        #           - PastelID is registered and is personal
        #           - Registration ticket from reg_height is valid and has artist's PastelID
        #           - Registration ticket at reg_txid is valid and has artist's PastelID
        #   d.c get the same ticket by txid from d.a.3 and compare with ticket from d.b.2
        #   a.d list all art registration tickets, check PastelIDs



        #3. storagefee tests
        #a. Get Network median storage fee
        #a.1 from non-MN without errors
        #a.2 from MN without errors
        #a.3 compare a.1 and a.2

        #b. Get local masternode storage fee
        #b.1 fail from non-MN
        #b.2 from MN without errors
        #b.3 compare b.2 and a.1

        #c. Set storage fee for MN
        #c.1 fail on non-MN
        #c.2 on MN without errors
        #c.3 get local MN storage fee and compare it with c.2


        #4. chaindata tests
        #No need


        # print("Register PastelIDs for non MN node", self.non_mn2, ". Will fail - no coins")
        # address2 = self.nodes[self.non_mn2].getnewaddress()
        # pastelid2 = self.nodes[self.non_mn2].pastelid("newkey", "passphrase")["pastelid"]
        # assert_true(pastelid2, "No Pastelid was created")
        #
        # try:
        #     self.nodes[self.non_mn2].tickets("register", "id", pastelid2, "passphrase", address2)["txid"]
        # except JSONRPCException,e:
        #     errorString = e.error['message']
        # assert_equal("No unspent transaction found" in errorString, True)
        # print("Ticket not create - No unspent transaction found")
        #
        # print("Register PastelIDs - non MN node", self.non_mn1)
        # # this is mining node
        # address1 = self.nodes[self.non_mn1].getnewaddress()
        # pastelid1 = self.nodes[self.non_mn1].pastelid("newkey", "passphrase")["pastelid"]
        # assert_true(pastelid1, "No Pastelid was created")
        #
        # ticket1_txid = self.nodes[self.non_mn1].tickets("register", "id", pastelid1, "passphrase", address1)["txid"]
        # assert_true(ticket1_txid, "No ticket was created")
        #
        # print("Check PastelIDs on another node")
        # time.sleep(2)
        # ticket1 = self.nodes[self.non_mn2].tickets("get", ticket1_txid)
        # print(ticket1)
        # json_ticket1 = json.loads(ticket1)
        # assert_equal(json_ticket1['ticket']['type'], "pastelid")
        # assert_equal(json_ticket1['ticket']['id_type'], "personal")
        # assert_equal(json_ticket1['ticket']['pastelID'], pastelid1)
        # assert_equal(json_ticket1['height'], -1)
        #
        # print("Check same ticket after new block (on another node)")
        # self.nodes[self.mining_node_num].generate(1)
        # time.sleep(2)
        # ticket1 = self.nodes[self.non_mn2].tickets("get", ticket1_txid)
        # print(ticket1)
        # json_ticket1 = json.loads(ticket1)
        # assert_equal(json_ticket1['ticket']['type'], "pastelid")
        # assert_equal(json_ticket1['ticket']['id_type'], "personal")
        # assert_equal(json_ticket1['ticket']['pastelID'], pastelid1)
        #
        # current_block = self.nodes[self.non_mn1].getblockcount()
        # assert_equal(json_ticket1['height'], current_block)

        # self.nodes[self.mining_node_num].sendtoaddress(address2, 10, "", "", False)


if __name__ == '__main__':
    MasterNodeTicketsTest ().main ()
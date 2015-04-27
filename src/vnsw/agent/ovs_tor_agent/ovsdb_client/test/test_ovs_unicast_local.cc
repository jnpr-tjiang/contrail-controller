/*
 * Copyright (c) 2015 Juniper Networks, Inc. All rights reserved.
 */

#include "base/os.h"
#include "testing/gunit.h"

#include <base/logging.h>
#include <io/event_manager.h>
#include <io/test/event_manager_test.h>
#include <tbb/task.h>
#include <base/task.h>

#include <cmn/agent_cmn.h>

#include "cfg/cfg_init.h"
#include "cfg/cfg_interface.h"
#include "pkt/pkt_init.h"
#include "services/services_init.h"
#include "vrouter/ksync/ksync_init.h"
#include "oper/interface_common.h"
#include "oper/nexthop.h"
#include "oper/tunnel_nh.h"
#include "route/route.h"
#include "oper/vrf.h"
#include "oper/mpls.h"
#include "oper/vm.h"
#include "oper/vn.h"
#include "filter/acl.h"
#include "openstack/instance_service_server.h"
#include "test_cmn_util.h"
#include "vr_types.h"

#include "openstack/instance_service_server.h"
#include "xmpp/xmpp_init.h"
#include "xmpp/test/xmpp_test_util.h"
#include "vr_types.h"
#include "control_node_mock.h"
#include "xml/xml_pugi.h"
#include "controller/controller_peer.h"
#include "controller/controller_export.h"
#include "controller/controller_vrf_export.h"

#include "ovs_tor_agent/ovsdb_client/ovsdb_route_peer.h"
#include "ovs_tor_agent/ovsdb_client/physical_switch_ovsdb.h"
#include "ovs_tor_agent/ovsdb_client/logical_switch_ovsdb.h"
#include "ovs_tor_agent/ovsdb_client/physical_port_ovsdb.h"
#include "ovs_tor_agent/ovsdb_client/unicast_mac_local_ovsdb.h"
#include "test_ovs_agent_init.h"
#include "test-xml/test_xml.h"
#include "test-xml/test_xml_oper.h"
#include "test_xml_physical_device.h"
#include "test_xml_ovsdb.h"

#include "test_ovs_agent_util.h"

#include <ovsdb_types.h>

using namespace pugi;
using namespace OVSDB;
using namespace boost::uuids;

EventManager evm1;
ServerThread *thread1;
test::ControlNodeMock *bgp_peer1;

EventManager evm2;
ServerThread *thread2;
test::ControlNodeMock *bgp_peer2;

void RouterIdDepInit(Agent *agent) {
    Agent::GetInstance()->controller()->Connect();
}

class UnicastLocalRouteTest : public ::testing::Test {
protected:
    UnicastLocalRouteTest() {
    }

    UnicastMacLocalEntry *FindUcastLocal(const string &logical_switch,
                                         const string &mac) {
        UnicastMacLocalOvsdb *table =
            tcp_session_->client_idl()->unicast_mac_local_ovsdb();
        UnicastMacLocalEntry key(table, logical_switch, mac);
        UnicastMacLocalEntry *entry =
            static_cast<UnicastMacLocalEntry *> (table->Find(&key));
        return entry;
    }

    virtual void SetUp() {
        agent_ = Agent::GetInstance();
        init_ = static_cast<TestOvsAgentInit *>(client->agent_init());
        peer_manager_ = init_->ovs_peer_manager();
        WAIT_FOR(100, 10000,
                 (tcp_session_ = static_cast<OvsdbClientTcpSession *>
                  (init_->ovsdb_client()->NextSession(NULL))) != NULL);
        WAIT_FOR(100, 10000,
                 (tcp_session_->client_idl() != NULL));
        WAIT_FOR(100, 10000, (tcp_session_->status() == string("Established")));

        AgentUtXmlTest test("controller/src/vnsw/agent/ovs_tor_agent/ovsdb_"
                            "client/test/xml/ucast-local-test-setup.xml");
        // set current session in test context
        OvsdbTestSetSessionContext(tcp_session_);
        AgentUtXmlOperInit(&test);
        AgentUtXmlPhysicalDeviceInit(&test);
        AgentUtXmlOvsdbInit(&test);
        if (test.Load() == true) {
            test.ReadXml();
            string str;
            test.ToString(&str);
            cout << str << endl;
            test.Run();
        }
    }

    virtual void TearDown() {
        AgentUtXmlTest test("controller/src/vnsw/agent/ovs_tor_agent/ovsdb_"
                            "client/test/xml/ucast-local-test-teardown.xml");
        // set current session in test context
        AgentUtXmlOperInit(&test);
        AgentUtXmlPhysicalDeviceInit(&test);
        AgentUtXmlOvsdbInit(&test);
        if (test.Load() == true) {
            test.ReadXml();
            string str;
            test.ToString(&str);
            cout << str << endl;
            test.Run();
        }
    }

    Agent *agent_;
    TestOvsAgentInit *init_;
    OvsPeerManager *peer_manager_;
    OvsdbClientTcpSession *tcp_session_;
};

TEST_F(UnicastLocalRouteTest, UnicastLocalBasic) {
    LogicalSwitchTable *table =
        tcp_session_->client_idl()->logical_switch_table();
    LogicalSwitchEntry key(table, UuidToString(MakeUuid(1)));
    LogicalSwitchEntry *entry = static_cast<LogicalSwitchEntry *>
        (table->Find(&key));
    EXPECT_TRUE((entry != NULL));
    if (entry != NULL) {
        WAIT_FOR(10, 10000,
                 (true == add_ucast_mac_local(entry->name(),
                                              "00:00:00:00:01:01",
                                              "11.11.11.11")));
        // Wait for entry to add
        WAIT_FOR(100, 10000,
                 (NULL != FindUcastLocal(entry->name(), "00:00:00:00:01:01")));

        WAIT_FOR(10, 10000,
                 (true == del_ucast_mac_local(entry->name(),
                                              "00:00:00:00:01:01")));
        // Wait for entry to del
        WAIT_FOR(100, 10000,
                 (NULL == FindUcastLocal(entry->name(), "00:00:00:00:01:01")));
    }
}

int main(int argc, char *argv[]) {
    GETUSERARGS();
    // override with true to initialize ovsdb server and client
    ksync_init = true;
    client = OvsTestInit(init_file, ksync_init);
    int ret = RUN_ALL_TESTS();
    TestShutdown();
    return ret;
}

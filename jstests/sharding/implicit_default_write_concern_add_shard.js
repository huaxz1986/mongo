/**
 * Tests adding shard to sharded cluster will fail if the implicitDefaultWriteConcern is
 * w:1 and CWWC is not set.
 * @tags: [requires_fcv_50, disabled_due_to_server_58295]
 */

(function() {
"use strict";

load("jstests/multiVersion/libs/multi_cluster.js");  // For 'upgradeCluster()'

function addNonArbiterNode(nodeId, rst) {
    const config = rst.getReplSetConfigFromNode();
    config.members.push({_id: nodeId, host: rst.add().host});
    config.version++;
    assert.commandWorked(rst.getPrimary().adminCommand({replSetReconfig: config}));
    assert.soon(() => isConfigCommitted(rst.getPrimary()));
    rst.waitForConfigReplication(rst.getPrimary());
    rst.awaitReplication();
}

function testAddShard(CWWCSet, isPSASet, fixAddShard) {
    jsTestLog("Running sharding test with CWWCSet: " + tojson(CWWCSet) +
              ", isPSASet: " + tojson(isPSASet));
    let replSetNodes = [{}, {}];
    if (isPSASet) {
        replSetNodes = [{}, {}, {arbiter: true}];
    }

    let shardServer = new ReplSetTest(
        {name: "shardServer", nodes: replSetNodes, nodeOptions: {shardsvr: ""}, useHostName: true});
    const conns = shardServer.startSet();
    shardServer.initiate();

    const st = new ShardingTest({
        shards: 0,
        mongos: 1,
    });
    var admin = st.getDB('admin');

    if (CWWCSet) {
        jsTestLog("Setting the CWWC before adding shard.");
        assert.commandWorked(st.s.adminCommand(
            {setDefaultRWConcern: 1, defaultWriteConcern: {w: "majority", wtimeout: 0}}));
    }

    jsTestLog("Attempting to add shard to the cluster");
    if (!CWWCSet && isPSASet) {
        jsTestLog("Adding shard to the cluster should fail.");
        assert.commandFailed(admin.runCommand({addshard: shardServer.getURL()}));

        if (fixAddShard == "setCWWC") {
            jsTestLog("Setting the CWWC to fix addShard.");
            assert.commandWorked(st.s.adminCommand(
                {setDefaultRWConcern: 1, defaultWriteConcern: {w: "majority", wtimeout: 0}}));
        } else {
            jsTestLog("Reconfig shardServer to fix addShard.");
            addNonArbiterNode(3, shardServer);
            addNonArbiterNode(4, shardServer);
        }
    }

    jsTestLog("Adding shard to the cluster should succeed.");
    assert.commandWorked(admin.runCommand({addshard: shardServer.getURL()}));

    st.stop();
    shardServer.stopSet();
}

for (const CWWCSet of [true, false]) {
    for (const isPSASet of [false, true]) {
        if (!CWWCSet && isPSASet) {
            for (const fixAddShard of ["setCWWC", "reconfig"]) {
                testAddShard(CWWCSet, isPSASet, fixAddShard);
            }
        } else {
            testAddShard(CWWCSet, isPSASet);
        }
    }
}
})();

# Phase 8: Real Distributed System Plan

**Status**: 📝 Planning
**Date Started**: TBD
**Target Completion**: TBD
**Branch**: TBD

## Overview

Phase 8 transforms the ProjectKeystone HMAS from a simulated distributed system to a **real multi-node distributed system** using gRPC for network communication, etcd for service discovery, and Raft consensus for leader election and fault tolerance. The system will run across multiple physical or virtual machines with real network partitions and failures.

### Current Status (Post-Phase D.3)

**What We Have**:
- ✅ `SimulatedNetwork` for network simulation
- ✅ `SimulatedCluster` for multi-node testing
- ✅ `SimulatedNUMANode` for NUMA awareness
- ✅ Network partition simulation
- ✅ Packet loss and latency simulation
- ✅ 329/329 tests passing (including distributed tests)

**What Phase 8 Adds**:
- Real gRPC-based network communication
- Service discovery (etcd or Consul)
- Raft consensus for leader election
- Multi-node deployment across machines
- Real network partition handling (split-brain)
- Distributed state management

---

## Phase 8 Architecture

```
┌─────────────────────────────────────────────────────┐
│ Node 1 (datacenter-1)                               │
│  ┌────────────────────────────────────────────┐    │
│  │ ChiefArchitectAgent (Leader via Raft)      │    │
│  │  - gRPC server: 0.0.0.0:50051              │    │
│  │  - etcd client: service registration       │    │
│  └────────────────────────────────────────────┘    │
│  ┌────────────────────────────────────────────┐    │
│  │ gRPC MessageBus Endpoint                   │    │
│  │  - Receives messages from remote nodes     │    │
│  │  - Routes to local agents                  │    │
│  └────────────────────────────────────────────┘    │
└──────────────────┬──────────────────────────────────┘
                   │
                   │ gRPC (TLS)
                   ▼
┌─────────────────────────────────────────────────────┐
│ Node 2 (datacenter-1)                               │
│  ┌────────────────────────────────────────────┐    │
│  │ ComponentLeadAgent (×2)                    │    │
│  │  - gRPC server: 0.0.0.0:50052              │    │
│  └────────────────────────────────────────────┘    │
└──────────────────┬──────────────────────────────────┘
                   │
                   │ gRPC (TLS)
                   ▼
┌─────────────────────────────────────────────────────┐
│ Node 3 (datacenter-2)                               │
│  ┌────────────────────────────────────────────┐    │
│  │ ModuleLeadAgent (×4)                       │    │
│  │  - gRPC server: 0.0.0.0:50053              │    │
│  └────────────────────────────────────────────┘    │
└──────────────────┬──────────────────────────────────┘
                   │
                   │ gRPC (TLS)
                   ▼
┌─────────────────────────────────────────────────────┐
│ Node 4 (datacenter-2)                               │
│  ┌────────────────────────────────────────────┐    │
│  │ TaskAgent (×20)                            │    │
│  │  - gRPC server: 0.0.0.0:50054              │    │
│  └────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ etcd Cluster (Service Discovery)                    │
│  - Node 1: etcd-1 (leader)                          │
│  - Node 2: etcd-2 (follower)                        │
│  - Node 3: etcd-3 (follower)                        │
│  - Stores: agent registry, cluster state            │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ Raft Consensus (Leader Election)                    │
│  - Leader: ChiefArchitectAgent (Node 1)             │
│  - Followers: ComponentLeads, ModuleLeads           │
│  - Log replication for fault tolerance              │
└─────────────────────────────────────────────────────┘
```

---

## Phase 8 Implementation Plan

### Phase 8.1: gRPC Message Protocol (Week 1-2)

**Goal**: Define `.proto` files and implement gRPC client/server

**Tasks**:

1. **Protobuf Message Definition** (8 hours)
   ```protobuf
   // protos/keystone_message.proto
   syntax = "proto3";

   package keystone;

   message KeystoneMessage {
       string msg_id = 1;
       string sender_id = 2;
       string receiver_id = 3;
       string command = 4;
       string payload = 5;  // JSON-encoded
       int64 timestamp_ms = 6;
       int32 priority = 7;
       int64 deadline_ms = 8;
   }

   message SendMessageRequest {
       KeystoneMessage message = 1;
   }

   message SendMessageResponse {
       bool success = 1;
       string error = 2;
   }

   message RegisterAgentRequest {
       string agent_id = 1;
       string node_address = 2;  // "192.168.1.100:50051"
       string agent_type = 3;    // "ChiefArchitect", "ComponentLead", etc.
   }

   message RegisterAgentResponse {
       bool success = 1;
   }

   service MessageBusService {
       rpc SendMessage(SendMessageRequest) returns (SendMessageResponse);
       rpc RegisterAgent(RegisterAgentRequest) returns (RegisterAgentResponse);
       rpc UnregisterAgent(google.protobuf.StringValue) returns (SendMessageResponse);
       rpc ListAgents(google.protobuf.Empty) returns (stream RegisterAgentRequest);
   }
   ```

2. **gRPC Server Implementation** (12 hours)
   ```cpp
   // include/distributed/grpc_message_bus_server.hpp
   class GrpcMessageBusServer final : public keystone::MessageBusService::Service {
   public:
       GrpcMessageBusServer(MessageBus* local_bus);

       grpc::Status SendMessage(grpc::ServerContext* context,
                               const keystone::SendMessageRequest* request,
                               keystone::SendMessageResponse* response) override;

       grpc::Status RegisterAgent(grpc::ServerContext* context,
                                  const keystone::RegisterAgentRequest* request,
                                  keystone::RegisterAgentResponse* response) override;

       void start(const std::string& server_address);
       void shutdown();

   private:
       MessageBus* local_bus_;
       std::unique_ptr<grpc::Server> server_;
   };
   ```

3. **gRPC Client Implementation** (12 hours)
   ```cpp
   // include/distributed/grpc_message_bus_client.hpp
   class GrpcMessageBusClient {
   public:
       GrpcMessageBusClient(const std::string& server_address);

       Task<bool> sendMessage(const KeystoneMessage& msg);
       Task<bool> registerAgent(const std::string& agent_id,
                               const std::string& node_address,
                               const std::string& agent_type);

   private:
       std::unique_ptr<keystone::MessageBusService::Stub> stub_;
       std::string server_address_;
   };
   ```

4. **Distributed MessageBus** (16 hours)
   ```cpp
   // include/distributed/distributed_message_bus.hpp
   class DistributedMessageBus : public MessageBus {
   public:
       DistributedMessageBus(const std::string& local_node_address);

       void registerRemoteNode(const std::string& node_address);
       bool routeMessage(const KeystoneMessage& msg) override;

   private:
       std::string local_node_address_;
       std::unordered_map<std::string, std::unique_ptr<GrpcMessageBusClient>> remote_clients_;

       bool isLocalAgent(const std::string& agent_id);
       std::string findAgentNode(const std::string& agent_id);
   };
   ```

**Deliverables**:
- ✅ Protobuf message definitions
- ✅ gRPC server implementation
- ✅ gRPC client implementation
- ✅ DistributedMessageBus class
- ✅ Unit tests for gRPC messaging

**Estimated Time**: 48 hours

---

### Phase 8.2: Service Discovery (Week 3)

**Goal**: Integrate etcd for dynamic agent registration and discovery

**Tasks**:

1. **etcd Client Integration** (12 hours)
   ```cpp
   // include/distributed/etcd_client.hpp
   class EtcdClient {
   public:
       EtcdClient(const std::string& etcd_endpoints);

       Task<void> put(const std::string& key, const std::string& value, int64_t ttl_seconds = 0);
       Task<std::string> get(const std::string& key);
       Task<std::vector<std::pair<std::string, std::string>>> getPrefix(const std::string& prefix);
       Task<void> del(const std::string& key);

       // Lease management for TTL
       Task<int64_t> createLease(int64_t ttl_seconds);
       Task<void> keepAlive(int64_t lease_id);

   private:
       std::string etcd_endpoints_;
       // etcd C++ client library
   };
   ```

2. **Service Registry** (12 hours)
   ```cpp
   // include/distributed/service_registry.hpp
   class ServiceRegistry {
   public:
       ServiceRegistry(std::unique_ptr<EtcdClient> etcd_client);

       struct AgentInfo {
           std::string agent_id;
           std::string node_address;
           std::string agent_type;
           int64_t last_heartbeat_ms;
       };

       Task<void> registerAgent(const AgentInfo& info);
       Task<void> unregisterAgent(const std::string& agent_id);
       Task<std::optional<AgentInfo>> findAgent(const std::string& agent_id);
       Task<std::vector<AgentInfo>> findAgentsByType(const std::string& agent_type);
       Task<std::vector<AgentInfo>> getAllAgents();

   private:
       std::unique_ptr<EtcdClient> etcd_client_;
       int64_t lease_id_;

       std::string agentKey(const std::string& agent_id);
   };
   ```

3. **Heartbeat Manager** (8 hours)
   ```cpp
   // include/distributed/heartbeat_manager.hpp
   class DistributedHeartbeatManager {
   public:
       DistributedHeartbeatManager(std::unique_ptr<ServiceRegistry> registry);

       void start();
       void stop();

       Task<void> sendHeartbeat(const std::string& agent_id);
       Task<std::vector<std::string>> getAliveAgents();

   private:
       std::unique_ptr<ServiceRegistry> registry_;
       std::atomic<bool> running_{false};
       std::thread heartbeat_thread_;

       void heartbeatLoop();
   };
   ```

4. **Integration with DistributedMessageBus** (8 hours)
   - Use ServiceRegistry to find agent nodes
   - Cache agent→node mapping for performance
   - Invalidate cache on heartbeat timeout

**Deliverables**:
- ✅ etcd client integration
- ✅ ServiceRegistry class
- ✅ DistributedHeartbeatManager
- ✅ Integration with DistributedMessageBus

**Estimated Time**: 40 hours

---

### Phase 8.3: Raft Consensus (Week 4-5)

**Goal**: Implement Raft consensus for leader election and log replication

**Tasks**:

1. **Raft Library Integration** (16 hours)
   - Evaluate: `willemt/raft` or `braft` (Baidu Raft)
   - Integrate with CMake
   - Create C++ wrapper if needed

2. **Raft State Machine** (20 hours)
   ```cpp
   // include/distributed/raft_state_machine.hpp
   class RaftStateMachine {
   public:
       enum class Role { FOLLOWER, CANDIDATE, LEADER };

       RaftStateMachine(const std::string& node_id,
                       const std::vector<std::string>& cluster_nodes);

       void start();
       void stop();

       Role getCurrentRole() const;
       std::string getLeaderId() const;

       // Raft RPCs
       Task<void> requestVote(const std::string& candidate_id, int term);
       Task<void> appendEntries(const std::string& leader_id,
                               const std::vector<LogEntry>& entries);

   private:
       std::string node_id_;
       std::vector<std::string> cluster_nodes_;
       Role current_role_{Role::FOLLOWER};
       std::string current_leader_;
   };
   ```

3. **Log Replication** (16 hours)
   ```cpp
   // include/distributed/replicated_log.hpp
   class ReplicatedLog {
   public:
       struct LogEntry {
           int64_t index;
           int term;
           std::string command;
           std::string data;
       };

       Task<void> append(const LogEntry& entry);
       Task<std::optional<LogEntry>> get(int64_t index);
       Task<void> commit(int64_t index);

   private:
       std::vector<LogEntry> entries_;
       int64_t commit_index_{0};
   };
   ```

4. **Leader Election** (12 hours)
   - Implement Raft election algorithm
   - Timeout-based candidate promotion
   - Vote request and response handling
   - Majority quorum for leader election

5. **Integration with ChiefArchitectAgent** (8 hours)
   ```cpp
   // Enhance AsyncChiefArchitectAgent
   class AsyncChiefArchitectAgent {
   public:
       void setRaftStateMachine(std::unique_ptr<RaftStateMachine> raft);

       bool isLeader() const;
       Task<void> waitForLeadership();

   private:
       std::unique_ptr<RaftStateMachine> raft_;
   };
   ```

**Deliverables**:
- ✅ Raft library integration
- ✅ RaftStateMachine implementation
- ✅ Log replication
- ✅ Leader election
- ✅ Integration with ChiefArchitect

**Estimated Time**: 72 hours

---

### Phase 8.4: Multi-Node Deployment (Week 6)

**Goal**: Deploy HMAS across multiple physical/virtual machines

**Tasks**:

1. **Node Configuration** (8 hours)
   ```yaml
   # config/node1.yaml
   node:
     id: "node-1"
     address: "192.168.1.100:50051"
     datacenter: "dc-1"
     rack: "rack-1"

   etcd:
     endpoints: ["192.168.1.101:2379", "192.168.1.102:2379", "192.168.1.103:2379"]

   raft:
     cluster_nodes:
       - "node-1"
       - "node-2"
       - "node-3"

   agents:
     - type: "ChiefArchitect"
       id: "chief-1"
   ```

2. **Deployment Scripts** (12 hours)
   ```bash
   #!/bin/bash
   # scripts/deploy_node.sh

   NODE_ID=$1
   CONFIG_FILE=$2

   # Start etcd
   etcd --name $NODE_ID --listen-client-urls http://0.0.0.0:2379

   # Start HMAS node
   ./projectkeystone_node --config $CONFIG_FILE
   ```

3. **Docker Compose for Multi-Node** (8 hours)
   ```yaml
   # docker-compose-distributed.yml
   version: '3.8'

   services:
     etcd-1:
       image: quay.io/coreos/etcd:v3.5
       command: etcd --name etcd-1 --advertise-client-urls http://etcd-1:2379

     node-1:
       build: .
       depends_on: [etcd-1]
       environment:
         - NODE_ID=node-1
       volumes:
         - ./config/node1.yaml:/config/node.yaml

     node-2:
       build: .
       depends_on: [etcd-1]
       environment:
         - NODE_ID=node-2
       volumes:
         - ./config/node2.yaml:/config/node.yaml
   ```

4. **Cross-Datacenter Testing** (8 hours)
   - Deploy to 2+ cloud regions (AWS us-east-1, us-west-2)
   - Measure cross-datacenter latency
   - Test network partition scenarios

**Deliverables**:
- ✅ Node configuration files
- ✅ Deployment scripts
- ✅ Docker Compose for multi-node
- ✅ Cross-datacenter deployment guide

**Estimated Time**: 36 hours

---

### Phase 8.5: Real Network Partitions (Week 7)

**Goal**: Handle real network partitions and split-brain scenarios

**Tasks**:

1. **Network Partition Detection** (12 hours)
   ```cpp
   // include/distributed/partition_detector.hpp
   class PartitionDetector {
   public:
       PartitionDetector(std::unique_ptr<ServiceRegistry> registry);

       struct PartitionInfo {
           std::vector<std::string> partition_a;
           std::vector<std::string> partition_b;
           std::chrono::system_clock::time_point detected_at;
       };

       Task<std::optional<PartitionInfo>> detectPartition();

   private:
       std::unique_ptr<ServiceRegistry> registry_;
   };
   ```

2. **Split-Brain Resolution** (16 hours)
   - Use Raft quorum for consistency
   - Minority partition stops accepting writes
   - Majority partition continues operating
   - Automatic healing when partition resolves

3. **Partition Testing with iptables** (8 hours)
   ```bash
   # scripts/create_partition.sh
   # Block traffic from node-1 to node-2
   iptables -A OUTPUT -d 192.168.1.102 -j DROP
   iptables -A INPUT -s 192.168.1.102 -j DROP

   # Heal partition
   iptables -D OUTPUT -d 192.168.1.102 -j DROP
   iptables -D INPUT -s 192.168.1.102 -j DROP
   ```

4. **E2E Test: Split-Brain Scenario** (12 hours)
   - Create 3-node cluster
   - Partition into 2-node + 1-node
   - Verify 2-node partition continues (majority)
   - Verify 1-node partition stops (minority)
   - Heal partition and verify convergence

**Deliverables**:
- ✅ Partition detection
- ✅ Split-brain resolution via Raft
- ✅ iptables testing scripts
- ✅ E2E test for split-brain

**Estimated Time**: 48 hours

---

## Success Criteria

### Must Have ✅
- [ ] gRPC message protocol working
- [ ] Multi-node communication (3+ nodes)
- [ ] Service discovery (etcd integration)
- [ ] Raft consensus for leader election
- [ ] Leader election working (3-node cluster)
- [ ] Real network partition handling
- [ ] E2E test: Multi-node system

### Should Have 🎯
- [ ] Log replication via Raft
- [ ] Heartbeat-based health checking
- [ ] Split-brain resolution via quorum
- [ ] TLS encryption for gRPC
- [ ] Cross-datacenter deployment
- [ ] Performance benchmarks (gRPC latency)

### Nice to Have 💡
- [ ] Dynamic cluster membership (add/remove nodes)
- [ ] Load balancing across nodes
- [ ] Geo-replication (multi-region)
- [ ] Conflict-free replicated data types (CRDTs)
- [ ] Automatic failover and recovery

---

## Test Plan

### E2E Tests (Phase 8)

1. **MultiNodeCommunication** (Priority: CRITICAL)
   - Deploy 3 nodes (Chief, Component, Module)
   - Send message from Chief to Module (crosses nodes)
   - Verify message delivered via gRPC

2. **ServiceDiscovery** (Priority: CRITICAL)
   - Register 10 agents across 3 nodes
   - Query ServiceRegistry for agent locations
   - Verify all agents found

3. **LeaderElection** (Priority: HIGH)
   - Start 3-node Raft cluster
   - Verify leader elected
   - Kill leader, verify new leader elected
   - Restore old leader, verify it becomes follower

4. **NetworkPartition** (Priority: HIGH)
   - 3-node cluster
   - Create partition: 2 nodes vs 1 node
   - Verify 2-node partition continues (majority)
   - Verify 1-node partition stops (minority)
   - Heal partition, verify convergence

5. **CrossDatacenterLatency** (Priority: MEDIUM)
   - Deploy nodes in 2 cloud regions
   - Measure gRPC message latency
   - Verify latency < 500ms (cross-region)

---

## Performance Expectations

**gRPC Latency**:
- Same datacenter: < 10 ms (p99)
- Cross-datacenter: < 200 ms (p99)
- Cross-region: < 500 ms (p99)

**Service Discovery**:
- Agent lookup: < 50 ms (etcd query)
- Heartbeat interval: 1 second
- Timeout threshold: 5 seconds

**Raft Performance**:
- Leader election time: < 1 second
- Log replication latency: < 50 ms
- Quorum agreement: < 100 ms

---

## Risk Mitigation

### Risk 1: Network Unreliability
**Impact**: High
**Likelihood**: High
**Mitigation**: Retry logic, exponential backoff, Raft consensus for consistency.

### Risk 2: Split-Brain Scenarios
**Impact**: Critical
**Likelihood**: Medium
**Mitigation**: Raft quorum ensures only majority partition accepts writes.

### Risk 3: Service Discovery Failures (etcd down)
**Impact**: High
**Likelihood**: Low
**Mitigation**: etcd cluster (3 nodes), fallback to cached agent registry.

### Risk 4: gRPC Complexity
**Impact**: Medium
**Likelihood**: Medium
**Mitigation**: Start with simple RPC calls, use existing protobuf definitions.

---

## Dependencies

### New Dependencies

1. **gRPC** - RPC framework
   - Version: 1.50+
   - Purpose: Inter-node communication

2. **protobuf** - Serialization
   - Version: 3.20+
   - Purpose: Message serialization

3. **etcd C++ client** - Service discovery
   - Repo: https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3
   - Purpose: Agent registry

4. **Raft library** - Consensus
   - Option 1: willemt/raft
   - Option 2: braft (Baidu Raft)
   - Purpose: Leader election, log replication

5. **iptables** (for testing) - Network partition simulation

---

## Next Steps

1. **Week 1-2**: gRPC message protocol
2. **Week 3**: Service discovery (etcd)
3. **Week 4-5**: Raft consensus
4. **Week 6**: Multi-node deployment
5. **Week 7**: Real network partitions

**After Phase 8**: Move to **Phase 10: Advanced Features** or integrate with **Phase 7: AI Integration**

---

**Status**: 📝 Planning - Ready for Implementation
**Total Estimated Time**: 244 hours (~7 weeks)
**Last Updated**: 2025-11-19

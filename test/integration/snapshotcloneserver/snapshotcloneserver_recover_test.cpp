/*
 *  Copyright (c) 2020 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * Project: curve
 * Created Date: Thu Nov 14 2019
 * Author: xuchaojie
 */

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <json/json.h>

#include "src/common/uuid.h"
#include "src/common/location_operator.h"
#include "test/integration/cluster_common/cluster.h"
#include "src/client/libcurve_file.h"
#include "src/snapshotcloneserver/snapshot/snapshot_service_manager.h"
#include "src/snapshotcloneserver/clone/clone_service_manager.h"
#include "test/integration/snapshotcloneserver/test_snapshotcloneserver_helpler.h"
#include "src/common/snapshotclone/snapshotclone_define.h"
#include "src/snapshotcloneserver/common/snapshotclone_meta_store.h"

using curve::CurveCluster;
using curve::client::FileClient;
using curve::client::SnapshotClient;
using curve::client::UserInfo_t;
using ::curve::common::LocationOperator;
using ::curve::common::UUIDGenerator;

const std::string kTestPrefix = "RcvSCSTest";  // NOLINT

const uint64_t testFile1Length = 10ULL * 1024 * 1024 * 1024;
const uint64_t chunkSize = 16ULL * 1024 * 1024;
const uint64_t segmentSize = 32ULL * 1024 * 1024;
const uint64_t chunkSplitSize = 65536;

// 测试文件只写2个segment
const uint64_t testFile1AllocSegmentNum = 2;

// 一些常数定义
const char *cloneTempDir_ = "/clone";
const char *mdsRootUser_ = "root";
const char *mdsRootPassword_ = "root_password";

constexpr uint32_t kProgressTransferSnapshotDataStart = 10;

const char *kEtcdClientIpPort = "127.0.0.1:10021";
const char *kEtcdPeerIpPort = "127.0.0.1:10022";
const char *kMdsIpPort = "127.0.0.1:10023";
const char *kChunkServerIpPort1 = "127.0.0.1:10024";
const char *kChunkServerIpPort2 = "127.0.0.1:10025";
const char *kChunkServerIpPort3 = "127.0.0.1:10026";
const char *kSnapshotCloneServerIpPort = "127.0.0.1:10027";
const char *kSnapshotCloneServerDummyServerPort = "12002";
const char *kLeaderCampaginPrefix = "snapshotcloneserverleaderlock1";

static const char *kDefaultPoolset = "default";

const int kMdsDummyPort = 10028;

const std::string kLogPath = "./runlog/" + kTestPrefix + "Log";  // NOLINT
const std::string kMdsDbName = kTestPrefix + "DB";               // NOLINT
const std::string kMdsConfigPath =                               // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_mds.conf";

const std::string kCSConfigPath =                                // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_chunkserver.conf";

const std::string kCsClientConfigPath =                          // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_cs_client.conf";

const std::string kSnapClientConfigPath =                        // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_snap_client.conf";

const std::string kS3ConfigPath =                                // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_s3.conf";

const std::string kSCSConfigPath =                               // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_scs.conf";

const std::string kClientConfigPath =                            // NOLINT
    "./test/integration/snapshotcloneserver/config/" + kTestPrefix +
    "_client.conf";

const std::vector<std::string> mdsConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
    std::string("mds.etcd.endpoint=") + kEtcdClientIpPort,
    std::string("mds.DbName=") + kMdsDbName,
    std::string("mds.auth.rootPassword=") + mdsRootPassword_,
    "mds.enable.copyset.scheduler=false",
    "mds.enable.leader.scheduler=false",
    "mds.enable.recover.scheduler=false",
    "mds.enable.replica.scheduler=false",
    "mds.heartbeat.misstimeoutMs=10000",
    "mds.topology.TopologyUpdateToRepoSec=5",
    std::string("mds.file.expiredTimeUs=50000"),
    std::string("mds.file.expiredTimeUs=10000"),
    std::string("mds.snapshotcloneclient.addr=") + kSnapshotCloneServerIpPort,
};

const std::vector<std::string> mdsConf1{
    { " --graceful_quit_on_sigterm" },
    std::string(" --confPath=") + kMdsConfigPath,
    std::string(" --log_dir=") + kLogPath,
    std::string(" --segmentSize=") + std::to_string(segmentSize),
    { " --stderrthreshold=3" },
};

const std::vector<std::string> chunkserverConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
    std::string("curve.config_path=") + kCsClientConfigPath,
    std::string("s3.config_path=") + kS3ConfigPath,
    std::string("curve.root_username") + mdsRootUser_,
    std::string("curve.root_password") + mdsRootPassword_,
    "walfilepool.use_chunk_file_pool=false",
    "walfilepool.enable_get_segment_from_pool=false",
    "global.block_size=4096",
    "global.meta_page_size=4096",
};

const std::vector<std::string> csClientConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
};

const std::vector<std::string> snapClientConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
};

const std::vector<std::string> s3ConfigOptions{};

const std::vector<std::string> chunkserverConf1{
    { " --graceful_quit_on_sigterm" },
    { " -chunkServerStoreUri=local://./" + kTestPrefix + "1/" },
    { " -chunkServerMetaUri=local://./" + kTestPrefix +
      "1/chunkserver.dat" },  // NOLINT
    { " -copySetUri=local://./" + kTestPrefix + "1/copysets" },
    { " -raftSnapshotUri=curve://./" + kTestPrefix + "1/copysets" },
    { " -recycleUri=local://./" + kTestPrefix + "1/recycler" },
    { " -chunkFilePoolDir=./" + kTestPrefix + "1/chunkfilepool/" },
    { " -chunkFilePoolMetaPath=./" + kTestPrefix +
      "1/chunkfilepool.meta" },  // NOLINT
    std::string(" -conf=") + kCSConfigPath,
    { " -raft_sync_segments=true" },
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
    { " -raftLogUri=curve://./" + kTestPrefix + "1/copysets" },
    { " -walFilePoolDir=./" + kTestPrefix + "1/walfilepool/" },
    { " -walFilePoolMetaPath=./" + kTestPrefix +
        "1/walfilepool.meta" },
};

const std::vector<std::string> chunkserverConf2{
    { " --graceful_quit_on_sigterm" },
    { " -chunkServerStoreUri=local://./" + kTestPrefix + "2/" },
    { " -chunkServerMetaUri=local://./" + kTestPrefix +
      "2/chunkserver.dat" },  // NOLINT
    { " -copySetUri=local://./" + kTestPrefix + "2/copysets" },
    { " -raftSnapshotUri=curve://./" + kTestPrefix + "2/copysets" },
    { " -recycleUri=local://./" + kTestPrefix + "2/recycler" },
    { " -chunkFilePoolDir=./" + kTestPrefix + "2/chunkfilepool/" },
    { " -chunkFilePoolMetaPath=./" + kTestPrefix +
      "2/chunkfilepool.meta" },  // NOLINT
    std::string(" -conf=") + kCSConfigPath,
    { " -raft_sync_segments=true" },
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
    { " -raftLogUri=curve://./" + kTestPrefix + "2/copysets" },
    { " -walFilePoolDir=./" + kTestPrefix + "2/walfilepool/" },
    { " -walFilePoolMetaPath=./" + kTestPrefix +
        "2/walfilepool.meta" },
};

const std::vector<std::string> chunkserverConf3{
    { " --graceful_quit_on_sigterm" },
    { " -chunkServerStoreUri=local://./" + kTestPrefix + "3/" },
    { " -chunkServerMetaUri=local://./" + kTestPrefix +
      "3/chunkserver.dat" },  // NOLINT
    { " -copySetUri=local://./" + kTestPrefix + "3/copysets" },
    { " -raftSnapshotUri=curve://./" + kTestPrefix + "3/copysets" },
    { " -recycleUri=local://./" + kTestPrefix + "3/recycler" },
    { " -chunkFilePoolDir=./" + kTestPrefix + "3/chunkfilepool/" },
    { " -chunkFilePoolMetaPath=./" + kTestPrefix +
      "3/chunkfilepool.meta" },  // NOLINT
    std::string(" -conf=") + kCSConfigPath,
    { " -raft_sync_segments=true" },
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
    { " -raftLogUri=curve://./" + kTestPrefix + "3/copysets" },
    { " -walFilePoolDir=./" + kTestPrefix + "3/walfilepool/" },
    { " -walFilePoolMetaPath=./" + kTestPrefix +
        "3/walfilepool.meta" },
};

const std::vector<std::string> snapshotcloneserverConfigOptions{
    std::string("client.config_path=") + kSnapClientConfigPath,
    std::string("s3.config_path=") + kS3ConfigPath,
    std::string("metastore.db_name=") + kMdsDbName,
    std::string("server.snapshotPoolThreadNum=8"),
    std::string("server.snapshotCoreThreadNum=2"),
    std::string("server.clonePoolThreadNum=8"),
    std::string("server.createCloneChunkConcurrency=2"),
    std::string("server.recoverChunkConcurrency=2"),
    std::string("mds.rootUser=") + mdsRootUser_,
    std::string("mds.rootPassword=") + mdsRootPassword_,
    std::string("client.methodRetryTimeSec=1"),
    std::string("server.clientAsyncMethodRetryTimeSec=1"),
    std::string("etcd.endpoint=") + kEtcdClientIpPort,
    std::string("server.dummy.listen.port=") +
        kSnapshotCloneServerDummyServerPort,
    std::string("leader.campagin.prefix=") + kLeaderCampaginPrefix,
    std::string("server.backEndReferenceRecordScanIntervalMs=100"),
    std::string("server.backEndReferenceFuncScanIntervalMs=1000"),
};

const std::vector<std::string> snapshotcloneConf{
    std::string(" --conf=") + kSCSConfigPath,
    std::string(" --log_dir=") + kLogPath,
    { " --stderrthreshold=3" },
};

const std::vector<std::string> clientConfigOptions{
    std::string("mds.listen.addr=") + kMdsIpPort,
    std::string("global.logPath=") + kLogPath,
    std::string("mds.rpcTimeoutMS=4000"),
};

const char *testFile1_ = "/RcvItUser1/file1";
const char *testUser1_ = "RcvItUser1";
int testFd1_ = 0;

namespace curve {
namespace snapshotcloneserver {

class SnapshotCloneServerTest : public ::testing::Test {
 public:
    static void SetUpTestCase() {
        std::string mkLogDirCmd = std::string("mkdir -p ") + kLogPath;
        system(mkLogDirCmd.c_str());

        cluster_ = new CurveCluster();
        ASSERT_NE(nullptr, cluster_);

        // 初始化db
        system(std::string("rm -rf " + kTestPrefix + ".etcd").c_str());
        system(std::string("rm -rf " + kTestPrefix + "1").c_str());
        system(std::string("rm -rf " + kTestPrefix + "2").c_str());
        system(std::string("rm -rf " + kTestPrefix + "3").c_str());

        // 启动etcd
        pid_t pid = cluster_->StartSingleEtcd(
            1, kEtcdClientIpPort, kEtcdPeerIpPort,
            std::vector<std::string>{ " --name " + kTestPrefix });
        LOG(INFO) << "etcd 1 started on " << kEtcdClientIpPort
                  << "::" << kEtcdPeerIpPort << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        cluster_->InitSnapshotCloneMetaStoreEtcd(kEtcdClientIpPort);

        cluster_->PrepareConfig<MDSConfigGenerator>(kMdsConfigPath,
                                                    mdsConfigOptions);

        // 启动一个mds
        pid = cluster_->StartSingleMDS(1, kMdsIpPort, kMdsDummyPort, mdsConf1,
                                       true);
        LOG(INFO) << "mds 1 started on " << kMdsIpPort << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        // 创建物理池
        ASSERT_EQ(0, cluster_->PreparePhysicalPool(
                         1,
                         "./test/integration/snapshotcloneserver/"
                         "config/topo3.json"));  // NOLINT

        // format chunkfilepool and walfilepool
        std::vector<std::thread> threadpool(3);

        threadpool[0] =
            std::thread(&CurveCluster::FormatFilePool, cluster_,
                        "./" + kTestPrefix + "1/chunkfilepool/",
                        "./" + kTestPrefix + "1/chunkfilepool.meta",
                        "./" + kTestPrefix + "1/chunkfilepool/", 2);
        threadpool[1] =
            std::thread(&CurveCluster::FormatFilePool, cluster_,
                        "./" + kTestPrefix + "2/chunkfilepool/",
                        "./" + kTestPrefix + "2/chunkfilepool.meta",
                        "./" + kTestPrefix + "2/chunkfilepool/", 2);
        threadpool[2] =
            std::thread(&CurveCluster::FormatFilePool, cluster_,
                        "./" + kTestPrefix + "3/chunkfilepool/",
                        "./" + kTestPrefix + "3/chunkfilepool.meta",
                        "./" + kTestPrefix + "3/chunkfilepool/", 2);
        for (int i = 0; i < 3; i++) {
            threadpool[i].join();
        }

        cluster_->PrepareConfig<CSClientConfigGenerator>(kCsClientConfigPath,
                                                         csClientConfigOptions);

        cluster_->PrepareConfig<S3ConfigGenerator>(kS3ConfigPath,
                                                   s3ConfigOptions);

        cluster_->PrepareConfig<CSConfigGenerator>(kCSConfigPath,
                                                   chunkserverConfigOptions);

        // 创建chunkserver
        pid = cluster_->StartSingleChunkServer(1, kChunkServerIpPort1,
                                               chunkserverConf1);
        LOG(INFO) << "chunkserver 1 started on " << kChunkServerIpPort1
                  << ", pid = " << pid;
        ASSERT_GT(pid, 0);
        pid = cluster_->StartSingleChunkServer(2, kChunkServerIpPort2,
                                               chunkserverConf2);
        LOG(INFO) << "chunkserver 2 started on " << kChunkServerIpPort2
                  << ", pid = " << pid;
        ASSERT_GT(pid, 0);
        pid = cluster_->StartSingleChunkServer(3, kChunkServerIpPort3,
                                               chunkserverConf3);
        LOG(INFO) << "chunkserver 3 started on " << kChunkServerIpPort3
                  << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 创建逻辑池, 并睡眠一段时间让底层copyset先选主
        ASSERT_EQ(0, cluster_->PrepareLogicalPool(
                         1,
                         "./test/integration/snapshotcloneserver/"
                         "config/topo3.json"));

        cluster_->PrepareConfig<SnapClientConfigGenerator>(
            kSnapClientConfigPath, snapClientConfigOptions);

        cluster_->PrepareConfig<SCSConfigGenerator>(
            kSCSConfigPath, snapshotcloneserverConfigOptions);

        pid = cluster_->StartSnapshotCloneServer(1, kSnapshotCloneServerIpPort,
                                                 snapshotcloneConf);
        LOG(INFO) << "SnapshotCloneServer 1 started on "
                  << kSnapshotCloneServerIpPort << ", pid = " << pid;
        ASSERT_GT(pid, 0);

        cluster_->PrepareConfig<ClientConfigGenerator>(kClientConfigPath,
                                                       clientConfigOptions);

        fileClient_ = new FileClient();
        fileClient_->Init(kClientConfigPath);

        snapClient_ = new SnapshotClient();
        snapClient_->Init(kSnapClientConfigPath);

        UserInfo_t userinfo;
        userinfo.owner = "RcvItUser1";

        ASSERT_EQ(0, fileClient_->Mkdir("/RcvItUser1", userinfo));

        std::string fakeData(4096, 'x');
        ASSERT_TRUE(
            CreateAndWriteFile(testFile1_, testUser1_, fakeData, &testFd1_));
        LOG(INFO) << "Write testFile1_ success.";
    }

    static bool CreateAndWriteFile(const std::string &fileName,
                                   const std::string &user,
                                   const std::string &dataSample, int *fdOut) {
        UserInfo_t userinfo;
        userinfo.owner = user;
        int ret = fileClient_->Create(fileName, userinfo, testFile1Length);
        if (ret < 0) {
            LOG(ERROR) << "Create fail, ret = " << ret;
            return false;
        }
        return WriteFile(fileName, user, dataSample, fdOut);
    }

    static bool WriteFile(const std::string &fileName, const std::string &user,
                          const std::string &dataSample, int *fdOut) {
        int ret = 0;
        UserInfo_t userinfo;
        userinfo.owner = user;
        *fdOut = fileClient_->Open(fileName, userinfo);
        if (*fdOut < 0) {
            LOG(ERROR) << "Open fail, ret = " << *fdOut;
            return false;
        }
        // 2个segment，每个写第一个chunk
        for (uint64_t i = 0; i < testFile1AllocSegmentNum; i++) {
            ret = fileClient_->Write(*fdOut, dataSample.c_str(),
                                     i * segmentSize, dataSample.size());
            if (ret < 0) {
                LOG(ERROR) << "Write Fail, ret = " << ret;
                return false;
            }
        }
        ret = fileClient_->Close(*fdOut);
        if (ret < 0) {
            LOG(ERROR) << "Close fail, ret = " << ret;
            return false;
        }
        return true;
    }

    static bool CheckFileData(const std::string &fileName,
                              const std::string &user,
                              const std::string &dataSample) {
        UserInfo_t userinfo;
        userinfo.owner = user;

        int ret = 0;
        // 检查文件状态
        FInfo fileInfo;
        ret = snapClient_->GetFileInfo(fileName, userinfo, &fileInfo);
        if (ret < 0) {
            LOG(ERROR) << "GetFileInfo fail, ret = " << ret;
            return false;
        }
        if (fileInfo.filestatus != FileStatus::Cloned &&
            fileInfo.filestatus != FileStatus::Created) {
            LOG(ERROR) << "check file status fail, status = "
                       << static_cast<int>(fileInfo.filestatus);
            return false;
        }

        int dstFd = fileClient_->Open(fileName, userinfo);
        if (dstFd < 0) {
            LOG(ERROR) << "Open fail, ret = " << dstFd;
            return false;
        }

        for (uint64_t i = 0; i < testFile1AllocSegmentNum; i++) {
            char buf[4096];
            ret = fileClient_->Read(dstFd, buf, i * segmentSize, 4096);
            if (ret < 0) {
                LOG(ERROR) << "Read fail, ret = " << ret;
                return false;
            }
            std::string data(buf, 4096);
            if (data != dataSample) {
                LOG(ERROR) << "CheckFileData not Equal, data = [" << data
                           << "] , expect data = [" << dataSample << "].";
                return false;
            }
        }
        ret = fileClient_->Close(dstFd);
        if (ret < 0) {
            LOG(ERROR) << "Close fail, ret = " << ret;
            return false;
        }
        return true;
    }

    static void TearDownTestCase() {
        fileClient_->UnInit();
        delete fileClient_;
        fileClient_ = nullptr;
        snapClient_->UnInit();
        delete snapClient_;
        snapClient_ = nullptr;
        ASSERT_EQ(0, cluster_->StopCluster());
        delete cluster_;
        cluster_ = nullptr;
        system(std::string("rm -rf " + kTestPrefix + ".etcd").c_str());
        system(std::string("rm -rf " + kTestPrefix + "1").c_str());
        system(std::string("rm -rf " + kTestPrefix + "2").c_str());
        system(std::string("rm -rf " + kTestPrefix + "3").c_str());
    }

    void SetUp() {}

    void TearDown() {}

    void PrepareSnapshotForTestFile1(std::string *uuid1) {
        if (!hasSnapshotForTestFile1_) {
            int ret = MakeSnapshot(testUser1_, testFile1_, "snap1", uuid1);
            ASSERT_EQ(0, ret);
            bool success1 =
                CheckSnapshotSuccess(testUser1_, testFile1_, *uuid1);
            ASSERT_TRUE(success1);
            hasSnapshotForTestFile1_ = true;
            snapIdForTestFile1_ = *uuid1;
        }
    }

    void WaitDeleteSnapshotForTestFile1() {
        if (hasSnapshotForTestFile1_) {
            ASSERT_EQ(0, DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_,
                                                       snapIdForTestFile1_));
        }
    }

    int PrepareCreateCloneFile(const std::string &fileName, FInfo *fInfoOut,
                               bool IsRecover = false) {
        uint64_t seqNum = 1;
        if (IsRecover) {
            seqNum = 2;  // 恢复新文件使用版本号+1
        } else {
            seqNum = 1;  // 克隆新文件使用初始版本号1
        }
        int ret = snapClient_->CreateCloneFile(
                testFile1_, fileName,
                UserInfo_t(mdsRootUser_, mdsRootPassword_), testFile1Length,
                seqNum, chunkSize, 0, 0, kDefaultPoolset, fInfoOut);
        return ret;
    }

    int PrepareCreateCloneMeta(FInfo *fInfoOut, const std::string &newFileName,
                               std::vector<SegmentInfo> *segInfoOutVec) {
        fInfoOut->fullPathName = newFileName;
        fInfoOut->userinfo = UserInfo_t(mdsRootUser_, mdsRootPassword_);
        for (int i = 0; i < testFile1AllocSegmentNum; i++) {
            SegmentInfo segInfoOut;
            int ret = snapClient_->GetOrAllocateSegmentInfo(
                true, i * segmentSize, fInfoOut, &segInfoOut);
            segInfoOutVec->emplace_back(segInfoOut);
            if (ret != LIBCURVE_ERROR::OK) {
                return ret;
            }
        }
        return LIBCURVE_ERROR::OK;
    }

    int PrepareCreateCloneChunk(const std::vector<SegmentInfo> &segInfoVec,
                                bool IsRecover = false) {
        if (segInfoVec.size() != testFile1AllocSegmentNum) {
            LOG(ERROR) << "internal error!";
            return -1;
        }
        auto tracker = std::make_shared<TaskTracker>();
        if (IsRecover) {
            for (int i = 0; i < testFile1AllocSegmentNum; i++) {
                ChunkDataName name;
                name.fileName_ = testFile1_;
                name.chunkSeqNum_ = 1;
                name.chunkIndex_ = i * segmentSize / chunkSize;
                std::string location =
                    LocationOperator::GenerateS3Location(name.ToDataChunkKey());
                // 由于测试文件每个segment只写了第一个chunk，
                // 快照可以做到只转储当前写过的chunk，
                // 所以从快照克隆每个segment只Create第一个chunk。
                // 而从文件克隆，由于mds不知道chunk写没写过，
                // 所以需要Create全部的chunk。
                ChunkIDInfo cidInfo = segInfoVec[i].chunkvec[0];
                SnapCloneCommonClosure *cb =
                    new SnapCloneCommonClosure(tracker);
                tracker->AddOneTrace();
                LOG(INFO) << "CreateCloneChunk, location = " << location
                          << ", logicalPoolId = " << cidInfo.lpid_
                          << ", copysetId = " << cidInfo.cpid_
                          << ", chunkId = " << cidInfo.cid_
                          << ", seqNum = " << 1 << ", csn = " << 2;
                int ret = snapClient_->CreateCloneChunk(
                    location, cidInfo,
                    1,  // 恢复使用快照中chunk的版本号
                    2,  // 恢复使用新文件的版本号, 即原文件版本号+1
                    chunkSize, cb);
                if (ret != LIBCURVE_ERROR::OK) {
                    return ret;
                }
            }
        } else {
            for (int i = 0; i < testFile1AllocSegmentNum; i++) {
                for (uint64_t j = 0; j < segmentSize / chunkSize; j++) {
                    std::string location =
                        LocationOperator::GenerateCurveLocation(
                            testFile1_, i * segmentSize + j * chunkSize);
                    ChunkIDInfo cidInfo = segInfoVec[i].chunkvec[j];
                    SnapCloneCommonClosure *cb =
                        new SnapCloneCommonClosure(tracker);
                    tracker->AddOneTrace();
                    LOG(INFO) << "CreateCloneChunk, location = " << location
                              << ", logicalPoolId = " << cidInfo.lpid_
                              << ", copysetId = " << cidInfo.cpid_
                              << ", chunkId = " << cidInfo.cid_
                              << ", seqNum = " << 1 << ", csn = " << 0;
                    int ret =
                        snapClient_->CreateCloneChunk(location, cidInfo,
                                                      1,  // 克隆使用初始版本号1
                                                      0,  // 克隆使用0
                                                      chunkSize, cb);
                    if (ret != LIBCURVE_ERROR::OK) {
                        return ret;
                    }
                }
            }
        }
        tracker->Wait();
        int ret = tracker->GetResult();
        if (ret != LIBCURVE_ERROR::OK) {
            LOG(ERROR) << "CreateCloneChunk tracker GetResult fail"
                       << ", ret = " << ret;
            return ret;
        }
        return LIBCURVE_ERROR::OK;
    }

    int PrepareCompleteCloneMeta(const std::string &uuid) {
        std::string fileName = std::string(cloneTempDir_) + "/" + uuid;
        int ret = snapClient_->CompleteCloneMeta(
            fileName, UserInfo_t(mdsRootUser_, mdsRootPassword_));
        return ret;
    }

    int PrepareRecoverChunk(const std::vector<SegmentInfo> &segInfoVec,
                            bool IsSnapshot = false) {
        if (segInfoVec.size() != testFile1AllocSegmentNum) {
            LOG(ERROR) << "internal error!";
            return -1;
        }
        auto tracker = std::make_shared<TaskTracker>();
        if (IsSnapshot) {
            for (int i = 0; i < testFile1AllocSegmentNum; i++) {
                // 由于测试文件每个segment只写了第一个chunk，
                // 快照可以做到只转储当前写过的chunk，
                // 所以从快照克隆每个segment只Recover第一个chunk。
                // 而从文件克隆，由于mds不知道chunk写没写过，
                // 所以需要Recover全部的chunk。
                ChunkIDInfo cidInfo = segInfoVec[i].chunkvec[0];
                for (uint64_t k = 0; k < chunkSize / chunkSplitSize; k++) {
                    SnapCloneCommonClosure *cb =
                        new SnapCloneCommonClosure(tracker);
                    tracker->AddOneTrace();
                    uint64_t offset = k * chunkSplitSize;
                    LOG(INFO) << "RecoverChunk"
                              << ", logicalPoolId = " << cidInfo.lpid_
                              << ", copysetId = " << cidInfo.cpid_
                              << ", chunkId = " << cidInfo.cid_
                              << ", offset = " << offset;
                    int ret = snapClient_->RecoverChunk(cidInfo, offset,
                                                        chunkSplitSize, cb);
                    if (ret != LIBCURVE_ERROR::OK) {
                        return ret;
                    }
                }
            }
        } else {
            for (int i = 0; i < testFile1AllocSegmentNum; i++) {
                for (uint64_t j = 0; j < segmentSize / chunkSize; j++) {
                    ChunkIDInfo cidInfo = segInfoVec[i].chunkvec[j];
                    for (uint64_t k = 0; k < chunkSize / chunkSplitSize; k++) {
                        SnapCloneCommonClosure *cb =
                            new SnapCloneCommonClosure(tracker);
                        tracker->AddOneTrace();
                        uint64_t offset = k * chunkSplitSize;
                        LOG(INFO) << "RecoverChunk"
                                  << ", logicalPoolId = " << cidInfo.lpid_
                                  << ", copysetId = " << cidInfo.cpid_
                                  << ", chunkId = " << cidInfo.cid_
                                  << ", offset = " << offset;
                        int ret = snapClient_->RecoverChunk(cidInfo, offset,
                                                            chunkSplitSize, cb);
                        if (ret != LIBCURVE_ERROR::OK) {
                            return ret;
                        }
                    }
                }
            }
        }
        tracker->Wait();
        int ret = tracker->GetResult();
        if (ret != LIBCURVE_ERROR::OK) {
            LOG(ERROR) << "RecoverChunk tracker GetResult fail"
                       << ", ret = " << ret;
            return ret;
        }
        return LIBCURVE_ERROR::OK;
    }

    int PrepareCompleteCloneFile(const std::string &fileName) {
        return snapClient_->CompleteCloneFile(
            fileName, UserInfo_t(mdsRootUser_, mdsRootPassword_));
    }

    int PrepareChangeOwner(const std::string &fileName) {
        return fileClient_->ChangeOwner(
            fileName, testUser1_, UserInfo_t(mdsRootUser_, mdsRootPassword_));
    }

    int PrepareRenameCloneFile(uint64_t originId, uint64_t destinationId,
                               const std::string &fileName,
                               const std::string &newFileName) {
        return snapClient_->RenameCloneFile(
            UserInfo_t(mdsRootUser_, mdsRootPassword_), originId, destinationId,
            fileName, newFileName);
    }

    static CurveCluster *cluster_;
    static FileClient *fileClient_;
    static SnapshotClient *snapClient_;

    bool hasSnapshotForTestFile1_ = false;
    std::string snapIdForTestFile1_;
};

CurveCluster *SnapshotCloneServerTest::cluster_ = nullptr;
FileClient *SnapshotCloneServerTest::fileClient_ = nullptr;
SnapshotClient *SnapshotCloneServerTest::snapClient_ = nullptr;

// 未在curve中创建快照阶段，重启恢复
TEST_F(SnapshotCloneServerTest, TestRecoverSnapshotWhenNotCreateSnapOnCurvefs) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    SnapshotInfo snapInfo(uuid1, testUser1_, testFile1_,
                        "snapxxx", 0, chunkSize,
                        segmentSize, testFile1Length,
                        0, 0, kDefaultPoolset, 0,
                        Status::pending);
    cluster_->metaStore_->AddSnapshot(snapInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_TRUE(success1);

    int ret = DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// 已在curve中创建快照，但成功结果未返回，重启恢复
TEST_F(SnapshotCloneServerTest,
       TestRecoverSnapshotWhenHasCreateSnapOnCurvefsNotReturn) {
    // 调用client接口创建快照
    uint64_t seq = 0;
    snapClient_->CreateSnapShot(testFile1_, UserInfo_t(testUser1_, ""), &seq);

    std::string uuid1 = UUIDGenerator().GenerateUUID();
    SnapshotInfo snapInfo(uuid1, testUser1_, testFile1_,
                        "snapxxx", 0, chunkSize,
                        segmentSize, testFile1Length,
                        0, 0, kDefaultPoolset, 0,
                        Status::pending);
    cluster_->metaStore_->AddSnapshot(snapInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_TRUE(success1);

    int ret = DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// 已在curve中创建快照，结果已返回，重启恢复
TEST_F(SnapshotCloneServerTest,
       TestRecoverSnapshotWhenHasCreateSnapOnCurvefsReturn) {
    // 调用client接口创建快照
    uint64_t seq = 0;
    snapClient_->CreateSnapShot(testFile1_, UserInfo_t(testUser1_, ""), &seq);

    std::string uuid1 = UUIDGenerator().GenerateUUID();
    SnapshotInfo snapInfo(uuid1, testUser1_, testFile1_, "snapxxx", seq,
                        chunkSize, segmentSize, testFile1Length,
                        0, 0, kDefaultPoolset, 0,
                        Status::pending);
    cluster_->metaStore_->AddSnapshot(snapInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_TRUE(success1);

    int ret = DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// 已在curve中创建快照阶段，nos上传部分快照，重启恢复
TEST_F(SnapshotCloneServerTest, TestRecoverSnapshotWhenHasTransferSomeData) {
    std::string uuid1;
    int ret = MakeSnapshot(testUser1_, testFile1_, "snap1", &uuid1);
    ASSERT_EQ(0, ret);

    bool success = false;
    for (int i = 0; i < 600; i++) {
        FileSnapshotInfo info1;
        int retCode = GetSnapshotInfo(testUser1_, testFile1_, uuid1, &info1);
        if (retCode != 0) {
            break;
        }
        if (info1.GetSnapshotInfo().GetStatus() == Status::pending) {
            if (info1.GetSnapProgress() > kProgressTransferSnapshotDataStart) {
                //  当进度到达转储的百分比时重启
                pid_t pid = cluster_->RestartSnapshotCloneServer(1, true);
                LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
                ASSERT_GT(pid, 0);
                success = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
            break;
        }
    }
    ASSERT_TRUE(success);

    bool success1 = CheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_TRUE(success1);

    ret = DeleteAndCheckSnapshotSuccess(testUser1_, testFile1_, uuid1);
    ASSERT_EQ(0, ret);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CreateCloneFile阶段重启，mds上未创建文件
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotCreateCloneFile) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotCreateCloneFile";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, 0, 0, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCreateCloneFile,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CreateCloneFile阶段重启，mds上创建文件成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneHasCreateCloneFileSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneHasCreateCloneFileSuccessNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, 0, 0, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCreateCloneFile,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CreateCloneMeta阶段重启， 在mds上未创建segment
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotCreateCloneMeta) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotCreateCloneMeta";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCreateCloneMeta,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CreateCloneMeta阶段重启， 在mds上创建segment成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneCreateCloneMetaSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneCreateCloneMetaSuccessNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCreateCloneMeta,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CreateCloneChunk阶段重启，未在chunkserver上创建clonechunk
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotCreateCloneChunk) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotCreateCloneChunk";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCreateCloneChunk,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CreateCloneChunk阶段重启，在chunkserver上创建部分clonechunk
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneCreateCloneChunkSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneCreateCloneChunkSuccessNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCreateCloneChunk,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CompleteCloneMeta阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotCompleteCloneMeta) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotCompleteCloneMeta";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCompleteCloneMeta,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CompleteCloneMeta阶段重启，同时在mds上调用CompleteCloneMeta成功但未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneCompleteCloneMetaSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneCompleteCloneMetaSuccessNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCompleteCloneMeta,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// RecoverChunk阶段重启，在chunkserver上未调用RecoverChunk
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotRecoverChunk) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotRecoverChunk";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kRecoverChunk,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// RecoverChunk阶段重启，在chunkserver上部分调用RecoverChunk
TEST_F(SnapshotCloneServerTest, TestRecoverCloneRecoverChunkSuccssNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneRecoverChunkSuccssNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kRecoverChunk,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CompleteCloneFile阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotCompleteCloneFile) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotCompleteCloneFile";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCompleteCloneFile,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// CompleteCloneFile阶段重启，但mds上CompleteCloneFile已成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneCompleteCloneFileSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneFile(fileName));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneCompleteCloneFileSuccessNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kCompleteCloneFile,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// ChangeOwner阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotChangeOwner) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneFile(fileName));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotChangeOwner";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kChangeOwner,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// ChangeOwner阶段重启，但mds上ChangeOwner成功未返回
TEST_F(SnapshotCloneServerTest, TestRecoverCloneChangeOwnerSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneFile(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneChangeOwnerSuccessNotReturn";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kChangeOwner,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// RenameCloneFile阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneHasNotRenameCloneFile) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneFile(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    std::string dstFile = "/RcvItUser1/TestRecoverCloneHasNotRenameCloneFile";
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kRenameCloneFile,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// RenameCloneFile阶段重启，但mds上已RenameCloneFile成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneRenameCloneFileSuccessNotReturn) {
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneFile(fileName, &fInfoOut));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneFile(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    std::string dstFile =
        "/RcvItUser1/TestRecoverCloneRenameCloneFileSuccessNotReturn";
    ASSERT_EQ(
        LIBCURVE_ERROR::OK,
        PrepareRenameCloneFile(fInfoOut.id, fInfoOut.id, fileName, dstFile));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kClone, testFile1_,
                     dstFile, kDefaultPoolset, fInfoOut.id, fInfoOut.id, 0,
                     CloneFileType::kFile, false,
                     CloneStep::kRenameCloneFile,
                     CloneStatus::cloning);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, true);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(dstFile, testUser1_, fakeData));
}

// 以下为Lazy模式用例
// CreateCloneFile阶段重启，mds上未创建文件
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotCreateCloneFile) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, 0, 0, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCreateCloneFile,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CreateCloneFile阶段重启，mds上创建文件成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyHasCreateCloneFileSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, 0, 0, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCreateCloneFile,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CreateCloneMeta阶段重启， 在mds上未创建segment
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotCreateCloneMeta) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCreateCloneMeta,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CreateCloneMeta阶段重启， 在mds上创建segment成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyCreateCloneMetaSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCreateCloneMeta,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CreateCloneChunk阶段重启，未在chunkserver上创建clonechunk
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotCreateCloneChunk) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCreateCloneChunk,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CreateCloneChunk阶段重启，在chunkserver上创建部分clonechunk
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyCreateCloneChunkSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCreateCloneChunk,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CompleteCloneMeta阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotCompleteCloneMeta) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCompleteCloneMeta,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CompleteCloneMeta阶段重启，同时在mds上调用CompleteCloneMeta成功但未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyCompleteCloneMetaSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCompleteCloneMeta,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// ChangeOwner阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotChangeOwner) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kChangeOwner,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// ChangeOwner阶段重启，但mds上ChangeOwner成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyChangeOwnerSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kChangeOwner,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// RenameCloneFile阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotRenameCloneFile) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kRenameCloneFile,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// RenameCloneFile阶段重启，但mds上已RenameCloneFile成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyRenameCloneFileSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRenameCloneFile(fInfoOut.id, testFd1_,
                                                         fileName, testFile1_));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kRenameCloneFile,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    ASSERT_TRUE(WaitMetaInstalledSuccess(testUser1_, uuid1, false));
    // Flatten
    int ret = Flatten(testUser1_, uuid1);
    ASSERT_EQ(0, ret);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// RecoverChunk阶段重启，在chunkserver上未调用RecoverChunk
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotRecoverChunk) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRenameCloneFile(fInfoOut.id, testFd1_,
                                                         fileName, testFile1_));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kRecoverChunk,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// RecoverChunk阶段重启，在chunkserver上部分调用RecoverChunk
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyRecoverChunkSuccssNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRenameCloneFile(fInfoOut.id, testFd1_,
                                                         fileName, testFile1_));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec, true));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kRecoverChunk,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CompleteCloneFile阶段重启
TEST_F(SnapshotCloneServerTest, TestRecoverCloneLazyHasNotCompleteCloneFile) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRenameCloneFile(fInfoOut.id, testFd1_,
                                                         fileName, testFile1_));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec, true));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCompleteCloneFile,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

// CompleteCloneFile阶段重启，但mds上CompleteCloneFile已成功未返回
TEST_F(SnapshotCloneServerTest,
       TestRecoverCloneLazyCompleteCloneFileSuccessNotReturn) {
    std::string snapId;
    PrepareSnapshotForTestFile1(&snapId);
    std::string uuid1 = UUIDGenerator().GenerateUUID();
    std::string fileName = std::string(cloneTempDir_) + "/" + uuid1;
    FInfo fInfoOut;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneFile(fileName, &fInfoOut, true));

    std::vector<SegmentInfo> segInfoOutVec;
    ASSERT_EQ(LIBCURVE_ERROR::OK,
              PrepareCreateCloneMeta(&fInfoOut, fileName, &segInfoOutVec));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCreateCloneChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneMeta(uuid1));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareChangeOwner(fileName));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRenameCloneFile(fInfoOut.id, testFd1_,
                                                         fileName, testFile1_));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareRecoverChunk(segInfoOutVec, true));

    ASSERT_EQ(LIBCURVE_ERROR::OK, PrepareCompleteCloneFile(testFile1_));

    CloneInfo cloneInfo(uuid1, testUser1_,
                     CloneTaskType::kRecover, snapId,
                     testFile1_, kDefaultPoolset, fInfoOut.id, testFd1_, 0,
                     CloneFileType::kSnapshot, true,
                     CloneStep::kCompleteCloneFile,
                     CloneStatus::recovering);

    cluster_->metaStore_->AddCloneInfo(cloneInfo);

    pid_t pid = cluster_->RestartSnapshotCloneServer(1);
    LOG(INFO) << "SnapshotCloneServer 1 restarted, pid = " << pid;
    ASSERT_GT(pid, 0);

    bool success1 = CheckCloneOrRecoverSuccess(testUser1_, uuid1, false);
    ASSERT_TRUE(success1);

    std::string fakeData(4096, 'x');
    ASSERT_TRUE(CheckFileData(testFile1_, testUser1_, fakeData));
}

}  // namespace snapshotcloneserver
}  // namespace curve

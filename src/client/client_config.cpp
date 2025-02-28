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
 * File Created: Tuesday, 23rd October 2018 4:57:56 pm
 * Author: tongguangxun
 */

#include "src/client/client_config.h"

#include <glog/logging.h>

#include <string>
#include <vector>

#include "src/common/net_common.h"
#include "src/common/string_util.h"

#define RETURN_IF_FALSE(x) \
    if (x == false) {      \
        return -1;         \
    }

namespace curve {
namespace client {

static constexpr int kDefaultDummyServerPort = 9000;

int ClientConfig::Init(const std::string& configpath) {
    conf_.SetConfigPath(configpath);

    LOG(INFO) << "Init config from " << configpath;

    if (!conf_.LoadConfig()) {
        LOG(ERROR) << "Load config failed, config path = " << configpath;
        return -1;
    }

    conf_.ExposeMetric("client_config");

    bool ret = false;

    ret = conf_.GetIntValue("global.logLevel", &fileServiceOption_.loginfo.logLevel);   // NOLINT
    LOG_IF(ERROR, ret == false) << "config no global.logLevel info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetStringValue("global.logPath", &fileServiceOption_.loginfo.logPath);            // NOLINT
    LOG_IF(ERROR, ret == false) << "config no global.logPath info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("global.fileIOSplitMaxSizeKB",
          &fileServiceOption_.ioOpt.ioSplitOpt.fileIOSplitMaxSizeKB);
    LOG_IF(ERROR, ret == false) << "config no global.fileIOSplitMaxSizeKB info";           // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetBoolValue("chunkserver.enableAppliedIndexRead",
          &fileServiceOption_.ioOpt.ioSenderOpt.chunkserverEnableAppliedIndexRead);        // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.enableAppliedIndexRead info";     // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("chunkserver.opMaxRetry",
          &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverOPMaxRetry);    // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.opMaxRetry info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("chunkserver.opRetryIntervalUS",
        &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverOPRetryIntervalUS);   // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.opRetryIntervalUS info";            // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("chunkserver.rpcTimeoutMS",
        &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverRPCTimeoutMS);         // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.rpcTimeoutMS info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("chunkserver.maxRetrySleepIntervalUS",
        &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverMaxRetrySleepIntervalUS);   // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.maxRetrySleepIntervalUS info";   // NOLINT

    ret = conf_.GetUInt64Value("chunkserver.maxRPCTimeoutMS",
        &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverMaxRPCTimeoutMS);   // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.maxRPCTimeoutMS info";

    ret = conf_.GetUInt32Value(
        "chunkserver.maxStableTimeoutTimes",
        &fileServiceOption_.ioOpt.metaCacheOpt.chunkserverUnstableOption.maxStableChunkServerTimeoutTimes);  // NOLINT
    LOG_IF(ERROR, ret == false)
        << "config no chunkserver.maxStableTimeoutTimes info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value(
        "chunkserver.checkHealthTimeoutMs",
        &fileServiceOption_.ioOpt.metaCacheOpt.chunkserverUnstableOption.checkHealthTimeoutMS);  // NOLINT
    LOG_IF(ERROR, ret == false)
        << "config no chunkserver.checkHealthTimeoutMs info";  // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value(
        "chunkserver.serverStableThreshold",
        &fileServiceOption_.ioOpt.metaCacheOpt.chunkserverUnstableOption.serverUnstableThreshold);  // NOLINT
    LOG_IF(ERROR, ret == false)
        << "config no chunkserver.serverStableThreshold info";  // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("chunkserver.minRetryTimesForceTimeoutBackoff",
        &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverMinRetryTimesForceTimeoutBackoff);  // NOLINT
    LOG_IF(ERROR, ret == false)
        << "config no chunkserver.minRetryTimesForceTimeoutBackoff "
        << "using default value";

    ret = conf_.GetUInt64Value("chunkserver.maxRetryTimesBeforeConsiderSuspend",
        &fileServiceOption_.ioOpt.ioSenderOpt.failRequestOpt.chunkserverMaxRetryTimesBeforeConsiderSuspend);   // NOLINT
    LOG_IF(ERROR, ret == false) << "config no chunkserver.maxRetryTimesBeforeConsiderSuspend info";             // NOLINT

    ret = conf_.GetUInt64Value("global.fileMaxInFlightRPCNum",
        &fileServiceOption_.ioOpt.ioSenderOpt.inflightOpt.fileMaxInFlightRPCNum);   // NOLINT
    LOG_IF(ERROR, ret == false) << "config no global.fileMaxInFlightRPCNum info";   // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("metacache.getLeaderRetry",
        &fileServiceOption_.ioOpt.metaCacheOpt.metacacheGetLeaderRetry);
    LOG_IF(ERROR, ret == false) << "config no metacache.getLeaderRetry info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("metacache.rpcRetryIntervalUS",
        &fileServiceOption_.ioOpt.metaCacheOpt.metacacheRPCRetryIntervalUS);
    LOG_IF(ERROR, ret == false) << "config no metacache.rpcRetryIntervalUS info";   // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("metacache.getLeaderTimeOutMS",
        &fileServiceOption_.ioOpt.metaCacheOpt.metacacheGetLeaderRPCTimeOutMS);
    LOG_IF(ERROR, ret == false) << "config no metacache.getLeaderTimeOutMS info";   // NOLINT
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("schedule.queueCapacity",
        &fileServiceOption_.ioOpt.reqSchdulerOpt.scheduleQueueCapacity);
    LOG_IF(ERROR, ret == false) << "config no schedule.queueCapacity info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("schedule.threadpoolSize",
        &fileServiceOption_.ioOpt.reqSchdulerOpt.scheduleThreadpoolSize);
    LOG_IF(ERROR, ret == false) << "config no schedule.threadpoolSize info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("mds.refreshTimesPerLease",
        &fileServiceOption_.leaseOpt.mdsRefreshTimesPerLease);
    LOG_IF(ERROR, ret == false) << "config no mds.refreshTimesPerLease info";
    RETURN_IF_FALSE(ret);

    fileServiceOption_.ioOpt.reqSchdulerOpt.ioSenderOpt =
        fileServiceOption_.ioOpt.ioSenderOpt;

    ret = conf_.GetUInt64Value("isolation.taskQueueCapacity",
        &fileServiceOption_.ioOpt.taskThreadOpt.isolationTaskQueueCapacity);
    LOG_IF(ERROR, ret == false) << "config no isolation.taskQueueCapacity info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("isolation.taskThreadPoolSize",
        &fileServiceOption_.ioOpt.taskThreadOpt.isolationTaskThreadPoolSize);
    LOG_IF(ERROR, ret == false) << "config no isolation.taskThreadPoolSize info";   // NOLINT
    RETURN_IF_FALSE(ret);

    std::string metaAddr;
    ret = conf_.GetStringValue("mds.listen.addr", &metaAddr);
    LOG_IF(ERROR, ret == false) << "config no mds.listen.addr info";
    RETURN_IF_FALSE(ret);

    std::vector<std::string> mdsAddr;
    common::SplitString(metaAddr, ",", &mdsAddr);
    if (mdsAddr.empty()) {
        LOG(ERROR) << "mds.listen.addr seems invalid or empty, `" << metaAddr
                   << "`'";
        return -1;
    }

    fileServiceOption_.metaServerOpt.rpcRetryOpt.addrs.assign(mdsAddr.begin(),
                                                        mdsAddr.end());
    for (auto& addr : fileServiceOption_.metaServerOpt.rpcRetryOpt.addrs) {
        if (!curve::common::NetCommon::CheckAddressValid(addr)) {
            LOG(ERROR) << "address valid!";
            return -1;
        }
    }

    ret = conf_.GetUInt64Value("mds.rpcTimeoutMS",
        &fileServiceOption_.metaServerOpt.rpcRetryOpt.rpcTimeoutMs);
    LOG_IF(ERROR, ret == false) << "config no mds.rpcTimeoutMS info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value("mds.rpcRetryIntervalUS",
        &fileServiceOption_.metaServerOpt.rpcRetryOpt.rpcRetryIntervalUS);
    LOG_IF(ERROR, ret == false) << "config no mds.rpcRetryIntervalUS info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("mds.maxRPCTimeoutMS",
        &fileServiceOption_.metaServerOpt.rpcRetryOpt.maxRPCTimeoutMS);
    LOG_IF(ERROR, ret == false) << "config no mds.maxRPCTimeoutMS info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt64Value("mds.maxRetryMS",
        &fileServiceOption_.metaServerOpt.mdsMaxRetryMS);
    LOG_IF(WARNING, ret == false) << "config no mds.maxRetryMS info";

    ret = conf_.GetUInt32Value("mds.maxFailedTimesBeforeChangeMDS",
        &fileServiceOption_.metaServerOpt.rpcRetryOpt.maxFailedTimesBeforeChangeAddr);  // NOLINT
    LOG_IF(ERROR, ret == false) << "config no mds.maxFailedTimesBeforeChangeMDS info";  // NOLINT

    ret = conf_.GetUInt64Value("mds.normalRetryTimesBeforeTriggerWait",
        &fileServiceOption_.metaServerOpt.rpcRetryOpt.normalRetryTimesBeforeTriggerWait); // NOLINT
    LOG_IF(ERROR, ret == false) << "config no mds.normalRetryTimesBeforeTriggerWait info";  // NOLINT

    ret = conf_.GetUInt64Value("mds.waitSleepMs",
        &fileServiceOption_.metaServerOpt.rpcRetryOpt.waitSleepMs);
    LOG_IF(ERROR, ret == false) << "config no mds.waitSleepMs info";

    ret = conf_.GetBoolValue("mds.registerToMDS",
        &fileServiceOption_.commonOpt.mdsRegisterToMDS);
    LOG_IF(ERROR, ret == false) << "config no mds.registerToMDS info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetBoolValue("global.turnOffHealthCheck",
                             &fileServiceOption_.commonOpt.turnOffHealthCheck);
    LOG_IF(WARNING, ret == false)
        << "config no global.turnOffHealthCheck info, using default value "
        << fileServiceOption_.commonOpt.turnOffHealthCheck;

    ret = conf_.GetUInt32Value(
        "closefd.timeout",
        &fileServiceOption_.ioOpt.closeFdThreadOption.fdTimeout);
    LOG_IF(WARNING, ret == false)
        << "config no closefd.timeout info, using default value "
        << fileServiceOption_.ioOpt.closeFdThreadOption.fdTimeout;

    ret = conf_.GetUInt32Value(
        "closefd.timeInterval",
        &fileServiceOption_.ioOpt.closeFdThreadOption.fdCloseTimeInterval);
    LOG_IF(WARNING, ret == false)
        << "config no closefd.timeInterval info, using default value "
        << fileServiceOption_.ioOpt.closeFdThreadOption.fdCloseTimeInterval;

    ret = conf_.GetBoolValue(
        "throttle.enable",
        &fileServiceOption_.ioOpt.throttleOption.enable);
    LOG_IF(WARNING, ret == false)
        << "config no throttle.enable info, using default value "
        << fileServiceOption_.ioOpt.throttleOption.enable;

    ret = conf_.GetBoolValue("discard.enable",
                             &fileServiceOption_.ioOpt.discardOption.enable);
    LOG_IF(ERROR, ret == false) << "config no discard.enable info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value(
        "discard.granularity",
        &fileServiceOption_.ioOpt.metaCacheOpt.discardGranularity);
    LOG_IF(ERROR, ret == false) << "config no discard.granularity info";
    RETURN_IF_FALSE(ret);

    ret = conf_.GetUInt32Value(
        "discard.taskDelayMs",
        &fileServiceOption_.ioOpt.discardOption.taskDelayMs);
    LOG_IF(ERROR, ret == false) << "config no discard.taskDelayMs info";
    RETURN_IF_FALSE(ret);

    // only client side need these follow 5 options
    ret = conf_.GetUInt32Value("csClientOpt.rpcTimeoutMs",
        &fileServiceOption_.csClientOpt.rpcTimeoutMs);
    LOG_IF(WARNING, ret = false) << "config no csClientOpt.rpcTimeoutMs info";

    ret = conf_.GetUInt32Value("csClientOpt.rpcMaxTry",
        &fileServiceOption_.csClientOpt.rpcMaxTry);
    LOG_IF(WARNING, ret = false) << "config no csClientOpt.rpcMaxTry info";

    ret = conf_.GetUInt32Value("csClientOpt.rpcIntervalUs",
        &fileServiceOption_.csClientOpt.rpcIntervalUs);
    LOG_IF(WARNING, ret = false) << "config no csClientOpt.rpcIntervalUs info";

    ret = conf_.GetUInt32Value("csClientOpt.rpcMaxTimeoutMs",
        &fileServiceOption_.csClientOpt.rpcIntervalUs);
    LOG_IF(WARNING, ret = false)
        << "config no csClientOpt.rpcMaxTimeoutMs info";

    ret = conf_.GetUInt32Value("csBroadCasterOpt.broadCastMaxNum",
        &fileServiceOption_.csBroadCasterOpt.broadCastMaxNum);
    LOG_IF(WARNING, ret = false)
        << "config no csBroadCasterOpt.broadCastMaxNum info";

    return 0;
}

int ClientConfig::GetDummyserverStartPort() {
    return conf_.GetIntValue("global.metricDummyServerStartPort",
                             kDefaultDummyServerPort);
}

}   // namespace client
}   // namespace curve

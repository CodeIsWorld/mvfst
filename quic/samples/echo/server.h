/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <folly/Synchronized.h>
#include <glog/logging.h>

#include <vector>
#include <string>
#include <utility>
#include <memory>

#include <quic/samples/echo/EchoHandler.h>
#include <quic/samples/echo/LogQuicStats.h>
#include <quic/server/QuicServer.h>
#include <quic/common/test/TestUtils.h>
#include <quic/server/QuicServerTransport.h>
#include <quic/server/QuicSharedUDPSocketFactory.h>

namespace quic {
namespace samples {

class ZQuicServer {
  using OnMessageCallback =
      std::function<void(const uint64_t stream_id, const std::string& msg)>;

 public:
  explicit ZQuicServer(
      OnMessageCallback callback,
      const std::string& host,
      uint16_t port,
      bool useDatagrams,
      uint64_t activeConnIdLimit,
      bool enableMigration,
      bool enableStreamGroups);

  ~ZQuicServer();

  void start();

 private:
  std::string host_;
  uint16_t port_;
  folly::EventBase eventbase_;
  std::shared_ptr<quic::QuicServer> server_;
  OnMessageCallback callback_;
};
} // namespace samples
} // namespace quic

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "quic/samples/echo/server.h"

#include <utility>
#include <vector>

#include <quic/samples/echo/echo_server_transport_factory.h>

namespace quic {
namespace samples {

ZQuicServer::ZQuicServer(
    OnMessageCallback callback,
    const std::string& host = "::1",
    uint16_t port = 6666,
    bool useDatagrams = false,
    uint64_t activeConnIdLimit = 10,
    bool enableMigration = true,
    bool enableStreamGroups = false)
    : host_(host),
      port_(port),
      server_(QuicServer::createQuicServer()),
      callback_(callback) {
  auto echo_server_transport_factory =
      std::make_unique<EchoServerTransportFactory>(callback_, useDatagrams);
  server_->setQuicServerTransportFactory(std::move(echo_server_transport_factory));
  server_->setTransportStatsCallbackFactory(
      std::make_unique<LogQuicStatsFactory>());
  auto serverCtx = quic::test::createServerCtx();
  serverCtx->setClock(std::make_shared<fizz::SystemClock>());
  server_->setFizzContext(serverCtx);

  auto settingsCopy = server_->getTransportSettings();
  settingsCopy.datagramConfig.enabled = useDatagrams;
  settingsCopy.selfActiveConnectionIdLimit = activeConnIdLimit;
  settingsCopy.disableMigration = !enableMigration;
  if (enableStreamGroups) {
    settingsCopy.notifyOnNewStreamsExplicitly = true;
    settingsCopy.maxStreamGroupsAdvertized = 1024;
  }
  server_->setTransportSettings(std::move(settingsCopy));
}

ZQuicServer::~ZQuicServer() {
  server_->shutdown();
}

void ZQuicServer::start() {
  // Create a SocketAddress and the default or passed in host.
  folly::SocketAddress addr1(host_.c_str(), port_);
  addr1.setFromHostPort(host_, port_);
  server_->start(addr1, 0);
  LOG(INFO) << "Echo server started at: " << addr1.describe();
  eventbase_.loopForever();
}

} // namespace samples
} // namespace quic

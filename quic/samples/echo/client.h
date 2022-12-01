/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <map>

#include <glog/logging.h>

#include <folly/fibers/Baton.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <fizz/protocol/CertificateVerifier.h>

#include <quic/api/QuicSocket.h>
#include <quic/client/QuicClientTransport.h>
#include <quic/common/BufUtil.h>
// #include <quic/common/test/TestClientUtils.h>
// #include <quic/common/test/TestUtils.h>
#include <quic/fizz/client/handshake/FizzClientQuicHandshakeContext.h>
#include <quic/samples/echo/LogQuicStats.h>

namespace quic {
namespace samples {
constexpr size_t kNumTestStreamGroups = 2;

class TestCertificateVerifier : public fizz::CertificateVerifier {
 public:
  ~TestCertificateVerifier() override = default;

  std::shared_ptr<const folly::AsyncTransportCertificate> verify(
      const std::vector<std::shared_ptr<const fizz::PeerCert>>& certs)
      const override {
    return certs.front();
  }

  [[nodiscard]] std::vector<fizz::Extension> getCertificateRequestExtensions()
      const override {
    return std::vector<fizz::Extension>();
  }
};

class ZQuicClient : public quic::QuicSocket::ConnectionSetupCallback,
                   public quic::QuicSocket::ConnectionCallback,
                   public quic::QuicSocket::ReadCallback,
                   public quic::QuicSocket::WriteCallback,
                   public quic::QuicSocket::DatagramCallback {
 public:
  ZQuicClient(
      const std::string& host,
      uint16_t port,
      bool useDatagrams,
      uint64_t activeConnIdLimit,
      bool enableMigration,
      bool enableStreamGroups);

  void readAvailable(quic::StreamId streamId) noexcept override;
  void readAvailableWithGroup(
      quic::StreamId streamId,
      quic::StreamGroupId groupId) noexcept override;
  void readError(quic::StreamId streamId, QuicError error) noexcept override;
  void readErrorWithGroup(
      quic::StreamId streamId,
      quic::StreamGroupId groupId,
      QuicError error) noexcept override;
  void onNewBidirectionalStream(quic::StreamId id) noexcept override;
  void onNewBidirectionalStreamGroup(
      quic::StreamGroupId groupId) noexcept override;
  void onNewBidirectionalStreamInGroup(
      quic::StreamId id,
      quic::StreamGroupId groupId) noexcept override;
  void onNewUnidirectionalStream(quic::StreamId id) noexcept override;
  void onNewUnidirectionalStreamGroup(
      quic::StreamGroupId groupId) noexcept override;
  void onNewUnidirectionalStreamInGroup(
      quic::StreamId id,
      quic::StreamGroupId groupId) noexcept override;
  void onStopSending(
      quic::StreamId id,
      quic::ApplicationErrorCode /*error*/) noexcept override;
  void onConnectionEnd() noexcept override;
  void onConnectionSetupError(QuicError error) noexcept override;
  void onConnectionError(QuicError error) noexcept override;
  void onTransportReady() noexcept override;
  void onStreamWriteReady(quic::StreamId id, uint64_t maxToSend) noexcept
      override;
  void onStreamWriteError(quic::StreamId id, QuicError error) noexcept override;

  void onDatagramsAvailable() noexcept override;

  void start(std::string token);

  ~ZQuicClient() override = default;

 private:
  [[nodiscard]] quic::StreamGroupId getNextGroupId();
  void sendMessage(quic::StreamId id, BufQueue& data);

  std::string host_;
  uint16_t port_;
  bool useDatagrams_;
  uint64_t activeConnIdLimit_;
  bool enableMigration_;
  bool enableStreamGroups_;
  std::shared_ptr<quic::QuicClientTransport> quicClient_;
  std::map<quic::StreamId, BufQueue> pendingOutput_;
  std::map<quic::StreamId, uint64_t> recvOffsets_;
  folly::fibers::Baton startDone_;
  std::array<StreamGroupId, kNumTestStreamGroups> streamGroups_;
  size_t curGroupIdIdx_{0};
};
} // namespace samples
} // namespace quic

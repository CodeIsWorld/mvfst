/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "quic/samples/echo/client.h"

#include <fstream>
#include <utility>

namespace quic {
namespace samples {

ZQuicClient::ZQuicClient(
    const std::string& host,
    uint16_t port,
    bool useDatagrams,
    uint64_t activeConnIdLimit,
    bool enableMigration,
    bool enableStreamGroups)
    : host_(host),
      port_(port),
      useDatagrams_(useDatagrams),
      activeConnIdLimit_(activeConnIdLimit),
      enableMigration_(enableMigration),
      enableStreamGroups_(enableStreamGroups) {}

void ZQuicClient::readAvailable(quic::StreamId streamId) noexcept {
  static std::string recevied_buffer;
  auto readData = quicClient_->read(streamId, 0);
  if (readData.hasError()) {
    LOG(ERROR) << "ZQuicClient failed read from stream=" << streamId
               << ", error=" << (uint32_t)readData.error();
  }
  auto copy = readData->first->clone();
  if (recvOffsets_.find(streamId) == recvOffsets_.end()) {
    recvOffsets_[streamId] = copy->length();
  } else {
    recvOffsets_[streamId] += copy->length();
  }
  recevied_buffer.append(copy->moveToFbString().toStdString());
  bool eof = readData->second;
  if (eof) {
    LOG(INFO) << "Client received len:" << recevied_buffer.length()
              << " data=" << recevied_buffer << " on stream=" << streamId;
    recevied_buffer.clear();
  }
}

void ZQuicClient::readAvailableWithGroup(
    quic::StreamId streamId,
    quic::StreamGroupId groupId) noexcept {
  auto readData = quicClient_->read(streamId, 0);
  if (readData.hasError()) {
    LOG(ERROR) << "ZQuicClient failed read from stream=" << streamId
               << ", groupId=" << groupId
               << ", error=" << (uint32_t)readData.error();
  }
  auto copy = readData->first->clone();
  if (recvOffsets_.find(streamId) == recvOffsets_.end()) {
    recvOffsets_[streamId] = copy->length();
  } else {
    recvOffsets_[streamId] += copy->length();
  }
  LOG(INFO) << "Client received data=" << copy->moveToFbString().toStdString()
            << " on stream=" << streamId << ", groupId=" << groupId;
}

void ZQuicClient::readError(quic::StreamId streamId, QuicError error) noexcept {
  LOG(ERROR) << "ZQuicClient failed read from stream=" << streamId
             << ", error=" << toString(error);
  // A read error only terminates the ingress portion of the stream state.
  // Your application should probably terminate the egress portion via
  // resetStream
}

void ZQuicClient::readErrorWithGroup(
    quic::StreamId streamId,
    quic::StreamGroupId groupId,
    QuicError error) noexcept {
  LOG(ERROR) << "ZQuicClient failed read from stream=" << streamId
             << ", groupId=" << groupId << ", error=" << toString(error);
}

void ZQuicClient::onNewBidirectionalStream(quic::StreamId id) noexcept {
  LOG(INFO) << "ZQuicClient: new bidirectional stream=" << id;
  quicClient_->setReadCallback(id, this);
}

void ZQuicClient::onNewBidirectionalStreamGroup(
    quic::StreamGroupId groupId) noexcept {
  LOG(INFO) << "ZQuicClient: new bidirectional stream group=" << groupId;
}

void ZQuicClient::onNewBidirectionalStreamInGroup(
    quic::StreamId id,
    quic::StreamGroupId groupId) noexcept {
  LOG(INFO) << "ZQuicClient: new bidirectional stream=" << id
            << " in group=" << groupId;
  quicClient_->setReadCallback(id, this);
}

void ZQuicClient::onNewUnidirectionalStream(quic::StreamId id) noexcept {
  LOG(INFO) << "ZQuicClient: new unidirectional stream=" << id;
  quicClient_->setReadCallback(id, this);
}

void ZQuicClient::onNewUnidirectionalStreamGroup(
    quic::StreamGroupId groupId) noexcept {
  LOG(INFO) << "ZQuicClient: new unidirectional stream group=" << groupId;
}

void ZQuicClient::onNewUnidirectionalStreamInGroup(
    quic::StreamId id,
    quic::StreamGroupId groupId) noexcept {
  LOG(INFO) << "ZQuicClient: new unidirectional stream=" << id
            << " in group=" << groupId;
  quicClient_->setReadCallback(id, this);
}

void ZQuicClient::onStopSending(
    quic::StreamId id,
    quic::ApplicationErrorCode) noexcept {
  VLOG(10) << "ZQuicClient got StopSending stream id=" << id;
}

void ZQuicClient::onConnectionEnd() noexcept {
  LOG(INFO) << "ZQuicClient connection end";
}

void ZQuicClient::onConnectionSetupError(QuicError error) noexcept {
  onConnectionError(std::move(error));
}

void ZQuicClient::onConnectionError(QuicError error) noexcept {
  LOG(ERROR) << "ZQuicClient error: " << toString(error.code)
             << "; errStr=" << error.message;
  startDone_.post();
}

void ZQuicClient::onTransportReady() noexcept {
  startDone_.post();
}

void ZQuicClient::onStreamWriteReady(
    quic::StreamId id,
    uint64_t maxToSend) noexcept {
  LOG(INFO) << "ZQuicClient socket is write ready with maxToSend=" << maxToSend;
  sendMessage(id, pendingOutput_[id]);
}

void ZQuicClient::onStreamWriteError(
    quic::StreamId id,
    QuicError error) noexcept {
  LOG(ERROR) << "ZQuicClient write error with stream=" << id
             << " error=" << toString(error);
}

void ZQuicClient::onDatagramsAvailable() noexcept {
  auto res = quicClient_->readDatagrams();
  if (res.hasError()) {
    LOG(ERROR) << "ZQuicClient failed reading datagrams; error=" << res.error();
    return;
  }
  for (const auto& datagram : *res) {
    LOG(INFO) << "Client received datagram ="
              << datagram.bufQueue()
                     .front()
                     ->cloneCoalesced()
                     ->moveToFbString()
                     .toStdString();
  }
}

void ZQuicClient::start(std::string token) {
  folly::ScopedEventBaseThread networkThread("EchoClientThread");
  auto evb = networkThread.getEventBase();
  folly::SocketAddress addr(host_.c_str(), port_);

  evb->runInEventBaseThreadAndWait([&] {
    auto sock = std::make_unique<folly::AsyncUDPSocket>(evb);
    auto fizzClientContext =
        FizzClientQuicHandshakeContext::Builder()
            .setCertificateVerifier(std::make_unique<TestCertificateVerifier>())
            .build();
    quicClient_ = std::make_shared<quic::QuicClientTransport>(
        evb, std::move(sock), std::move(fizzClientContext));
    quicClient_->setHostname("echo.com");
    quicClient_->addNewPeerAddress(addr);
    if (!token.empty()) {
      quicClient_->setNewToken(token);
    }
    if (useDatagrams_) {
      auto res = quicClient_->setDatagramCallback(this);
      CHECK(res.hasValue()) << res.error();
    }

    TransportSettings settings;
    settings.datagramConfig.enabled = useDatagrams_;
    settings.selfActiveConnectionIdLimit = activeConnIdLimit_;
    settings.disableMigration = !enableMigration_;
    if (enableStreamGroups_) {
      settings.notifyOnNewStreamsExplicitly = true;
      settings.maxStreamGroupsAdvertized = kNumTestStreamGroups;
    }
    quicClient_->setTransportSettings(settings);

    quicClient_->setTransportStatsCallback(
        std::make_shared<LogQuicStats>("client"));

    LOG(INFO) << "ZQuicClient connecting to " << addr.describe();
    quicClient_->start(this, this);
  });

  startDone_.wait();

  std::string message;
  bool closed = false;
  auto client = quicClient_;

  if (enableStreamGroups_) {
    // Generate two groups.
    for (size_t i = 0; i < kNumTestStreamGroups; ++i) {
      auto groupId = quicClient_->createBidirectionalStreamGroup();
      CHECK(groupId.hasValue())
          << "Failed to generate a stream group: " << groupId.error();
      streamGroups_[i] = *groupId;
    }
  }

  auto sendMessageInStream = [&]() {
    if (message == "/close") {
      quicClient_->close(folly::none);
      closed = true;
      return;
    }

    // create new stream for each message
    auto streamId = client->createBidirectionalStream().value();
    client->setReadCallback(streamId, this);
    pendingOutput_[streamId].append(folly::IOBuf::copyBuffer(message));
    sendMessage(streamId, pendingOutput_[streamId]);
  };

  auto sendMessageInStreamGroup = [&]() {
    // create new stream for each message
    auto streamId = client->createBidirectionalStreamInGroup(getNextGroupId());
    CHECK(streamId.hasValue())
        << "Failed to generate stream id in group: " << streamId.error();
    client->setReadCallback(*streamId, this);
    pendingOutput_[*streamId].append(folly::IOBuf::copyBuffer(message));
    sendMessage(*streamId, pendingOutput_[*streamId]);
  };

  // loop until Ctrl+D
  while (!closed && std::getline(std::cin, message)) {
    if (message.empty()) {
      continue;
    }

    // auto streamId = client->createBidirectionalStream().value();
    // std::string filename = "/home/zelos/Temp/" + message + ".jpg";
    // std::ifstream infile;
    // std::string buffer;
    // uint64_t size = 1800;
    // bool eof = false;
    // try {
    //   infile.open(filename, std::ios::binary);
    //   if (!infile.is_open()) {
    //     return;
    //   }
    //   while (!infile.eof()) {
    //     buffer.resize(size);
    //     infile.read(buffer.data(), size);
    //     auto real_size = infile.gcount();
    //     if (real_size != size) {
    //       buffer.resize(real_size);
    //       eof = true;
    //     }
    //     quicClient_->writeChain(
    //         streamId, folly::IOBuf::copyBuffer(buffer), eof);
    //   }
    //   infile.close();
    // } catch (std::exception& e) {
    //   std::cerr << "read file [" << filename << "] failed";
    //   infile.close();
    //   return;
    // }

    evb->runInEventBaseThreadAndWait([=] {
      if (enableStreamGroups_) {
        sendMessageInStreamGroup();
      } else {
        sendMessageInStream();
      }
    });
  }
  LOG(INFO) << "ZQuicClient stopping client";
}

quic::StreamGroupId ZQuicClient::getNextGroupId() {
  return streamGroups_[(curGroupIdIdx_++) % kNumTestStreamGroups];
}

void ZQuicClient::sendMessage(quic::StreamId id, BufQueue& data) {
  auto message = data.move();
  auto res = useDatagrams_
      ? quicClient_->writeDatagram(message->clone())
      : quicClient_->writeChain(id, message->clone(), true);
  if (res.hasError()) {
    LOG(ERROR) << "ZQuicClient writeChain error=" << uint32_t(res.error());
  } else {
    auto str = message->moveToFbString().toStdString();
    LOG(INFO) << "ZQuicClient wrote \"" << str << "\""
              << ", len=" << str.size() << " on stream=" << id;
    // sent whole message
    pendingOutput_.erase(id);
  }
}

} // namespace samples
} // namespace quic

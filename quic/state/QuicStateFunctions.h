/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <quic/QuicConstants.h>
#include <quic/codec/QuicPacketBuilder.h>
#include <quic/codec/Types.h>
#include <quic/state/StateData.h>

namespace quic {

void updateAckSendStateOnRecvPacket(
    QuicConnectionStateBase& conn,
    AckState& ackState,
    uint64_t distanceFromExpectedPacketNum,
    bool pktHasRetransmittableData,
    bool pktHasCryptoData,
    bool initPktNumSpace = false);

void updateAckStateOnAckTimeout(QuicConnectionStateBase& conn);

void updateAckSendStateOnSentPacketWithAcks(
    QuicConnectionStateBase& conn,
    AckState& ackState,
    PacketNum largestAckScheduled);

void updateRtt(
    QuicConnectionStateBase& conn,
    const std::chrono::microseconds rttSample,
    const std::chrono::microseconds ackDelay);

bool isConnectionPaced(const QuicConnectionStateBase& conn) noexcept;

AckState& getAckState(
    QuicConnectionStateBase& conn,
    PacketNumberSpace pnSpace) noexcept;

const AckState& getAckState(
    const QuicConnectionStateBase& conn,
    PacketNumberSpace pnSpace) noexcept;

AckStateVersion currentAckStateVersion(
    const QuicConnectionStateBase& conn) noexcept;

PacketNum getNextPacketNum(
    const QuicConnectionStateBase& conn,
    PacketNumberSpace pnSpace) noexcept;

void increaseNextPacketNum(
    QuicConnectionStateBase& conn,
    PacketNumberSpace pnSpace) noexcept;

/**
 * Update largestReceivedPacketNum in ackState with packetNum. Return the
 * distance from the next packet number we expect to receive.
 */
template <typename ClockType = quic::Clock>
uint64_t updateLargestReceivedPacketNum(
    AckState& ackState,
    PacketNum packetNum,
    TimePoint receivedTime) {
  PacketNum expectedNextPacket = 0;
  if (ackState.largestRecvdPacketNum) {
    expectedNextPacket = *ackState.largestRecvdPacketNum + 1;
  }
  ackState.largestRecvdPacketNum = std::max<PacketNum>(
      ackState.largestRecvdPacketNum.value_or(packetNum), packetNum);
  ackState.acks.insert(packetNum);
  if (ackState.largestRecvdPacketNum == packetNum) {
    ackState.largestRecvdPacketTime = receivedTime;
  }
  static_assert(ClockType::is_steady, "Needs steady clock");

  ackState.lastRecvdPacketInfo.assign({packetNum, receivedTime});

  if (packetNum >= expectedNextPacket) {
    if (ackState.recvdPacketInfos.size() == kMaxReceivedPktsTimestampsStored) {
      ackState.recvdPacketInfos.pop_front();
    }
    ackState.recvdPacketInfos.emplace_back(
        RecvdPacketInfo{packetNum, receivedTime});
  }

  if (expectedNextPacket) {
    return (packetNum > expectedNextPacket) ? packetNum - expectedNextPacket
                                            : expectedNextPacket - packetNum;
  } else {
    return 0;
  }
}

std::deque<OutstandingPacket>::iterator getNextOutstandingPacket(
    QuicConnectionStateBase& conn,
    PacketNumberSpace packetNumberSpace,
    std::deque<OutstandingPacket>::iterator from);
std::deque<OutstandingPacket>::iterator getFirstOutstandingPacket(
    QuicConnectionStateBase& conn,
    PacketNumberSpace packetNumberSpace);

std::deque<OutstandingPacket>::reverse_iterator getLastOutstandingPacket(
    QuicConnectionStateBase& conn,
    PacketNumberSpace packetNumberSpace);
std::deque<OutstandingPacket>::reverse_iterator
getLastOutstandingPacketIncludingLost(
    QuicConnectionStateBase& conn,
    PacketNumberSpace packetNumberSpace);

bool hasReceivedPackets(const QuicConnectionStateBase& conn) noexcept;

bool hasReceivedPacketsAtLastCloseSent(
    const QuicConnectionStateBase& conn) noexcept;

bool hasNotReceivedNewPacketsSinceLastCloseSent(
    const QuicConnectionStateBase& conn) noexcept;

void updateLargestReceivedPacketsAtLastCloseSent(
    QuicConnectionStateBase& conn) noexcept;

folly::Optional<TimePoint>& getLossTime(
    QuicConnectionStateBase& conn,
    PacketNumberSpace pnSpace) noexcept;

bool canSetLossTimerForAppData(const QuicConnectionStateBase& conn) noexcept;

std::pair<folly::Optional<TimePoint>, PacketNumberSpace> earliestLossTimer(
    const QuicConnectionStateBase& conn) noexcept;

std::pair<folly::Optional<TimePoint>, PacketNumberSpace> earliestTimeAndSpace(
    const EnumArray<PacketNumberSpace, folly::Optional<TimePoint>>& times,
    bool considerAppData) noexcept;

uint64_t maximumConnectionIdsToIssue(const QuicConnectionStateBase& conn);

} // namespace quic

/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "launcherpackets.h"

#include <QByteArray>
#include <QCoreApplication>

namespace Utils {
namespace Internal {

LauncherPacket::~LauncherPacket() = default;

QByteArray LauncherPacket::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << static_cast<int>(0) << static_cast<quint8>(type) << token;
    doSerialize(stream);
    stream.device()->reset();
    stream << static_cast<int>(data.size() - sizeof(int));
    return data;
}

void LauncherPacket::deserialize(const QByteArray &data)
{
    QDataStream stream(data);
    doDeserialize(stream);
}


StartProcessPacket::StartProcessPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::StartProcess, token)
{
}

void StartProcessPacket::doSerialize(QDataStream &stream) const
{
    stream << command
           << arguments
           << workingDir
           << env
           << int(processMode)
           << writeData
           << int(processChannelMode)
           << standardInputFile
           << belowNormalPriority
           << nativeArguments
           << lowPriority
           << unixTerminalDisabled
           << useCtrlCStub;
}

void StartProcessPacket::doDeserialize(QDataStream &stream)
{
    int processModeInt;
    int processChannelModeInt;
    stream >> command
           >> arguments
           >> workingDir
           >> env
           >> processModeInt
           >> writeData
           >> processChannelModeInt
           >> standardInputFile
           >> belowNormalPriority
           >> nativeArguments
           >> lowPriority
           >> unixTerminalDisabled
           >> useCtrlCStub;
    processMode = Utils::ProcessMode(processModeInt);
    processChannelMode = QProcess::ProcessChannelMode(processChannelModeInt);
}


ProcessStartedPacket::ProcessStartedPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::ProcessStarted, token)
{
}

void ProcessStartedPacket::doSerialize(QDataStream &stream) const
{
    stream << processId;
}

void ProcessStartedPacket::doDeserialize(QDataStream &stream)
{
    stream >> processId;
}


StopProcessPacket::StopProcessPacket(quintptr token)
    : LauncherPacket(LauncherPacketType::StopProcess, token)
{
}

void StopProcessPacket::doSerialize(QDataStream &stream) const
{
    stream << int(signalType);
}

void StopProcessPacket::doDeserialize(QDataStream &stream)
{
    int signalTypeInt;
    stream >> signalTypeInt;
    signalType = SignalType(signalTypeInt);
}

void WritePacket::doSerialize(QDataStream &stream) const
{
    stream << inputData;
}

void WritePacket::doDeserialize(QDataStream &stream)
{
    stream >> inputData;
}

void ReadyReadPacket::doSerialize(QDataStream &stream) const
{
    stream << standardChannel;
}

void ReadyReadPacket::doDeserialize(QDataStream &stream)
{
    stream >> standardChannel;
}


ProcessDonePacket::ProcessDonePacket(quintptr token)
    : LauncherPacket(LauncherPacketType::ProcessDone, token)
{
}

void ProcessDonePacket::doSerialize(QDataStream &stream) const
{
    stream << exitCode
           << int(exitStatus)
           << int(error)
           << errorString
           << stdOut
           << stdErr;
}

void ProcessDonePacket::doDeserialize(QDataStream &stream)
{
    int exitStatusInt, errorInt;
    stream >> exitCode
           >> exitStatusInt
           >> errorInt
           >> errorString
           >> stdOut
           >> stdErr;
    exitStatus = QProcess::ExitStatus(exitStatusInt);
    error = QProcess::ProcessError(errorInt);
}

ShutdownPacket::ShutdownPacket() : LauncherPacket(LauncherPacketType::Shutdown, 0) { }
void ShutdownPacket::doSerialize(QDataStream &stream) const { Q_UNUSED(stream); }
void ShutdownPacket::doDeserialize(QDataStream &stream) { Q_UNUSED(stream); }

void PacketParser::setDevice(QIODevice *device)
{
    m_stream.setDevice(device);
    m_sizeOfNextPacket = -1;
}

bool PacketParser::parse()
{
    static const int commonPayloadSize = static_cast<int>(1 + sizeof(quintptr));
    if (m_sizeOfNextPacket == -1) {
        if (m_stream.device()->bytesAvailable() < static_cast<int>(sizeof m_sizeOfNextPacket))
            return false;
        m_stream >> m_sizeOfNextPacket;
        if (m_sizeOfNextPacket < commonPayloadSize)
            throw InvalidPacketSizeException(m_sizeOfNextPacket);
    }
    if (m_stream.device()->bytesAvailable() < m_sizeOfNextPacket)
        return false;
    quint8 type;
    m_stream >> type;
    m_type = static_cast<LauncherPacketType>(type);
    m_stream >> m_token;
    m_packetData = m_stream.device()->read(m_sizeOfNextPacket - commonPayloadSize);
    m_sizeOfNextPacket = -1;
    return true;
}

} // namespace Internal
} // namespace Utils

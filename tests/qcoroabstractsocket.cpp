// SPDX-FileCopyrightText: 2021 Daniel Vrátil <dvratil@kde.org>
//
// SPDX-License-Identifier: MIT

#include "testhttpserver.h"
#include "testobject.h"

#include "qcoro/network/qcoroabstractsocket.h"

#include <QTcpServer>
#include <QTcpSocket>

#include <thread>

class QCoroAbstractSocketTest : public QCoro::TestObject<QCoroAbstractSocketTest> {
    Q_OBJECT

private:
    QCoro::Task<> testWaitForConnectedTriggers_coro(QCoro::TestContext) {
        QTcpSocket socket;
        QTimer::singleShot(10ms, [this, &socket]() mutable {
            socket.connectToHost(QHostAddress::LocalHost, mServer.port());
        });

        co_await qCoro(socket).waitForConnected();

        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
    }

    QCoro::Task<> testWaitForDisconnectedTriggers_coro(QCoro::TestContext) {
        QTcpSocket socket;
        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());
        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        QTimer::singleShot(10ms, [&socket]() mutable { socket.disconnectFromHost(); });

        co_await qCoro(socket).waitForDisconnected();

        QCORO_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);
    }

    QCoro::Task<> testDoesntCoAwaitConnectedSocket_coro(QCoro::TestContext context) {
        QTcpSocket socket;
        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());

        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        context.setShouldNotSuspend();
        co_await qCoro(socket).waitForConnected();
    }

    QCoro::Task<> testDoesntCoAwaitDisconnectedSocket_coro(QCoro::TestContext context) {
        context.setShouldNotSuspend();
        mServer.setExpectTimeout(true); // no-one actually connects, so the server times out.

        QTcpSocket socket;
        QCORO_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);

        co_await qCoro(socket).waitForDisconnected();
    }

    QCoro::Task<> testConnectToServerWithArgs_coro(QCoro::TestContext) {
        QTcpSocket socket;

        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());

        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
    }

    QCoro::Task<> testWaitForConnectedTimeout_coro(QCoro::TestContext) {
        mServer.setExpectTimeout(true);
        QTcpSocket socket;

        const auto start = std::chrono::steady_clock::now();
        const bool ok = co_await qCoro(socket).waitForConnected(10ms);
        const auto end = std::chrono::steady_clock::now();
        QCORO_VERIFY(!ok);
        QCORO_VERIFY(end - start < 500ms); // give some leeway
    }

    QCoro::Task<> testWaitForDisconnectedTimeout_coro(QCoro::TestContext) {
        mServer.setExpectTimeout(true);

        QTcpSocket socket;
        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());
        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        const auto start = std::chrono::steady_clock::now();
        const bool ok = co_await qCoro(socket).waitForDisconnected(10ms);
        const auto end = std::chrono::steady_clock::now();
        QCORO_VERIFY(!ok);
        QCORO_VERIFY(end - start < 500ms);
    }

    QCoro::Task<> testReadAllTriggers_coro(QCoro::TestContext) {
        QTcpSocket socket;
        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());
        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        socket.write("GET /stream HTTP/1.1\r\n");

        QByteArray data;
        while (socket.state() == QAbstractSocket::ConnectedState) {
            data += co_await qCoro(socket).readAll();
        }
        QCORO_VERIFY(!data.isEmpty());
        data += socket.readAll(); // read what's left in the buffer

        QCORO_VERIFY(!data.isEmpty());
    }

    QCoro::Task<> testReadTriggers_coro(QCoro::TestContext) {
        QTcpSocket socket;
        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());
        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        socket.write("GET /stream HTTP/1.1\r\n");

        QByteArray data;
        while (socket.state() == QAbstractSocket::ConnectedState) {
            data += co_await qCoro(socket).read(1);
        }
        QCORO_VERIFY(!data.isEmpty());
        data += socket.readAll(); // read what's left in the buffer

        QCORO_VERIFY(!data.isEmpty());
    }

    QCoro::Task<> testReadLineTriggers_coro(QCoro::TestContext) {
        QTcpSocket socket;
        co_await qCoro(socket).connectToHost(QHostAddress::LocalHost, mServer.port());
        QCORO_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        socket.write("GET /stream HTTP/1.1\r\n");

        QByteArrayList lines;
        while (socket.state() == QAbstractSocket::ConnectedState) {
            const auto line = co_await qCoro(socket).readLine();
            if (!line.isNull()) {
                lines.push_back(line);
            }
        }

        QCORO_COMPARE(lines.size(), 14);
    }

private Q_SLOTS:
    void init() {
        mServer.start(QHostAddress::LocalHost);
    }

    void cleanup() {
        mServer.stop();
    }

    addTest(WaitForConnectedTriggers)
    addTest(WaitForConnectedTimeout)
    addTest(WaitForDisconnectedTriggers)
    addTest(WaitForDisconnectedTimeout)
    addTest(DoesntCoAwaitConnectedSocket)
    addTest(DoesntCoAwaitDisconnectedSocket)
    addTest(ConnectToServerWithArgs)
    addTest(ReadAllTriggers)
    addTest(ReadTriggers)
    addTest(ReadLineTriggers)

private:
    TestHttpServer<QTcpServer> mServer;
};

QTEST_GUILESS_MAIN(QCoroAbstractSocketTest)

#include "qcoroabstractsocket.moc"

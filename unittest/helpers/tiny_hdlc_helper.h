/*
    Copyright 2019-2020,2022 (,2022 (C) Alexey Dynda

    This file is part of Tiny Protocol Library.

    GNU General Public License Usage

    Protocol Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Protocol Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Protocol Library.  If not, see <http://www.gnu.org/licenses/>.

    Commercial License Usage

    Licensees holding valid commercial Tiny Protocol licenses may use this file in
    accordance with the commercial license agreement provided in accordance with
    the terms contained in a written agreement between you and Alexey Dynda.
    For further information contact via email on github account.
*/
#pragma once

#include "tiny_base_helper.h"
#include <functional>
#include <stdint.h>
#include <thread>
#include <atomic>
#include <string.h>
#include <string>
#include "fake_endpoint.h"
#include "proto/hdlc/high_level/hdlc.h"

class TinyHdlcHelper: public IBaseHelper<TinyHdlcHelper>
{
private:
    hdlc_struct_t m_handle;

public:
    TinyHdlcHelper(FakeEndpoint *endpoint, const std::function<void(uint8_t *, int)> &onRxFrameCb = nullptr,
                   const std::function<void(uint8_t *, int)> &onTxFrameCb = nullptr, int rx_buf_size = 1024,
                   hdlc_crc_t crc = HDLC_CRC_16);
    ~TinyHdlcHelper();
    int send(const uint8_t *buf, int len, int timeout = 1000);
    void send(int count, const std::string &msg);
    int recv(uint8_t *buf, int len, int timeout = 1000);
    int run_rx() override;
    int run_tx() override;
    int rx_count()
    {
        return m_rx_count;
    }
    int tx_count()
    {
        return m_tx_count;
    }

    void wait_until_rx_count(int count, uint32_t timeout);

    void setMcuMode()
    {
        m_tx_from_main = true;
    }

private:
    std::function<void(uint8_t *, int)> m_onRxFrameCb;
    std::function<void(uint8_t *, int)> m_onTxFrameCb;
    tiny_mutex_t m_read_mutex;
    bool m_stop_sender = false;
    std::thread *m_sender_thread = nullptr;
    std::atomic<int> m_rx_count{0};
    std::atomic<int> m_tx_count{0};
    uint8_t *m_user_rx_buf = nullptr;
    int m_user_rx_buf_size = 0;
    bool m_tx_from_main = false;

    static int onRxFrame(void *handle, void *buf, int len);
    static int onTxFrame(void *handle, const void *buf, int len);
    static void MessageSenderStatic(TinyHdlcHelper *helper, int count, std::string msg);
    void MessageSender(int count, std::string msg);
};

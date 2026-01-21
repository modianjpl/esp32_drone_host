#pragma once

#include <stdint.h>

class UsartProtocol {
    public:
        static const uint8_t MAGIC1 = 0xAA;
        static const uint8_t MAGIC2 = 0x55;
        static const uint8_t MAX_PLD = 250;

        UsartProtocol();

        // 喂一个字节
        void feed(uint8_t b);

        // 收到完整且校验正确帧？
        bool available() const { return _ready; }

        // 取解析结果
        uint8_t getTo()      const { return _to; }
        uint8_t getLen()     const { return _len; }
        const uint8_t* getPayload() const { return _pld; }

        // 释放/复用
        void reset();

    private:
        enum State { ST_MAGIC1, ST_MAGIC2, ST_TO, ST_LEN, ST_PAYLOAD, ST_CS };
        State _state;                   // 单个数据包传送完成标志位
        uint8_t _to;
        uint8_t _len;
        uint8_t _pld[MAX_PLD];
        uint8_t _idx;
        uint8_t _calcCs;               // 校验和（字节全部相加后取反）
        bool _ready;

        void clear();

    public:
        static uint8_t pack(const uint8_t *pld, uint8_t len,
            uint8_t *outBuf, uint8_t buf_len = 0);

};
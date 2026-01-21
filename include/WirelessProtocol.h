#pragma once

#include <stdint.h>

class WirelessProtocol {
  public:
    typedef void (*OnPacket)(uint8_t seq, uint8_t flag,
                             const uint8_t *pld, uint16_t len);
    void setCallback(OnPacket cb) { _cb = cb; }
    void feed(uint8_t b);               // 喂字节
    uint8_t getSt()const;
  private:
    enum ST { ST_HEAD, ST_LEN_H, ST_LEN_L, ST_SEQ, ST_FLAG, ST_PAYLOAD, ST_CRC_H, ST_CRC_L, ST_TAIL };
    ST _st = ST_HEAD;
    uint16_t _len, _got;
    uint8_t  _seq, _flag;
    uint8_t  _buf[260];
    uint16_t _crcRx;
    uint16_t _crc;
    bool     _esc = false;
    OnPacket _cb = nullptr;

    void crcUpdate(uint8_t b);

  public:
    static uint16_t pack(uint8_t seq, uint8_t flag,
                  const uint8_t *pld, uint16_t len,
                  uint8_t *outBuf, uint16_t buf_len = 0);
  private:
    static void stuffByte(uint8_t b, uint8_t *&dst);
    static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len);
};


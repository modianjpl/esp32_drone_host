#include "WirelessProtocol.h"

uint8_t WirelessProtocol::getSt()const{return _st;}

void WirelessProtocol::feed(uint8_t b) {
  if (b == 0x7E) {              // 帧头→复位
    _st = ST_LEN_H;
    _esc = false;
    _got = 0;
    _crc = 0xFFFF;
    return;
  }
  if (b == 0x7F) {              // 帧尾→校验
    if (_st == ST_TAIL && _crc == _crcRx && _cb)
      _cb(_seq, _flag, _buf, _len);
    _st = ST_HEAD;              // 自动回到等待新帧
    return;
  }
  if (_esc) {                   // 处理转义
    b = (b == 0x5E) ? 0x7E : (b == 0x5D) ? 0x7D : (b == 0x5F) ? 0x7F : b;
    _esc = false;
  } else if (b == 0x7D) {       // 转义标志
    _esc = true;
    return;
  }

  switch (_st) {
    case ST_LEN_H:
      _len = (b & 0x3F) << 8; _st = ST_LEN_L; crcUpdate(b); break;
    case ST_LEN_L:
      _len |= b; crcUpdate(b);
      if (_len > sizeof(_buf)) { _st = ST_HEAD; return; } // 超长丢弃
      _st = ST_SEQ; break;
    case ST_SEQ:
      _seq = b; _st = ST_FLAG; crcUpdate(b); break;
    case ST_FLAG:
      _flag = b; _got = 0; crcUpdate(b); _st = (_len ? ST_PAYLOAD : ST_CRC_H); break;
    case ST_PAYLOAD:
      crcUpdate(b);
      if (_got < _len) { _buf[_got++] = b; }
      if (_got == _len) _st = ST_CRC_H; break;
    case ST_CRC_H:
      _crcRx = b << 8; _st = ST_CRC_L; break;
    case ST_CRC_L:
      _crcRx |= b; _st = ST_TAIL; break;
    default: break;
  }
}

inline void WirelessProtocol::crcUpdate(uint8_t b) {
  _crc ^= b << 8;
  for (uint8_t i = 0; i < 8; i++)
    _crc = (_crc & 0x8000) ? (_crc << 1) ^ 0x1021 : (_crc << 1);
}


uint16_t WirelessProtocol::pack(uint8_t seq, uint8_t flag,
                             const uint8_t *pld, uint16_t len,
                             uint8_t *out, uint16_t buf_len) {
  uint8_t *p = out;
  *p++ = 0x7E;                       // 帧头

  *p++ = (len >> 8) & 0x3F;          // LEN high 14bit
  *p++ = len & 0xFF;                 // LEN low
  *p++ = seq;
  *p++ = flag;                       // 独立 flag 字节

  /* stuff payload */
  for (uint16_t i = 0; i < len; i++) {
    if(buf_len && uint16_t(p-out)+5 > buf_len) return 0;
    stuffByte(pld[i], p);
  }

  /* CRC16-CCITT：从 LEN 第一字节到 stuff 后的最后一个 payload 字节 */
  uint16_t crc = crc16_ccitt(out + 1, p - (out + 1));
  stuffByte(crc >> 8, p);
  stuffByte(crc & 0xFF, p);

  *p++ = 0x7F;                       // 帧尾
  return p - out;                    // 总长度
}

inline void WirelessProtocol::stuffByte(uint8_t b, uint8_t *&dst) {
  switch (b) {
    case 0x7E: *dst++ = 0x7D; *dst++ = 0x5E; break;
    case 0x7D: *dst++ = 0x7D; *dst++ = 0x5D; break;
    case 0x7F: *dst++ = 0x7D; *dst++ = 0x5F; break;
    default:   *dst++ = b; break;
  }
}

/* CRC16-CCITT */
uint16_t WirelessProtocol::crc16_ccitt(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= (*data++) << 8;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}
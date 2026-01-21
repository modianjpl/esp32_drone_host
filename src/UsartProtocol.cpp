#include "UsartProtocol.h"

UsartProtocol::UsartProtocol() {
  clear();
}

void UsartProtocol::clear() {
    _state = ST_MAGIC1;

    _ready = false;
    _idx = 0;
    _calcCs = 0;
}

void UsartProtocol::reset() {
  clear();
}

void UsartProtocol::feed(uint8_t b) {
  if (_ready) return;          // 上层未 reset 前忽略新数据

  switch (_state) {
    case ST_MAGIC1:
      if (b == MAGIC1) {
        _calcCs = b;
        _state = ST_MAGIC2;
      }
      break;

    case ST_MAGIC2:
      if (b == MAGIC2) {
        _calcCs += b;
        _state = ST_TO;
      } else {
        _state = (b == MAGIC1) ? ST_MAGIC2 : ST_MAGIC1;
      }
      break;

    case ST_TO:
      _to = b;
      _calcCs += b;
      _state = ST_LEN;
      break;

    case ST_LEN:
      _len = b;
      _calcCs += b;
      if (_len == 0 || _len > MAX_PLD) {
        clear();               // 非法长度
      } else {
        _idx = 0;
        _state = ST_PAYLOAD;
      }
      break;

    case ST_PAYLOAD:
      _pld[_idx++] = b;
      _calcCs += b;
      if (_idx == _len) _state = ST_CS;
      break;

    case ST_CS:
      _calcCs = ~_calcCs;
      if (b == _calcCs)
        _ready = true;         // 成功
      else
        clear();               // 校验失败
      break;
  }
}
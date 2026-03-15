#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // Ethereum-compatible Keccak-256
  // output must be 32 bytes
  void keccak256(
      const uint8_t *input,
      size_t input_len,
      uint8_t *output);

#ifdef __cplusplus
}
#endif

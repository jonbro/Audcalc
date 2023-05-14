#ifndef _I2C_DMA_H
#define _I2C_DMA_H

#include "hardware/i2c.h"

// Explanation of symbols used in function documentation below.
// --------------+------------------------------------------------------------
// S             | Start condition.
// Sr            | Repeated start condition, used to switch from write to read
//               | mode.
// P             | Stop condition.
// Rd/Wr (1 bit) | Read/Write bit. Rd equals 1, Wr equals 0.
// A, NA (1 bit) | Acknowledge (ACK) and Not Acknowledge (NACK) bit
// addr (7 bits) | 7 bit I2C address.
// reg (8 bits)  | Register byte, a data byte which typically selects a
//               | register on the device.
// [..]          | Data sent by I2C device, as opposed to data sent by the
//               | host adapter.
// --------------+------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// An i2c_dma_t stores all the data required by the i2c_dma_* functions
// for driving I2C devices connected to I2C peripherals I2C0 and I2C1. Call
// i2c_dma_init to get a pointer to an i2c_dma_t for I2C0 or I2C1.
typedef struct i2c_dma_s i2c_dma_t;

// Initializes an I2C peripheral, its SDA pin, its SCL pin, its baudrate,
// enables the peripheral, and prepares it for DMA usage. i2c_dma_init must be
// called before other functions. Copies a pointer to an i2c_dma_t to
// *pi2c_dma. This i2c_dma_t pointer is the pointer passed as the first
// parameter to all other i2c_dma_* functions.
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_GENERIC
//     Error creating semaphore
//     Error creating mutex
//     Error attempting to take a semaphore
int i2c_dma_init(
  i2c_dma_t **pi2c_dma, // A pointer to an i2c_dma_t pointer
  i2c_inst_t *i2c,      // Either i2c0 or i2c1
  uint baudrate,        // Baudrate in hertz
  uint sda_gpio,        // GPIO number for SDA
  uint scl_gpio         // GPIO number for SCL
);

// Writes a block of bytes and/or reads a block of bytes in a single I2C
// transaction.
//
// I2C Transactions:
//
// Write a block of bytes:
// S addr Wr [A] wbuf(0) [A] wbuf(1) [A] ... [A] wbuf(wbuf_len-1) [A] P
//
// Read a block of bytes:
// S addr Rd [A] [rbuf(0)] A [rbuf(1)] A ... A [rbuf(rbuf_len-1)] NA P
//
// Write a block of bytes and read a block of bytes:
// S addr Wr [A] wbuf(0) [A] wbuf(1) [A] ... [A] wbuf(wbuf_len-1) [A]
//   Sr addr Rd [A] [rbuf(0)] A [rbuf(1)] A ... A [rbuf(rbuf_len-1)] NA P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
int i2c_dma_write_read(
  i2c_dma_t *i2c_dma,  // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,        // 7 bit I2C address
  const uint8_t *wbuf, // Pointer to block of bytes to write or NULL
  size_t wbuf_len,     // Length of block of bytes to write or 0
  uint8_t *rbuf,       // Pointer to block of bytes for data read or NULL
  size_t rbuf_len      // Number of bytes of data to read or 0
);

// Writes a block of bytes.
//
// I2C Transaction:
// S addr Wr [A] wbuf(0) [A] wbuf(1) [A] ... [A] wbuf(wbuf_len-1) [A] P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_write(
  i2c_dma_t *i2c_dma,  // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,        // 7 bit I2C address
  const uint8_t *wbuf, // Pointer to block of bytes to write or NULL
  size_t wbuf_len      // Length of block of bytes to write or 0
) {
  return i2c_dma_write_read(i2c_dma, addr, wbuf, wbuf_len, NULL, 0);
}

// Reads a block of bytes.
//
// I2C Transaction:
// S addr Rd [A] [rbuf(0)] A [rbuf(1)] A ... A [rbuf(rbuf_len-1)] NA P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_read(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t *rbuf,      // Pointer to block of bytes for data read or NULL
  size_t rbuf_len     // Number of bytes of data to read or 0
) {
  return i2c_dma_write_read(i2c_dma, addr, NULL, 0, rbuf, rbuf_len);
}

// Writes a byte to a register.
// 
// I2C Transaction:
// S addr Wr [A] reg [A] byte [A] P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_write_byte(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t reg,        // Number of the register to write to
  uint8_t byte        // Byte to write
) {
  const uint8_t wbuf[2] = {reg, byte};
  return i2c_dma_write_read(i2c_dma, addr, wbuf, 2, NULL, 0);
}

// Reads a byte from a register.
// 
// I2C Transaction:
// S addr Wr [A] reg [A] Sr addr Rd [A] [byte] NA P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_read_byte(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t reg,        // Number of the register to read from
  uint8_t *byte       // Pointer to the byte for the data read
) {
  return i2c_dma_write_read(i2c_dma, addr, &reg, 1, byte, 1);
}

// Writes a 16-bit word to a register. The least significant byte is sent
// over the wire first.
//
// I2C Transaction:
// S addr Wr [A] reg [A] word lsb [A] word msb [A] P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_write_word(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t reg,        // Number of the register to write to
  uint16_t word       // 16-bit word to write
) {
  const uint8_t wbuf[3] = {reg, word & 0xff, word >> 8};
  return i2c_dma_write_read(i2c_dma, addr, wbuf, 3, NULL, 0);
}

// Reads a 16-bit word from a register. The least significant byte is received
// over the wire first.
//
// I2C Transaction:
// S addr Wr [A] reg [A] Sr addr Rd [A] [word lsb] A [word msb] NA P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_read_word(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t reg,        // Number of the register to read from
  uint16_t *word      // Pointer to the 16-bit word for the data read
) {
  return i2c_dma_write_read(i2c_dma, addr, &reg, 1, (uint8_t *) word, 2);
}

// Writes a 16-bit word to a register. The most significant byte is sent
// over the wire first.
//
// I2C Transaction:
// S addr Wr [A] reg [A] word msb [A] word lsb [A] P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_write_word_swapped(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t reg,        // Number of the register to write to
  uint16_t word       // 16-bit word to write
) {
  const uint8_t wbuf[3] = {reg, word >> 8, word & 0xff};
  return i2c_dma_write_read(i2c_dma, addr, wbuf, 3, NULL, 0);
}

// Reads a 16-bit word from a register. The most significant byte is received
// over the wire first.
//
// I2C Transaction:
// S addr Wr [A] reg [A] Sr addr Rd [A] [word msb] A [word lsb] NA P
//
// Returns
//   PICO_OK
//     Function completed successfully
//   PICO_ERROR_INVALID_ARG
//     Invalid argument passed to function
//   PICO_ERROR_TIMEOUT
//     Timeout waiting to take a mutex
//     Timeout waiting for I2C transaction to complete
//   PICO_ERROR_IO
//     I2C transaction aborted by I2C peripheral
//     No stop condition for transaction detected by I2C peripheral
//   PICO_ERROR_GENERIC
//     Error attempting to give a mutex
//     Error attemptimg to claim a DMA channel
static inline int i2c_dma_read_word_swapped(
  i2c_dma_t *i2c_dma, // i2c_dma_t pointer for I2C0 or I2C1
  uint8_t addr,       // 7 bit I2C address
  uint8_t reg,        // Number of the register to read from
  uint16_t *word      // Pointer to the 16-bit word for the data read
) {
  int rc = i2c_dma_write_read(i2c_dma, addr, &reg, 1, (uint8_t *) word, 2);
  *word = *word << 8 | *word >> 8;
  return rc;
}

#ifdef __cplusplus
}
#endif

#endif


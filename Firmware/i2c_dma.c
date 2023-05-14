#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/sync.h"
#include "i2c_dma.h"

#define I2C_MAX_TRANSFER_SIZE     1056
// A transfer timeout of 1000ms will allow a 10000 bit transfer to complete
// successfully without timeouts at baudrates as low as 10000 baud.
#define I2C_TRANSFER_TIMEOUT_MS   1000
#define I2C_TAKE_MUTEX_TIMEOUT_MS 10000

typedef struct i2c_dma_s {
  i2c_inst_t *i2c;

  uint irq_num;
  irq_handler_t irq_handler;

  uint baudrate;
  uint sda_gpio;
  uint scl_gpio;

  semaphore_t semaphore;
  mutex_t mutex;

  volatile bool stop_detected;
  volatile bool abort_detected;

  uint16_t data_cmds[I2C_MAX_TRANSFER_SIZE];
} i2c_dma_t;

static i2c_dma_t i2c_dma_list[2];

static void i2c_dma_irq_handler(i2c_dma_t *i2c_dma) {
  const uint32_t status = i2c_get_hw(i2c_dma->i2c)->intr_stat;

  // If there is an abort, normally there is an abort interrupt followed by a
  // stop interrupt. On the rare occasion, for example, if the first I2C
  // transaction after reset is aborted, the abort and stop interrupt flags
  // appear to be set at the same instant or almost the same instant.
  if (status & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
    // Transfer aborted.
    i2c_get_hw(i2c_dma->i2c)->clr_tx_abrt;
    i2c_dma->abort_detected = true;
  }

  if (status & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
    // Transfer complete.
    i2c_get_hw(i2c_dma->i2c)->clr_stop_det;
    i2c_dma->stop_detected = true;

    sem_release(&i2c_dma->semaphore);
  }
}

static void i2c0_dma_irq_handler(void) {
  i2c_dma_irq_handler(&i2c_dma_list[0]);
}

static void i2c1_dma_irq_handler(void) {
  i2c_dma_irq_handler(&i2c_dma_list[1]);
}

static void i2c_dma_set_target_addr(i2c_inst_t *i2c, uint8_t addr) {
  i2c_get_hw(i2c)->enable = 0;
  i2c_get_hw(i2c)->tar = addr;
  i2c_get_hw(i2c)->enable = 1;
}

static void i2c_dma_tx_channel_configure(
  i2c_inst_t *i2c, int tx_channel, const uint16_t *tx_buf, size_t len
) {
  dma_channel_config tx_config = dma_channel_get_default_config(tx_channel);
  channel_config_set_read_increment(&tx_config, true);
  channel_config_set_write_increment(&tx_config, false);
  channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_16);
  channel_config_set_dreq(&tx_config, i2c_get_dreq(i2c, true));
  dma_channel_configure(
    tx_channel, &tx_config, &i2c_get_hw(i2c)->data_cmd, tx_buf, len, true
  );
}

static void i2c_dma_rx_channel_configure(
  i2c_inst_t *i2c, int rx_channel, uint8_t *rx_buf, size_t len
) {
  dma_channel_config rx_config = dma_channel_get_default_config(rx_channel);
  channel_config_set_read_increment(&rx_config, false);
  channel_config_set_write_increment(&rx_config, true);
  channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
  channel_config_set_dreq(&rx_config, i2c_get_dreq(i2c, false));
  dma_channel_configure(
    rx_channel, &rx_config, rx_buf, &i2c_get_hw(i2c)->data_cmd, len, true
  );
}

static void i2c_dma_pin_open_drain(uint gpio) {
  gpio_set_function(gpio, GPIO_FUNC_SIO);
  gpio_set_dir(gpio, GPIO_IN);
  gpio_put(gpio, 0);
}

static void i2c_dma_pin_od_low(uint gpio) {
  gpio_set_dir(gpio, GPIO_OUT);
}

static void i2c_dma_pin_od_high(uint gpio) {
  gpio_set_dir(gpio, GPIO_IN);
}

static void i2c_dma_unblock(i2c_dma_t *i2c_dma) {
  i2c_dma_pin_open_drain(i2c_dma->sda_gpio);
  i2c_dma_pin_open_drain(i2c_dma->scl_gpio);

  bool sda_high;
  int max_tries = 9;

  // Make sure the frequency of the bit-bannged I2C clock is at most 100KHz.
  const uint32_t f_clk_sys_khz =
    frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
  const uint32_t i2c_delay = f_clk_sys_khz / 100 / 2;

  do {
    i2c_dma_pin_od_low(i2c_dma->scl_gpio);
    for (int i = i2c_delay; i > 0; i -= 1) {
      __asm__("nop");
    }

    i2c_dma_pin_od_high(i2c_dma->scl_gpio);
    for (int i = i2c_delay; i > 0; i -= 1) {
      __asm__("nop");
    }

    max_tries -= 1;
    sda_high = gpio_get(i2c_dma->sda_gpio);
  } while (!sda_high && max_tries > 0);
}

static bool i2c_dma_is_blocked(i2c_dma_t *i2c_dma) {
  i2c_dma_pin_open_drain(i2c_dma->sda_gpio);
  i2c_dma_pin_open_drain(i2c_dma->scl_gpio);

  const bool sda_high = gpio_get(i2c_dma->sda_gpio);
  const bool scl_high = gpio_get(i2c_dma->scl_gpio);

  return !sda_high || !scl_high;
}

static int i2c_dma_init_intern(i2c_dma_t *i2c_dma) {
  irq_set_enabled(i2c_dma->irq_num, false);

  i2c_dma->stop_detected = false;
  i2c_dma->abort_detected = false;
  
  if (sem_available(&i2c_dma->semaphore) != 0) {
    if (!sem_try_acquire(&i2c_dma->semaphore)) {
      return PICO_ERROR_GENERIC;
    }
  }

  // Don't do anything with i2c_dma->mutex here, let i2c_dma_write_read take
  // care of it. Also, directly after creation with xSemaphoreCreateMutex a
  // mutex can be successfully taken.

  // Attempt to unblock a blocked bus. If it can't be unblocked, continue
  // anyway.
  if (i2c_dma_is_blocked(i2c_dma)) {
    i2c_dma_unblock(i2c_dma);
  }

  i2c_init(i2c_dma->i2c, i2c_dma->baudrate);

  gpio_set_function(i2c_dma->sda_gpio, GPIO_FUNC_I2C);
  gpio_set_function(i2c_dma->scl_gpio, GPIO_FUNC_I2C);
  gpio_pull_up(i2c_dma->sda_gpio);
  gpio_pull_up(i2c_dma->scl_gpio);

  i2c_get_hw(i2c_dma->i2c)->intr_mask =
    I2C_IC_INTR_MASK_M_STOP_DET_BITS |
    I2C_IC_INTR_MASK_M_TX_ABRT_BITS;

  irq_add_shared_handler(i2c_dma->irq_num, i2c_dma->irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY-3);
  irq_set_enabled(i2c_dma->irq_num, true);

  return PICO_OK;
}

static int i2c_dma_reinit(i2c_dma_t *i2c_dma) {
  return i2c_dma_init_intern(i2c_dma);
}

int i2c_dma_init(
  i2c_dma_t **pi2c_dma,
  i2c_inst_t *i2c,
  uint baudrate,
  uint sda_gpio,
  uint scl_gpio
) {
  i2c_dma_t *i2c_dma;

  if (i2c == i2c0) {
    i2c_dma = &i2c_dma_list[0];
    i2c_dma->i2c = i2c0;
    i2c_dma->irq_num = I2C0_IRQ;
    i2c_dma->irq_handler = i2c0_dma_irq_handler;
  } else {
    i2c_dma = &i2c_dma_list[1];
    i2c_dma->i2c = i2c1;
    i2c_dma->irq_num = I2C1_IRQ;
    i2c_dma->irq_handler = i2c1_dma_irq_handler;
  }

  *pi2c_dma = i2c_dma;

  i2c_dma->baudrate = baudrate;
  i2c_dma->sda_gpio = sda_gpio;
  i2c_dma->scl_gpio = scl_gpio;
  sem_init(&i2c_dma->semaphore, 1, 1);
  mutex_init(&i2c_dma->mutex);

  return i2c_dma_init_intern(i2c_dma);
}

static int i2c_dma_write_read_internal(
  i2c_dma_t *i2c_dma,
  uint8_t addr,
  const uint8_t *wbuf,
  size_t wbuf_len,
  uint8_t *rbuf,
  size_t rbuf_len
) {
  if (
    (wbuf_len > 0 && wbuf == NULL) ||
    (rbuf_len > 0 && rbuf == NULL) ||
    (wbuf_len == 0 && rbuf_len == 0) ||
    (wbuf_len + rbuf_len > I2C_MAX_TRANSFER_SIZE)
  ) {
    return PICO_ERROR_INVALID_ARG;
  }

  const bool writing = (wbuf_len > 0);
  const bool reading = (rbuf_len > 0);

  int tx_chan = 0; // Channel for writing data_cmds to I2C peripheral.
  int rx_chan = 0; // Channel for reading data from I2C peripheral, if needed.

  if (writing) {
    // Setup commands for each byte to write to the I2C bus.
    for (size_t i = 0; i != wbuf_len; ++i) {
      i2c_dma->data_cmds[i] = wbuf[i];
    }

    // The first byte written must be preceded by a start.
    i2c_dma->data_cmds[0] |= I2C_IC_DATA_CMD_RESTART_BITS;
  }

  // DMA tx_chan is needed for both writing and reading.
  tx_chan = dma_claim_unused_channel(false);
  if (tx_chan == -1) {
    return PICO_ERROR_GENERIC;
  }

  if (reading) {
    // Setup commands for each byte to read from the I2C bus.
    for (size_t i = 0; i != rbuf_len; ++i) {
      i2c_dma->data_cmds[wbuf_len + i] = I2C_IC_DATA_CMD_CMD_BITS;
    }

    // The first byte read must be preceded by a start/restart.
    i2c_dma->data_cmds[wbuf_len] |= I2C_IC_DATA_CMD_RESTART_BITS;

    // DMA rx_chan is only needed for reading.
    rx_chan = dma_claim_unused_channel(false);
    if (rx_chan == -1) {
      dma_channel_unclaim(tx_chan);
      return PICO_ERROR_GENERIC;
    }
  }

  // The last byte transfered must be followed by a stop.
  i2c_dma->data_cmds[wbuf_len + rbuf_len - 1] |= I2C_IC_DATA_CMD_STOP_BITS;

  // Tell the I2C peripheral the adderss of the device for the transfer.
  i2c_dma_set_target_addr(i2c_dma->i2c, addr);

  i2c_dma->stop_detected = false;
  i2c_dma->abort_detected = false;

  // Start the I2C transfer on required DMA channels.
  if (reading) {
    i2c_dma_rx_channel_configure(i2c_dma->i2c, rx_chan, rbuf, rbuf_len);
  }
  i2c_dma_tx_channel_configure(
    i2c_dma->i2c, tx_chan, i2c_dma->data_cmds, wbuf_len + rbuf_len
  );

  // The I2C transfer via DMA has been started. Wait for it to complete. Under
  // normal circumstances, the transfer is complete when a stop is detected on
  // the bus. If the hardware detects problems during the transfer, there will
  // normally be an abort followed by a stop. Scenarios where a stop and/or
  // abort are not detected are also possible, for these scenarios a timeout
  // is needed. As an example, no stop will be detected if SDA gets stuck low.
  const bool timeout = !sem_acquire_timeout_ms(&i2c_dma->semaphore, I2C_TAKE_MUTEX_TIMEOUT_MS);

  // If there were problems, abort the DMA.
  if (timeout || i2c_dma->abort_detected || !i2c_dma->stop_detected) {
    dma_channel_abort(tx_chan);
    if (reading) {
      dma_channel_abort(rx_chan);
    }
  }

  // Free the DMA channels.
  dma_channel_unclaim(tx_chan);
  if (reading) {
    dma_channel_unclaim(rx_chan);
  }

  int rc = PICO_OK;

  if (timeout) {
    rc = PICO_ERROR_TIMEOUT;
  } else if (i2c_dma->abort_detected || !i2c_dma->stop_detected) {
    rc = PICO_ERROR_IO;
  }

  // Attempt to recover from errors.
  if (rc != PICO_OK) {
    i2c_dma_reinit(i2c_dma);
  }

  return rc;
}

int i2c_dma_write_read(
  i2c_dma_t *i2c_dma,
  uint8_t addr,
  const uint8_t *wbuf,
  size_t wbuf_len,
  uint8_t *rbuf,
  size_t rbuf_len
) {
  if (!mutex_enter_timeout_ms(&i2c_dma->mutex, I2C_TAKE_MUTEX_TIMEOUT_MS)) {
    return PICO_ERROR_TIMEOUT;
  }

  const int rc = i2c_dma_write_read_internal(
    i2c_dma, addr, wbuf, wbuf_len, rbuf, rbuf_len
  );

  mutex_exit(&i2c_dma->mutex);

  return rc;
}


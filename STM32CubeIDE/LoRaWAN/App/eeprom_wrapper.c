/*
 * eeprom_wrapper.c
 *
 *  Created on: 14. 10. 2026
 *      Author: pesek
 */

#include "main.h"             /* DEBUG_PRINTF */
#include "eeprom_wrapper.h"

_Static_assert((EE_VA_USER_BASE + EE_VA_USER_WORDS - 1U) <= NB_OF_VARIABLES, "VA must be 1..NB_OF_VARIABLES");

/* Writes one variable only when the stored value differs.
   EE_CleanUp() erases flash pages (CPU stalls): do not call during a LoRaWAN TX/RX window. */
EE_Status ee_write(uint16_t va, uint32_t data)
{
  uint32_t stored;
  EE_Status status;

  if ((EE_ReadVariable32bits(va, &stored) == EE_OK) && (stored == data))
  {
    return EE_OK;
  }

  HAL_FLASH_Unlock();
  status = EE_WriteVariable32bits(va, data);
  if (status == EE_CLEANUP_REQUIRED)
  {
    DEBUG_PRINTF("EEPROM cleanup\r\n");
    status = EE_CleanUp();
  }
  HAL_FLASH_Lock();

  if (status != EE_OK)
  {
    DEBUG_PRINTF("EEPROM write of VA %u failed (status %u)\r\n", (unsigned int)va, (unsigned int)status);
  }
  return status;
}

EE_Status eeprom_vars_init(void)
{
  uint32_t v;
  EepromVars_t *ee = &eeprom_vars;

  /* Nothing is written here: the EEPROM is written only by AT+PROFILE and the AT setters */
  dev_state.init_phase = INIT_PHASE_NONE;
  dev_state.profile_id = PROFILE_NONE;
  if ((EE_ReadVariable32bits(EE_VA_PROFILE_ID, &v) == EE_OK) && (v < PROFILE_COUNT))
  {
    dev_state.profile_id = (uint8_t)v;
  }
  /* No valid profile ID (EEPROM empty, older firmware) = not configured, whatever init_phase says */
  if ((dev_state.profile_id != PROFILE_NONE) && (EE_ReadVariable32bits(EE_VA_INIT_PHASE, &v) == EE_OK))
  {
    dev_state.init_phase = (uint8_t)v;
  }

  /* RAM start values: EEPROM part of the stored profile, or of the default profile (not written to EEPROM) */
  eeprom_vars = profiles[(dev_state.profile_id != PROFILE_NONE) ? dev_state.profile_id : PROFILE_DEFAULT].ee;

  DEBUG_PRINTF("Reading EEPROM variables, init_phase %u\r\n", (unsigned int)dev_state.init_phase);
  /* A variable that is not found keeps the profile value. Also read when not configured:
     variables set by AT before AT+PROFILE */
  if (EE_ReadVariable32bits(EE_VA_UART_BR, &v) == EE_OK)      { ee->uart_baud       = v; }
  if (EE_ReadVariable32bits(EE_VA_UART_TIMEOUT, &v) == EE_OK) { ee->uart_timeout_ms = (uint16_t)v; }
  if (EE_ReadVariable32bits(EE_VA_DR, &v) == EE_OK)           { ee->dr              = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_DC, &v) == EE_OK)           { ee->dc              = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_NET, &v) == EE_OK)          { ee->net             = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_LDRO, &v) == EE_OK)         { ee->ldro            = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_THLD, &v) == EE_OK)         { ee->thld_dbm        = (int16_t)(uint16_t)v; }
  if (EE_ReadVariable32bits(EE_VA_DUMRX, &v) == EE_OK)        { ee->dumrx           = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_PORT, &v) == EE_OK)         { ee->port            = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_RETRY, &v) == EE_OK)        { ee->retry           = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_WDT, &v) == EE_OK)          { ee->wdt             = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_CT, &v) == EE_OK)           { ee->ct              = (uint8_t)v; }
  if (EE_ReadVariable32bits(EE_VA_LOWPOWER, &v) == EE_OK)     { ee->lowpower_auto   = (uint8_t)v; }
  return EE_OK;
}

EE_Status eeprom_profile_write(uint8_t id)
{
  uint32_t status;             /* OR of all results: non-zero = at least one write failed */
  const EepromVars_t *ee;

  if (id >= PROFILE_COUNT)
  {
    return EE_WRITE_ERROR;
  }
  ee = &profiles[id].ee;

  /* Not configured first, INIT_PHASE_FIRST last: an interrupted write leaves the module
     not configured, the host repeats AT+PROFILE */
  status = eeprom_set_init_phase(INIT_PHASE_NONE);
  eeprom_vars = profiles[id].ee;

  status |= ee_write(EE_VA_UART_BR,      ee->uart_baud);
  status |= ee_write(EE_VA_UART_TIMEOUT, ee->uart_timeout_ms);
  status |= ee_write(EE_VA_DR,           ee->dr);
  status |= ee_write(EE_VA_DC,           ee->dc);
  status |= ee_write(EE_VA_NET,          ee->net);
  status |= ee_write(EE_VA_LDRO,         ee->ldro);
  status |= ee_write(EE_VA_THLD,         (uint16_t)ee->thld_dbm);
  status |= ee_write(EE_VA_DUMRX,        ee->dumrx);
  status |= ee_write(EE_VA_PORT,         ee->port);
  status |= ee_write(EE_VA_RETRY,        ee->retry);
  status |= ee_write(EE_VA_WDT,          ee->wdt);
  status |= ee_write(EE_VA_CT,           ee->ct);
  status |= ee_write(EE_VA_LOWPOWER,     ee->lowpower_auto);
  status |= eeprom_set_profile_id(id);

  if (status != EE_OK)
  {
    DEBUG_PRINTF("Profile %u not written to EEPROM, init_phase not set\r\n", (unsigned int)id);
    return EE_WRITE_ERROR;
  }
  return eeprom_set_init_phase(INIT_PHASE_FIRST);
}

EE_Status eeprom_set_uart_baud(uint32_t value)
{
  eeprom_vars.uart_baud = value;
  return ee_write(EE_VA_UART_BR, value);
}

EE_Status eeprom_set_uart_timeout(uint16_t value)
{
  eeprom_vars.uart_timeout_ms = value;
  return ee_write(EE_VA_UART_TIMEOUT, value);
}

EE_Status eeprom_set_dr(uint8_t value)
{
  eeprom_vars.dr = value;
  return ee_write(EE_VA_DR, value);
}

EE_Status eeprom_set_dc(uint8_t value)
{
  eeprom_vars.dc = value;
  return ee_write(EE_VA_DC, value);
}

EE_Status eeprom_set_net(uint8_t value)
{
  eeprom_vars.net = value;
  return ee_write(EE_VA_NET, value);
}

EE_Status eeprom_set_ldro(uint8_t value)
{
  eeprom_vars.ldro = value;
  return ee_write(EE_VA_LDRO, value);
}

EE_Status eeprom_set_thld(int16_t value)
{
  eeprom_vars.thld_dbm = value;
  return ee_write(EE_VA_THLD, (uint16_t)value);   /* -65 is stored as 0x0000FFBF */
}

EE_Status eeprom_set_dumrx(uint8_t value)
{
  eeprom_vars.dumrx = value;
  return ee_write(EE_VA_DUMRX, value);
}

EE_Status eeprom_set_port(uint8_t value)
{
  eeprom_vars.port = value;
  return ee_write(EE_VA_PORT, value);
}

EE_Status eeprom_set_retry(uint8_t value)
{
  eeprom_vars.retry = value;
  return ee_write(EE_VA_RETRY, value);
}

EE_Status eeprom_set_wdt(uint8_t value)
{
  eeprom_vars.wdt = value;
  return ee_write(EE_VA_WDT, value);
}

EE_Status eeprom_set_ct(uint8_t value)
{
  eeprom_vars.ct = value;
  return ee_write(EE_VA_CT, value);
}

EE_Status eeprom_set_lowpower(uint8_t value)
{
  eeprom_vars.lowpower_auto = value;
  return ee_write(EE_VA_LOWPOWER, value);
}

EE_Status eeprom_set_profile_id(uint8_t value)
{
  dev_state.profile_id = value;
  return ee_write(EE_VA_PROFILE_ID, value);
}

EE_Status eeprom_set_init_phase(uint8_t value)
{
  dev_state.init_phase = value;
  return ee_write(EE_VA_INIT_PHASE, value);
}

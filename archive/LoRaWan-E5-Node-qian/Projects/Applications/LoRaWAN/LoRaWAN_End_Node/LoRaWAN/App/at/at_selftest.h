/*
 * at_selftest.h - AT command self-test
 * Pure C99, no HAL dependencies
 */
#ifndef AT_SELFTEST_H
#define AT_SELFTEST_H

/*
 * Run self-tests for AT command layer
 * print: function to print test output (PASS/FAIL messages)
 * Returns: number of failures (0 = all pass)
 */
int at_selftest(void (*print)(const char *));

#endif /* AT_SELFTEST_H */

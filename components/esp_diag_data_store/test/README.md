# ESP Diagnostic Data Store Tests

This directory contains comprehensive tests for the ESP Diagnostic Data Store component, including unit tests and Python stress tests.

## Test Cases

### Unit Tests (test_data_store.c)

1. **Data Store Initialization Tests**
   - `data store init deinit`: Tests basic initialization and cleanup

2. **Data Write Tests**
   - `data store write`: Tests data writing functionality with various scenarios
   - Invalid argument handling
   - Memory limit testing
   - Critical data writing

3. **Data Read Tests**
   - `read critical data in b1`: Tests reading from bank 1
   - `read critical data in b2`: Tests reading from bank 2
   - `read stale critical data in b1 b2`: Tests reading stale data

4. **Reset and Recovery Tests**
   - `write critical data in b1 and reset`: Tests data persistence after reset
   - `write critical data in b2 and reset`: Tests bank 2 reset scenarios
   - `write critical data in b1 b2 and reset`: Tests dual bank scenarios

5. **RTC Store Tests**
   - `write critical data in rtc and reset`: Tests RTC storage functionality
   - `read critical data in rtc`: Tests RTC data reading

---

Note: these tests are run from [unit_test_app](../../../unit_test_app)

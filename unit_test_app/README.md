# ESP Insights Unit Test Application

This directory contains application for running the unit tests regarding the ESP Insights.

## Overview

This test application provides a framework for testing the esp-insights firmware agent functionality.

## Building and Running

To build and run the tests:

```bash
idf.py build
idf.py flash monitor
```

> **Note:** This unit test app is being used in the python tests and relies on [tests_sequence.json](tests_sequence.json) for it to run properly. Please consider updating the same if you need to modify the sequence.
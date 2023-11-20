# tcping

## Introduction

A fork of [Eli Fulkerson's tcping](https://www.elifulkerson.com/projects/tcping.php) with improvements.

## Changes

The code has been updated to be mostly in line with C++20 standards, but more work remains to be done. A leaking socket when using source addresses has been fixed. Functions with an unwieldy amount of parameters have been refactored to be more manageable. Headers have been replaced with modules and repetitive declarations have been consolidated. Also removed some random commented out code.

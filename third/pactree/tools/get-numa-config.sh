#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2019-2021 Virginia Tech
# SPDX-License-Identifier: Apache-2.0

python3 ./cpu-topology.py > ../include/numa-config.h

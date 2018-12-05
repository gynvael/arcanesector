#!/bin/bash
export ARCANE_HOST=127.0.0.1:5001
export ARCANE_CLIENT=`realpath ../client`
python main.py

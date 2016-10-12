#!/bin/bash

echo "Running Flask with app: $1"
export FLASK_DEBUG=1
export FLASK_APP=$1
python -m flask run

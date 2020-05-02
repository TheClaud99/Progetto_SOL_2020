#!/bin/bash
BDIR=./
SUPERMERCATO=./supermercato
K=4
P=1
W=4
C=2

cd $BDIR

echo "test"
$SUPERMERCATO 
PID=$!
echo $PID

# echo "terminazione"
# kill -SIGINT $PID
# sleep 0.5
# kill -SIGINT $PID



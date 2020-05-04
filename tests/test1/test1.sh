#!/bin/bash
BDIR=./
SUPERMERCATO=./supermercato
K=2
C=20
E=5
T=500
P=80
S=30
TP=10

cd $BDIR

echo "test"
$SUPERMERCATO $K $C $E $T $P $S $TP
PID=$!
echo $PID

# echo "terminazione"
# kill -SIGINT $PID
# sleep 0.5
# kill -SIGINT $PID

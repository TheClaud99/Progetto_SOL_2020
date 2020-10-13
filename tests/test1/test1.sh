#!/bin/bash
BDIR=./
SUPERMERCATO=./supermercato
DIRETTORE=./direttore
K=2
C=20
E=5
T=500
P=80
S=30
S1=2
S2=10
TP=10

cd $BDIR

echo "test"
$DIRETTORE $K $S1 $S2 &
$SUPERMERCATO $K $C $E $T $P $S $S1 $S2 $TP
PID=$!
echo $PID

# echo "terminazione"
# kill -SIGINT $PID
# sleep 0.5
# kill -SIGINT $PID

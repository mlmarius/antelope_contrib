echo on

% Construct a pointer to a new trace object:
tr=trnew

% Out of curiosity, check a couple things about it:
dbquery( tr,'dbDATABASE_NAME' )

dbquery( tr,'dbSCHEMA_NAME' )  

echo off

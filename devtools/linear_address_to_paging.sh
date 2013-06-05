#!/bin/bash

# 64 bits address.
# 0x7FFFFFFFFFFF


# 1111111111111111 100000000 000000000 000000000 000000000 000000000001
# 1111111111111111100000000000000000000000000000000000100000000001

function bin2hex()
{
    echo "obase=16; ibase=2; $1" | bc
}







# Cleanup
ADDR=`echo $1 | tr [a-z] [A-Z] | sed 's/0X//'`
ADDR="0000000000000000$ADDR"
#ADDR=`echo $ADDR | awk -F "" '{for(i=$(NF-15);i<=NF;++i)printf $i}'`

# To binary
BINADDR=`echo "obase=2; ibase=16; $ADDR" | bc | tr -d '\n'`


echo "Address: $ADDR"
echo "Binary:  $BINADDR"
echo ""

BINADDR="0000000000000000000000000000000000000000000000000000000000000000$BINADDR"


FIELDS=`echo $BINADDR | wc -c`
PHYSICAL_PAGE_OFFSET=`echo $BINADDR | sed -e 's/\(.\)/\1 /g' | cut -d ' ' -f$((FIELDS - 12))-$FIELDS | sed 's/ //g'`
PTE=`echo $BINADDR | sed -e 's/\(.\)/\1 /g' | cut -d ' ' -f$((FIELDS - 21))-$((FIELDS - 13)) | sed 's/ //g'`
PDE=`echo $BINADDR | sed -e 's/\(.\)/\1 /g' | cut -d ' ' -f$((FIELDS - 30))-$((FIELDS - 22)) | sed 's/ //g'`
PDPE=`echo $BINADDR | sed -e 's/\(.\)/\1 /g' | cut -d ' ' -f$((FIELDS - 39))-$((FIELDS - 31)) | sed 's/ //g'`
PML4E=`echo $BINADDR | sed -e 's/\(.\)/\1 /g' | cut -d ' ' -f$((FIELDS - 48))-$((FIELDS - 40)) | sed 's/ //g'`
SIGN_EXTENT=`echo $BINADDR | sed -e 's/\(.\)/\1 /g' | cut -d ' ' -f1-$((FIELDS - 49)) | sed 's/ //g'`



echo "SIGN_EXTENT: $(bin2hex $SIGN_EXTENT)"
echo "PML4E: $(bin2hex $PML4E)"
echo "PDPE: $(bin2hex $PDPE)"
echo "PDE: $(bin2hex $PDE)"
echo "PTE: $(bin2hex $PTE)"
echo "PHYSICAL_PAGE_OFFSET: $(bin2hex $PHYSICAL_PAGE_OFFSET)"

echo ""
echo "SIGN_EXTENT: $SIGN_EXTENT"
echo "PML4E: $PML4E"
echo "PDPE: $PDPE"
echo "PDE: $PDE"
echo "PTE: $PTE"
echo "PHYSICAL_PAGE_OFFSET: $PHYSICAL_PAGE_OFFSET"


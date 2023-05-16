#!/bin/bash

set -e

DIR=$(mktemp -d /tmp/dxr2png.XXXXXX)

# Uncompressing
for dxr in samples/*.dxr.gz; do
	gunzip -cd $dxr > $DIR/$(basename ${dxr%%.gz})
done

for dxr in $DIR/*.dxr; do

	if ! ./dxr2png $dxr > /dev/null 2>&1; then
		echo "$dxr : - : CRASH or ASSERTION !!!"
		continue
	fi

	MD5=$(md5sum ${dxr%%.dxr}.png | awk '{print $1}')

	dxr=$(basename ${dxr%%.dxr})
	case "$dxr,${MD5}" in
	white_pattern,b9fd430f6c638daf1727083f73b14eb1)
		;&
	blue_pattern,8f025e13d69eafd14088ec77673303bf)
		;&
	red_pattern,4633619ca306e4a0a879f5ec77d5b54c)
		;&
	green_pattern,18b65c55859649beb787dfb1e082a67d)
		echo "${dxr} : $MD5 : OK"
		;;
	*)
		echo "${dxr} : $MD5 : BAD or UNKNOWN checksum"
		;;
	esac
done | column -t -s':'

echo "-----------------------------"

for dxr in $DIR/{red,white}_pattern.dxr; do

	for plan in R; do
		if ! ./dxr2png $dxr $plan > /dev/null 2>&1; then
			echo "$dxr : $plan : - : CRASH or ASSERTION !!!"
			continue
		fi

		MD5=$(md5sum ${dxr%%.dxr}-${plan}.png | awk '{print $1}')

		DXR=$(basename ${dxr%%.dxr})

		case "$DXR,$plan,${MD5}" in
		red_pattern,R,b197911f581bc4817006357edd623156)
			;&
		white_pattern,R,b197911f581bc4817006357edd623156)
			echo "$DXR : $plan : $MD5 : OK"
			;;
		*)
			echo "$DXR : $plan : $MD5 : BAD checksum"
			;;
		esac
	done
done | column -t -s':'

# Comment to avoid loosing PNGs
rm -fr $DIR

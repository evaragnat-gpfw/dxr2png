#!/bin/bash

set -e

for dxr in samples/*.dxr.gz; do
        gunzip -cd $dxr > test.dxr
	dxr=$(basename ${dxr%%.gz})

        if ! dxr2png test.dxr > /dev/null 2>&1; then
		echo "$dxr : - : CRASH or ASSERTION !!!"
		continue
	fi

        MD5=$(md5sum test.png | awk '{print $1}')

        case "${dxr%%.dxr},${MD5}" in
	blue_pattern,779c538252301bfcd7ca05271e12695d)
		;&
	red_pattern,5baee5d3924895a1f0eafafa110fb07e)
		;&
	green_pattern,a943c8abc3157e885b569830b5ab2aac)
		echo "${dxr} : $MD5 : OK"
		;;
	*)
		echo "${dxr} : $MD5 : BAD checksum"
		;;
	esac
done | column -t -s':'

rm -f test.dxr test.png

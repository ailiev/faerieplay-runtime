# generate a write for each index
# and then some number of random read/write steps


function getdefine {
    file=$1
    var=$2
    
    sed -ne "s/#define $var \\(.*\\)$/\1/p" < $file
}

function rand() {
    [ -z "$binsize" ] && binsize=$(echo "$valsize / 1.333333" | bc)
    head -c $binsize /dev/urandom | \
	mimencode | \
	head -c $valsize
}


paramdefs='array-test-sizes.h'
len=$(getdefine $paramdefs 'ARR_ARRAYLEN')
valsize=$(getdefine $paramdefs 'ARR_OBJSIZE')
numsteps=$(getdefine $paramdefs 'ARR_NUMSTEPS')


out='array-test-cmds.txt'
for (( i=0 ; $i < $len; i++ )); do
    r=$(rand)
    cat <<EOF
write
$i
$(rand)
EOF
done >| $out


for (( i=0 ; $i < $numsteps - $len; i++ )); do
    idx=$(( $RANDOM % $len ))
    if (( $RANDOM % 2 == 0 )); then
	# write
	val=$(rand)
	cmd="write\n$idx\n$val"
    else
	cmd="read\n$idx"
    fi
    
    echo -e $cmd
    
done >> $out
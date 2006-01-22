# generate a write for each index

len=256
numsteps=515

for (( i=0 ; $i < $len; i++ )); do
    cat <<EOF
write
$i
$RANDOM
EOF
done


for (( i=0 ; $i < $numsteps; i++ )); do
    r=$RANDOM
    idx=$(( $RANDOM % 128 ))
    if (( $r % 2 == 0 )); then
	# write
	val=$RANDOM
	cmd="write\n$idx\n$val"
    else
	cmd="read\n$idx"
    fi
    
    echo -e $cmd
    
done

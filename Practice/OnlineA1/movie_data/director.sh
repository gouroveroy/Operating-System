last_lines() {
    tail --lines 2 < "$1"
}

for file in *.txt; do
    array=()
    lines=$(last_lines "$file")
    for line in $lines; do
        array+=("$line")
    done
    size="${#array[@]}"
    unset array[$(expr $size - 1)]
    # echo "${#array[@]}"
    director=
    for ele in "${array[@]}"; do
        director+="$ele "
    done
    folder="${director%" "}"
    # echo "$folder"
    mkdir -p "$folder"
    cp "$file" "$folder"
done
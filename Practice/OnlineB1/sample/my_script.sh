print_tree() {
    local dir="$1"
    local prefix="$2"
    local entries=("$dir"/*)
    local count=${#entries[@]}
    local index=0

    for entry in "${entries[@]}"; do
        [ -e "$entry" ] || continue
        index=$((index + 1))
        local base=$(basename "$entry")
        local connector="|--"
        [ $index -eq $count ] && connector="|--"

        # if [[ -f "$entry" && "$entry" == *.zip ]]; then
        #     unzip -q "$entry" -d "$dir" && rm "$entry"
        #     continue
        # fi

        echo "${prefix}${connector}$base"

        if [ -d "$entry" ]; then
            new_prefix="$prefix"
            [ $index -eq $count ] && new_prefix+="|   " || new_prefix+="│   "
            print_tree "$entry" "$new_prefix"
        fi
    done
}

echo "."
print_tree "." ""

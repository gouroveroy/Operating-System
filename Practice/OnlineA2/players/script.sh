findLine() {
    head -n 4 "$1"
}

for folder in */; do
    while IFS= read -r file; do
        # echo "$file"
        name=""; country=""; role=""
        count=0
        while IFS= read -r line; do
            if [ $count -eq 0 ]; then
                name+="$line"
            elif [ $count -eq 1 ]; then
                country+="$line"
            elif [ $count -eq 3 ]; then
                role+="$line"
            fi
            # echo "$line"
            # echo "$count"
            ((count++))
        done < <(findLine "$file")
        # echo "$name $country $role"
        mkdir -p "$country/$role"
        # mv "$file" "$country/$role"
    done < <(find "$folder" -type f -name "*.txt")
    find . -type d -empty -delete
done
# Map filename to file descriptor
declare -A file_to_fd
# Map file descriptor to filename
declare -A fd_to_file

# Read number of operations
read Q
Q=$((Q - 1))
# Process each operation
for ((i = 0; i < Q; i++)); do
    read cmd arg1 arg2

    if [[ "$cmd" == "open" ]]; then
        filename="$arg1"
        if [[ -v file_to_fd["$filename"] ]]; then
            continue  # already open
        fi
        # Create file if it doesn't exist
        [[ -f "$filename" ]] || touch "$filename"

        # Find smallest unused fd
        fd=0
        while [[ -v fd_to_file[$fd] ]]; do
            ((fd++))
        done
        file_to_fd["$filename"]=$fd
        fd_to_file[$fd]="$filename"

    elif [[ "$cmd" == "close" ]]; then
        filename="$arg1"
        if [[ -v file_to_fd["$filename"] ]]; then
            fd=${file_to_fd["$filename"]}
            unset file_to_fd["$filename"]
            unset fd_to_file["$fd"]
        fi

    elif [[ "$cmd" == "append" ]]; then
        fd="$arg1"
        word="$arg2"
        if [[ -v fd_to_file[$fd] ]]; then
            filename="${fd_to_file[$fd]}"
            echo "$word" >> "$filename"
        fi
    fi
done

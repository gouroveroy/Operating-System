if [[ $# -ne 1 ]]; then
    echo "file not provided"
    exit 1
fi

if [ ! -f "$1" ]; then
    echo "file not exists"
    exit 1
fi

while IFS= read -r line; do
    dept=$(echo "$line" | cut -d' ' -f1)
    courseId=$(echo "$line" | cut -d' ' -f2)
    termId=$(echo "$line" | cut -d' ' -f3)
    
    mainOrLab=$(echo "$courseId" | cut -c3)

    level=$(echo "$courseId" | cut -c1)
    term=$(echo "$termId" | cut -d'-' -f2)

    subdir="L$level"
    subdir+="T$term"
    mkdir -p "Academic Materials/$subdir"

    num=$((mainOrLab))

    if (( num % 2 == 0 )); then
        mkdir -p "Academic Materials/$subdir/LABS/$dept $courseId"
    else
        mkdir -p "Academic Materials/$subdir/$dept $courseId"
    fi
    
done < "$1"